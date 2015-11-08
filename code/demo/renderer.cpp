#include "renderer.h"
#include <util/debug.h>
#include <util/common.h>
#include <util/thread/mutex.h>
#include <util/id_table.h>
#include <util/array.h>
#include <util/array_util.h>
#include <util/id_array.h>
#include <util/pool_allocator.h>
#include <util/buffer_utils.h>
#include <util/random.h>
#include <util/time.h>
#include <util/handle_manager.h>
#include <util/float16.h>

#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <gdi/gdi_context.h>
#include <gdi/gdi_sort_list.h>

#include <gfx/gfx_type.h>

#include <algorithm>

#include "gfx_private.h"
#include "gfx/gfx_debug_draw.h"
namespace bx
{
    float gfxCameraAspect( const GfxCamera* cam )
    {
        const GfxCameraParams& p = cam->params;
        return (abs( p.vAperture ) < 0.0001f) ? p.hAperture : p.hAperture / p.vAperture;
    }
    float gfxCameraFov( const GfxCamera* cam )
    {
        return 2.f * atan( ( 0.5f * cam->params.hAperture ) / ( cam->params.focalLength * 0.03937f ) );
    }
    Vector3 gfxCameraEye( const GfxCamera* cam )
    {
        return cam->world.getTranslation();
    }
    Vector3 gfxCameraDir( const GfxCamera* cam )
    {
        return -cam->world.getCol2().getXYZ();
    }

    void gfxCameraViewport( GfxViewport* vp, const GfxCamera* cam, int dstWidth, int dstHeight, int srcWidth, int srcHeight )
    {
        const int windowWidth = dstWidth;
        const int windowHeight = dstHeight;

        const float aspectRT = (float)srcWidth / (float)srcHeight;
        const float aspectCamera = gfxCameraAspect( cam );

        int imageWidth;
        int imageHeight;
        int offsetX = 0, offsetY = 0;


        if( aspectCamera > aspectRT )
        {
            imageWidth = windowWidth;
            imageHeight = (int)( windowWidth / aspectCamera + 0.0001f );
            offsetY = windowHeight - imageHeight;
            offsetY = offsetY / 2;
        }
        else
        {
            float aspect_window = (float)windowWidth / (float)windowHeight;
            if( aspect_window <= aspectRT )
            {
                imageWidth = windowWidth;
                imageHeight = (int)( windowWidth / aspectRT + 0.0001f );
                offsetY = windowHeight - imageHeight;
                offsetY = offsetY / 2;
            }
            else
            {
                imageWidth = (int)( windowHeight * aspectRT + 0.0001f );
                imageHeight = windowHeight;
                offsetX = windowWidth - imageWidth;
                offsetX = offsetX / 2;
            }
        }
        vp[0] = GfxViewport( offsetX, offsetY, imageWidth, imageHeight );
    }

    void gfxCameraComputeMatrices( GfxCamera* cam )
    {
        const float fov = gfxCameraFov( cam );
        const float aspect = gfxCameraAspect( cam );

        cam->view = inverse( cam->world );
        cam->proj = Matrix4::perspective( fov, aspect, cam->params.zNear, cam->params.zFar );
        cam->viewProj = cam->proj * cam->view;
    }


    void gfxCameraWorldMatrixSet( GfxCamera* cam, const Matrix4& world )
    {
        cam->world = world;
    }
    Matrix4 gfxCameraWorldMatrixGet( const GfxCamera* camera )
    {
        return camera->world;
    }

    void gfxViewFrameParamsFill( GfxViewFrameParams* fparams, const GfxCamera* camera, int rtWidth, int rtHeight )
    {
        //SYS_STATIC_ASSERT( sizeof( FrameData ) == 376 );

        const Matrix4 sc = Matrix4::scale( Vector3( 1, 1, 0.5f ) );
        const Matrix4 tr = Matrix4::translation( Vector3( 0, 0, 1 ) );
        const Matrix4 proj = sc * tr * camera->proj;

        fparams->_camera_view = camera->view;
        fparams->_camera_proj = proj;
        fparams->_camera_viewProj = proj * camera->view;
        fparams->_camera_world = camera->world;

        const float fov = gfxCameraFov( camera );
        const float aspect = gfxCameraAspect( camera );

        fparams->_camera_fov = fov;
        fparams->_camera_aspect = aspect;

        const float zNear = camera->params.zNear;
        const float zFar  = camera->params.zFar;
        fparams->_camera_zNear = zNear;
        fparams->_camera_zFar = zFar;
        fparams->_reprojectDepthScale = ( zFar - zNear ) / ( -zFar * zNear );
        fparams->_reprojectDepthBias = zFar / ( zFar * zNear );

        fparams->_renderTarget_rcp = float2_t( 1.f / (float)rtWidth, 1.f / (float)rtHeight );
        fparams->_renderTarget_size = float2_t( (float)rtWidth, (float)rtHeight );

        {
            const float m11 = proj.getElem( 0, 0 ).getAsFloat();
            const float m22 = proj.getElem( 1, 1 ).getAsFloat();
            const float m33 = proj.getElem( 2, 2 ).getAsFloat();
            const float m44 = proj.getElem( 3, 2 ).getAsFloat();

            const float m13 = proj.getElem( 0, 2 ).getAsFloat();
            const float m23 = proj.getElem( 1, 2 ).getAsFloat();

            fparams->_reprojectInfo = float4_t( 1.f / m11, 1.f / m22, m33, -m44 );
            fparams->_reprojectInfoFromInt = float4_t(
                ( -fparams->_reprojectInfo.x * 2.f ) * fparams->_renderTarget_rcp.x,
                ( -fparams->_reprojectInfo.y * 2.f ) * fparams->_renderTarget_rcp.y,
                fparams->_reprojectInfo.x,
                fparams->_reprojectInfo.y
                );
        }
        m128_to_xyzw( fparams->_camera_eyePos.xyzw, Vector4( gfxCameraEye( camera ), oneVec ).get128() );
        m128_to_xyzw( fparams->_camera_viewDir.xyzw, Vector4( gfxCameraDir( camera ), zeroVec ).get128() );
    }

    ////
    //
    void gfxContextStartup( GfxContext** gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxContext* g = BX_NEW( bxDefaultAllocator(), GfxContext );

        gfxViewCreate( &g->_cmdQueue._view, dev, 1024*4 );
                
        gfxLightsCreate( &g->_lights, dev );
        gfxShadowCreate( &g->_shadow, dev, 1024 * 4 );
        gfxToneMapCreate( &g->_toneMap, dev, resourceManager );

        const int fbWidth = 1920;
        const int fbHeight = 1080;
        g->_framebuffer[eFB_COLOR0] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        g->_framebuffer[eFB_ALBEDO] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        g->_framebuffer[eFB_SAO]    = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        g->_framebuffer[eFB_SHADOW] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        g->_framebuffer[eFB_DEPTH]  = dev->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );

        g->_framebuffer[eFB_TEMP0] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        g->_framebuffer[eFB_TEMP1] = dev->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );

        {
            g->_fxISky = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "sky" );
            g->_fxISao = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "sao" );
            g->_fxIShadow = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "shadow" );
        }

        {

            DynamicPoolAllocatorThreadSafe* dpoolAlloc = BX_NEW( bxDefaultAllocator(), DynamicPoolAllocatorThreadSafe );
            dpoolAlloc->startup( sizeof( GfxMeshInstance ), 64, bxDefaultAllocator(), 16 );
            g->_allocMesh = dpoolAlloc;
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = BX_NEW( bxDefaultAllocator(), DynamicPoolAllocatorThreadSafe );
            dpoolAlloc->startup( sizeof( GfxCamera ), 16, bxDefaultAllocator(), 16 );
            g->_allocCamera = dpoolAlloc;
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = BX_NEW( bxDefaultAllocator(), DynamicPoolAllocatorThreadSafe );
            dpoolAlloc->startup( sizeof( Matrix4 ), 64, bxDefaultAllocator(), 16 );
            g->_allocIDataSingle = dpoolAlloc;
        }
        {
            g->_allocScene = bxDefaultAllocator();
            g->_allocIDataMulti = bxDefaultAllocator();
        }

        gfxGlobalResourcesStartup( &g->_globalResources, dev, resourceManager );
        gfxMaterialManagerStartup( &g->_materialManager, dev, resourceManager );

        gfxSunLightCreate( &g->_sunLight, g );
        gfxSunLightDirectionSet( g->_sunLight, Vector3( 1.0f, -1.f,-1.f ) );

        gfx[0] = g;
    }

    void gfxContextShutdown( GfxContext** gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxContext* g = gfx[0];
        gfxSunLightDestroy( &g->_sunLight );

        /// implicit tick. 
        gfxContextTick( g, dev, resourceManager );

        gfxMaterialManagerShutdown( &g->_materialManager, dev, resourceManager );
        gfxGlobalResourcesShutdown( &g->_globalResources, dev, resourceManager );

        {
            g->_allocIDataMulti = nullptr;
            g->_allocScene = nullptr;
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = (DynamicPoolAllocatorThreadSafe*)g->_allocIDataSingle;
            dpoolAlloc->shutdown();
            BX_DELETE0( bxDefaultAllocator(), g->_allocIDataSingle );
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = (DynamicPoolAllocatorThreadSafe*)g->_allocCamera;
            dpoolAlloc->shutdown();
            BX_DELETE0( bxDefaultAllocator(), g->_allocCamera );
        }
        {
            DynamicPoolAllocatorThreadSafe* dpoolAlloc = (DynamicPoolAllocatorThreadSafe*)g->_allocMesh;
            dpoolAlloc->shutdown();
            BX_DELETE0( bxDefaultAllocator(), g->_allocMesh );
        }
        
        {
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &g->_fxIShadow );
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &g->_fxISao );
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &g->_fxISky );
        }

        for ( int i = 0; i < eFB_COUNT; ++i )
            dev->releaseTexture( &g->_framebuffer[i] );

        gfxToneMapDestroy( &g->_toneMap, dev, resourceManager );
        gfxShadowDestroy( &g->_shadow, dev );
        gfxLightsDestroy( &g->_lights, dev );
        gfxViewDestroy( &g->_cmdQueue._view, dev );

        BX_DELETE0( bxDefaultAllocator(), gfx[0] );
    }

    void gfxContextTick( GfxContext* gfx, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        gfx->_lockActorsToRelease.lock();

        for( int i = 0; i < array::size( gfx->_actorsToRelease ); ++i )
        {
            GfxActor* actor = gfx->_actorsToRelease[i];

            if( actor->isScene() )
            {
                GfxScene* scene = actor->isScene();
                for( int j = 0; j < scene->_data.size; ++j )
                {
                    u32 meshIhandle = scene->_data.meshHandle[j];
                    GfxActor* meshActor = nullptr;
                    if( gfx->_handles.get( ActorHandleManager::Handle( meshIhandle ), &meshActor ) != -1 )
                    {
                        gfxContextHandleRemove( gfx, meshIhandle );
                        array::push_back( gfx->_actorsToRelease, meshActor );
                    }
                }

                bxGdi::sortList_delete( &scene->_sListColor );
                bxGdi::sortList_delete( &scene->_sListDepth );

                gfxSceneDataFree( &scene->_data );
                BX_DELETE0( gfx->_allocScene, scene );
            }
            else if( actor->isMeshInstance() )
            {
                GfxMeshInstance* meshI = actor->isMeshInstance();
                
                bxAllocator* allocIdata = (meshI->_idata.count == 1) ? gfx->_allocIDataSingle : gfx->_allocIDataMulti;
                allocIdata->free( meshI->_idata.pose );

                BX_DELETE0( gfx->_allocMesh, meshI );
            }
            else if( actor->isCamera() )
            {
                GfxCamera* camera = actor->isCamera();
                BX_DELETE0( gfx->_allocCamera, camera );
            }
            else if( actor->isSunLight() )
            {
                GfxSunLight* sunLight = actor->isSunLight();
                BX_DELETE0( bxDefaultAllocator(), sunLight );
            }
            else
            {
                SYS_ASSERT( false );
            }

        }
        array::clear( gfx->_actorsToRelease );
        gfx->_lockActorsToRelease.unlock();
    }

    void gfxContextFrameBegin( GfxContext* gfx, bxGdiContext* gdi )
    {
        (void)gfx;
        gdi->clear();
    }

    void gfxContextFrameEnd( GfxContext* gfx, bxGdiContext* gdi )
    {
        (void)gfx;
        gdi->backend()->swap();
    }

    void gfxCommandQueueAcquire( GfxCommandQueue** cmdq, GfxContext* ctx, bxGdiContext* gdiContext )
    {
        GfxCommandQueue* cmdQueue = &ctx->_cmdQueue;
        SYS_ASSERT( cmdQueue->_acquireCounter == 0 );
        ++cmdQueue->_acquireCounter;
        cmdQueue->_ctx = ctx;
        cmdQueue->_gdiContext = gdiContext;
        cmdq[0] = cmdQueue;
    }
    void gfxCommandQueueRelease( GfxCommandQueue** cmdq )
    {
        cmdq[0]->_gdiContext = nullptr;
        cmdq[0]->_ctx = nullptr;
        SYS_ASSERT( cmdq[0]->_acquireCounter == 1 );
        --cmdq[0]->_acquireCounter;
        cmdq[0] = nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    ///
    void gfxSubmitFullScreenQuad( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName )
    {
        GfxGlobalResources* gr = gfxGlobalResourcesGet();
        bxGdi::renderSource_enable( ctx, gr->mesh.fullScreenQuad );
        bxGdi::shaderFx_enable( ctx, fxI, passName );
        ctx->setTopology( bxGdi::eTRIANGLES );
        ctx->draw( gr->mesh.fullScreenQuad->vertexBuffers->numElements, 0 );
    }

    void gfxCopyTextureRGBA( bxGdiContext* ctx, const bxGdiTexture& outputTexture, const bxGdiTexture& inputTexture )
    {
        GfxGlobalResources* gr = gfxGlobalResourcesGet();
        ctx->changeRenderTargets( (bxGdiTexture*)&outputTexture, 1 );
        bxGdi::context_setViewport( ctx, outputTexture );
        gfxSubmitFullScreenQuad( ctx, gr->fx.texUtils, "copy_rgba" );
    }

    void gfxRasterizeFramebuffer( bxGdiContext* ctx, const bxGdiTexture& colorFB, float cameraAspect )
    {
        ctx->changeToMainFramebuffer();

        bxGdiTexture colorTexture = colorFB; // _framebuffer[bxGfx::eFRAMEBUFFER_COLOR];
        bxGdiTexture backBuffer = ctx->backend()->backBufferTexture();
        GfxViewport viewport = bx::gfx::computeViewport( cameraAspect, backBuffer.width, backBuffer.height, colorTexture.width, colorTexture.height );

        ctx->setViewport( viewport );
        ctx->clearBuffers( 0.f, 0.f, 0.f, 1.f, 1.f, 1, 0 );

        GfxGlobalResources* gr = gfxGlobalResourcesGet();
        bxGdiShaderFx_Instance* fxI = gr->fx.texUtils;
        fxI->setTexture( "gtexture", colorTexture );
        fxI->setSampler( "gsampler", bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR ) );

        gfxSubmitFullScreenQuad( ctx, fxI, "copy_rgba" );
    }

    int gfxLoadTextureFromFile( bxGdiTexture* tex, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* filename )
    {
        int ierr = 0;
        bxFS::File file = resourceManager->readFileSync( filename );
        if ( file.ok() )
        {
            tex[0] = dev->createTexture( file.bin, file.size );
        }
        else
        {
            ierr = -1;
        }
        file.release();

        return ierr;
    }

    GfxGlobalResources* gfxGlobalResourcesGet()
    {
        return GfxContext::_globalResources;
    }
    bxGdiShaderFx_Instance* gfxMaterialFind( const char* name )
    {
        const u64 key = gfxMaterialManagerCreateNameHash( name );
        hashmap_t::cell_t* cell = hashmap::lookup( GfxContext::_materialManager->_map, key );

        return (cell) ? (bxGdiShaderFx_Instance*)cell->value : 0;
    }

    GfxContext* gfxContextGet( GfxScene* scene )
    {
        return scene->_ctx;
    }

    //////////////////////////////////////////////////////////////////////////
    ///
    void gfxCameraCreate( GfxCamera** camera, GfxContext* ctx )
    {
        GfxCamera* c = BX_NEW( ctx->_allocCamera, GfxCamera );
        c->_ctx = ctx;
        c->_internalHandle = gfxContextHandleAdd( ctx, c );

        camera[0] = c;
    }

    void gfxCameraDestroy( GfxCamera** camera )
    {
        GfxCamera* c = camera[0];

        if( !c )
            return;

        if( !gfxContextHandleValid( c->_ctx, c->_internalHandle ) )
            return;

        gfxContextHandleRemove( c->_ctx, c->_internalHandle );
        gfxContextActorRelease( c->_ctx, c );

        camera[0] = nullptr;
    }
    GfxCameraParams gfxCameraParamsGet( const GfxCamera* camera )
    {
        return camera->params;
    }

    void gfxCameraParamsSet( GfxCamera* camera, const GfxCameraParams& params )
    {
        camera->params = params;
    }

    //////////////////////////////////////////////////////////////////////////
    void gfxSunLightDirectionSet( GfxContext* ctx, const Vector3& direction )
    {
        if( ctx->_sunLight )
        {
            gfxSunLightDirectionSet( ctx->_sunLight, direction );
        }
    }


    //////////////////////////////////////////////////////////////////////////
    void gfxMeshInstanceCreate( GfxMeshInstance** meshI, GfxContext* ctx, int numInstances )
    {
        GfxMeshInstance* m = BX_NEW( ctx->_allocMesh, GfxMeshInstance );
        m->_ctx = ctx;
        m->_internalHandle = gfxContextHandleAdd( ctx, m );
        
        bxAllocator* allocInstance = ( numInstances == 1 ) ? ctx->_allocIDataSingle : ctx->_allocIDataMulti;
        m->_idata.pose = (Matrix4*)allocInstance->alloc( numInstances * sizeof( Matrix4 ), 16 );
        m->_idata.count = numInstances;

        meshI[0] = m;
    }

    void gfxMeshInstanceDestroy( GfxMeshInstance** meshI )
    {
        GfxMeshInstance* m = meshI[0];
        if( !m )
            return;

        if( !gfxContextHandleValid( m->_ctx, m->_internalHandle ) )
            return;

        if( m->_scene )
        {
            gfxSceneMeshInstanceRemove( m->_scene, m );
        }
        gfxContextHandleRemove( m->_ctx, m->_internalHandle );
        gfxContextActorRelease( m->_ctx, m );

        meshI[0] = nullptr;
    }
    bxGdiRenderSource* gfxMeshInstanceRenderSourceGet( GfxMeshInstance* meshI )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );
        return meshI->_rsource;
    }
    bxGdiShaderFx_Instance* gfxMeshInstanceFxGet( GfxMeshInstance* meshI )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );
        return meshI->_fxI;
    }
    void gfxMeshInstanceDataSet( GfxMeshInstance* meshI, const GfxMeshInstanceData& data )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );

        if( data.mask & GfxMeshInstanceData::eMASK_RENDER_SOURCE )
        {
            meshI->_rsource = data.rsource;
        }
        if( data.mask & GfxMeshInstanceData::eMASK_SHADER_FX )
        {
            meshI->_fxI = data.fxInstance;
        }
        if( data.mask & GfxMeshInstanceData::eMASK_LOCAL_AABB )
        {
            memcpy( &meshI->_localAABB[0], &data.localAABB[0], 6 * sizeof( f32 ) );
        }

        if( data.mask && meshI->_scene )
        {
            gfxSceneDataRefresh( meshI->_scene, meshI->_internalHandle );
        }
    }

    void gfxMeshInstanceWorldMatrixSet( GfxMeshInstance* meshI, const Matrix4* matrices, int nMatrices )
    {
        SYS_ASSERT( gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) );
        SYS_ASSERT( nMatrices > 0 );

        int numToSet = minOfPair( nMatrices, meshI->_idata.count );
        memcpy( meshI->_idata.pose, matrices, numToSet * sizeof( Matrix4 ) );
    }

    //////////////////////////////////////////////////////////////////////////
    void gfxSceneCreate( GfxScene** scene, GfxContext* ctx )
    {
        GfxScene* s = BX_NEW( ctx->_allocScene, GfxScene );
        s->_ctx = ctx;
        s->_internalHandle = gfxContextHandleAdd( ctx, s );

        scene[0] = s;
    }
    void gfxSceneDestroy( GfxScene** scene )
    {
        GfxScene* s = scene[0];
        if( !s )
            return;

        if( !gfxContextHandleValid( s->_ctx, s->_internalHandle ) )
            return;

        gfxContextHandleRemove( s->_ctx, s->_internalHandle );
        gfxContextActorRelease( s->_ctx, s );
    }

    void gfxSceneMeshInstanceAdd( GfxScene* scene, GfxMeshInstance* meshI )
    {
        if ( !gfxContextHandleValid( scene->_ctx, scene->_internalHandle ) )
            return;
        
        if ( !gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) )
            return;

        GfxScene::Cmd cmd;
        cmd.handle = meshI->_internalHandle;
        cmd.op = GfxScene::Cmd::eOP_ADD;

        scene->_lockCmd.lock();
        array::push_back( scene->_cmd, cmd );
        scene->_lockCmd.unlock();
    }
    void gfxSceneMeshInstanceRemove( GfxScene* scene, GfxMeshInstance* meshI )
    {
        if ( !gfxContextHandleValid( scene->_ctx, scene->_internalHandle ) )
            return;

        if ( !gfxContextHandleValid( meshI->_ctx, meshI->_internalHandle ) )
            return;

        GfxScene::Cmd cmd;
        cmd.handle = meshI->_internalHandle;
        cmd.op = GfxScene::Cmd::eOP_REMOVE;

        scene->_lockCmd.lock();
        array::push_back( scene->_cmd, cmd );
        scene->_lockCmd.unlock();

        meshI->_scene = nullptr;
    }
    
    namespace 
    {
        inline GfxMeshInstance* _MeshInstanceFromHandle( GfxContext* ctx, u32 handle )
        {
            GfxActor* actor = nullptr;
            if ( gfxContextHandleActorGet_noLock( &actor, ctx, handle ) != -1 )
            {
                return actor->isMeshInstance();
            }
            return nullptr;
        }

        void sceneBuildSortListColorDepth( bxChunk* colorChunk, bxChunk* depthChunk, GfxScene* scene, const GfxCamera* camera )
        {
            const GfxScene::Data& data = scene->_data;

            if ( !scene->_sListColor || scene->_sListColor->capacity < scene->_instancesCount )
            {
                bxGdi::sortList_delete( &scene->_sListColor );
                bxGdi::sortList_delete( &scene->_sListDepth );
                bxGdi::sortList_new( &scene->_sListColor, scene->_instancesCount, bxDefaultAllocator() );
                bxGdi::sortList_new( &scene->_sListDepth, scene->_instancesCount, bxDefaultAllocator() );
            }

            GfxSortListColor* colorList = scene->_sListColor;
            GfxSortListDepth* depthList = scene->_sListDepth;

            //bxChunk colorChunk, depthChunk;
            bxChunk_create( colorChunk, 1, data.size );
            bxChunk_create( depthChunk, 1, data.size );

            GfxViewFrustum frustum = bx::gfx::viewFrustumExtract( camera->viewProj );

            bxAABB sceneAABB = bxAABB::prepare();

            const int n = data.size;
            for ( int iitem = colorChunk->begin; iitem < colorChunk->end; ++iitem )
            {
                const GfxInstanceData& idata = data.idata[iitem];
                const bxAABB& localAABB = data.bbox[iitem];

                for ( int ii = 0; ii < idata.count; ++ii )
                {
                    const Matrix4& world = idata.pose[ii];
                    bxAABB worldAABB = bxAABB::transform( world, localAABB );

                    sceneAABB = bxAABB::merge( sceneAABB, worldAABB );

                    bool inFrustum = bx::gfx::viewFrustumAABBIntersect( frustum, worldAABB.min, worldAABB.max ).getAsBool();
                    if ( !inFrustum )
                        continue;

                    u32 hashMesh = data.rsource[iitem]->sortHash;
                    u16 hashMesh16 = u16( hashMesh >> 16 ) ^ u16( hashMesh & 0xFFFF );
                    u32 hashShader = data.fxInstance[iitem]->sortHash( 0 );

                    float depth = bx::gfx::cameraDepth( camera->world, world.getTranslation() ).getAsFloat();
                    u16 depth16 = float_to_half_fast3( fromF32( depth ) ).u;

                    GfxSortItemColor colorSortItem;
                    GfxSortKeyColor& colorSortKey = colorSortItem.key;
                    colorSortKey.instance = ii;
                    colorSortKey.mesh = hashMesh16;
                    colorSortKey.shader = hashShader;
                    colorSortKey.layer = 8;

                    GfxSortItemDepth depthSortItem;
                    GfxSortKeyDepth& depthSortKey = depthSortItem.key;
                    depthSortKey.depth = depth16;

                    colorSortItem.index = iitem;
                    depthSortItem.index = iitem;

                    bxGdi::sortList_chunkAdd( colorList, colorChunk, colorSortItem );
                    bxGdi::sortList_chunkAdd( depthList, depthChunk, depthSortItem );
                }
            }
            scene->_aabb = sceneAABB;


            bxGdi::sortList_sortLess( colorList, *colorChunk );
            bxGdi::sortList_sortLess( depthList, *depthChunk );
        }

        

        void sortListColorSubmit( bxGdiContext* gdi, const GfxView& view, const GfxScene* scene, const GfxSortListColor* sList, int begin, int end )
        {
            const GfxScene::Data& data = scene->_data;
            for ( int i = begin; i < end; ++i )
            {
                const GfxSortItemColor& item = sList->items[i];
                int meshDataIndex = item.index;

                bxGdiRenderSource* rsource = data.rsource[meshDataIndex];
                bxGdiShaderFx_Instance* fxI = data.fxInstance[meshDataIndex];

                u32 instanceOffset = view._instanceOffsetArray[i];
                u32 instanceCount = view._instanceOffsetArray[i + 1] - instanceOffset;
                gdi->backend()->updateCBuffer( view._instanceOffsetBuffer, &instanceOffset );

                bxGdi::renderSource_enable( gdi, rsource );
                bxGdi::shaderFx_enable( gdi, fxI, 0 );

                bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
                bxGdi::renderSurface_drawIndexedInstanced( gdi, surf, instanceCount );
            }
        }
        void sortListDepthSubmit( bxGdiContext* gdi, const GfxView& view, const GfxScene* scene, const GfxSortListDepth* sList, int begin, int end )
        {
            const GfxScene::Data& data = scene->_data;
            for( int i = begin; i < end; ++i )
            {
                const GfxSortListDepth::ItemType& item = sList->items[i];
                int meshDataIndex = item.index;

                bxGdiRenderSource* rsource = data.rsource[meshDataIndex];

                u32 instanceOffset = view._instanceOffsetArray[i];
                u32 instanceCount = view._instanceOffsetArray[i + 1] - instanceOffset;
                gdi->backend()->updateCBuffer( view._instanceOffsetBuffer, &instanceOffset );

                bxGdi::renderSource_enable( gdi, rsource );

                bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
                bxGdi::renderSurface_drawIndexedInstanced( gdi, surf, instanceCount );
            }
        }
    }///

    void gfxSceneDraw( GfxScene* scene, GfxCommandQueue* cmdq, const GfxCamera* camera )
    {
        GfxContext* ctx = scene->_ctx;
        bxScopeRecursiveBenaphore handlesLock( ctx->_lockHandles );
        {
            bxScopeRecursiveBenaphore lock( scene->_lockCmd );
            int nCmd = array::size( scene->_cmd );
            for( int i = 0; i < nCmd; ++i )
            {
                const GfxScene::Cmd cmd = scene->_cmd[i];

                switch( cmd.op )
                {
                case GfxScene::Cmd::eOP_ADD:
                    {
                        GfxMeshInstance* meshInstance = _MeshInstanceFromHandle( ctx, cmd.handle );
                        if( meshInstance )
                        {
                            gfxSceneDataAdd( &scene->_data, meshInstance );
                        }
                    }break;
                case GfxScene::Cmd::eOP_REMOVE:
                    {
                        GfxMeshInstance* meshInstance = _MeshInstanceFromHandle( ctx, cmd.handle );
                        if ( meshInstance )
                        {
                            int index = gfxSceneDataFind( scene->_data, cmd.handle );
                            gfxSceneDataRemove( &scene->_data, index );

                            
                        }
                    }break;
                case GfxScene::Cmd::eOP_REFRESH:
                    {
                        GfxMeshInstance* meshInstance = _MeshInstanceFromHandle( ctx, cmd.handle );
                        if ( meshInstance )
                        {
                            int index = gfxSceneDataFind( scene->_data, cmd.handle );
                            gfxSceneDataRefresh( &scene->_data, ctx, index );
                        }
                    }break;
                }//
            }
            
            array::clear( scene->_cmd );

            if( nCmd )
            {
                scene->_instancesCount = 0;
                for( int i = 0; i < scene->_data.size; ++i )
                {
                    const GfxInstanceData& idata = scene->_data.idata[i];
                    scene->_instancesCount += idata.count;
                }
            }
        }

        bxGdiContext* gdi = cmdq->_gdiContext;
        

        const GfxScene::Data& data = scene->_data;
        int nMeshes = scene->_data.size;
        if( !nMeshes )
            return;

        bxChunk colorChunk, depthChunk;
        sceneBuildSortListColorDepth( &colorChunk, &depthChunk, scene, camera );

        GfxSortListColor* colorList = scene->_sListColor;
        GfxSortListDepth* depthList = scene->_sListDepth;

        GfxView& view = cmdq->_view;
        
        gfxViewCameraSet( gdi, &view, camera, ctx->_framebuffer->width, ctx->_framebuffer->height );
        gfxViewEnable( gdi, &view );

        /// depth prepass
        {
            gfxViewUploadInstanceData( gdi, &view, scene->_data, depthList, depthChunk.begin, depthChunk.current );
            gdi->changeRenderTargets( nullptr, 0, ctx->_framebuffer[eFB_DEPTH] );
            gdi->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );

            bxGdiShaderFx_Instance* fxI = gfxGlobalResourcesGet()->fx.utils;
            bxGdi::shaderFx_enable( gdi, fxI, "zPrepassDepthOnly" );
            sortListDepthSubmit( gdi, view, scene, depthList, depthChunk.begin, depthChunk.current );
        }

        /// shadow
        {
            gfxShadowDraw( cmdq, &ctx->_shadow, scene, camera, gfxSunLightDirectionGet( ctx->_sunLight ) );
            gfxShadowResolve( cmdq, ctx->_framebuffer[eFB_SHADOW], ctx->_framebuffer[eFB_DEPTH], &ctx->_shadow, camera );
        }

        /// sao
        {
            bxGdiTexture saoTexture = ctx->_framebuffer[eFB_SAO];
            bxGdiTexture tmp0Texture = ctx->_framebuffer[eFB_TEMP0];
            bxGdiTexture tmp1Texture = ctx->_framebuffer[eFB_TEMP1];

            gdi->changeRenderTargets( &tmp0Texture, 1 );
            gdi->clearBuffers( 0.f, 0.f, 0.f, 1.f, 0.f, 1, 0 );
            
            bxGdiShaderFx_Instance* fxI = ctx->_fxISao;
            fxI->setUniform( "_radius", 1.2f );
            fxI->setUniform( "_radius2", 1.2f * 1.2f );
            fxI->setUniform( "_bias", 0.025f );
            fxI->setUniform( "_intensity", 0.25f );
            fxI->setUniform( "_projScale", 700.f );
            fxI->setUniform( "_ssaoTexSize", float2_t( (float)saoTexture.width, (float)saoTexture.height ) );
            fxI->setTexture( "texHwDepth", ctx->_framebuffer[eFB_DEPTH] );
            gfxSubmitFullScreenQuad( gdi, fxI, "ssao" );

            gdi->changeRenderTargets( &tmp1Texture, 1 );
            gdi->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
            fxI->setTexture( "tex_source", tmp0Texture );
            gfxSubmitFullScreenQuad( gdi, fxI, "blurX" );

            gdi->changeRenderTargets( &saoTexture, 1 );
            gdi->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
            fxI->setTexture( "tex_source", tmp1Texture );
            gfxSubmitFullScreenQuad( gdi, fxI, "blurY" );

        }

        gdi->clear();
        gfxViewCameraSet( gdi, &view, camera, ctx->_framebuffer->width, ctx->_framebuffer->height );
        gfxViewEnable( gdi, &view );

        gfxLightsUploadData( gdi, &ctx->_lights, ctx->_sunLight );
        gfxLightsEnable( gdi, &ctx->_lights );

        /// sky
        {
            gdi->changeRenderTargets( &ctx->_framebuffer[eFB_COLOR0], 1 );
            gdi->clearBuffers( 0.f, 0.f, 0.f, 1.f, 0.f, 1, 0 );

            bxGdiShaderFx_Instance* fxI = ctx->_fxISky;
            gfxSubmitFullScreenQuad( gdi, fxI, "skyPreetham" );
        }

        /// color pass
        {
            gfxViewUploadInstanceData( gdi, &view, scene->_data, colorList, colorChunk.begin, colorChunk.current );

            gdi->setTexture( ctx->_framebuffer[eFB_SAO], eRS_TEXTURE_SAO, bxGdi::eSTAGE_MASK_PIXEL );
            gdi->setTexture( ctx->_framebuffer[eFB_SHADOW], eRS_TEXTURE_SHADOW, bxGdi::eSTAGE_MASK_PIXEL );
            gdi->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ), eRS_TEXTURE_SAO, bxGdi::eSTAGE_MASK_PIXEL );

            gdi->changeRenderTargets( &ctx->_framebuffer[eFB_COLOR0], 1, ctx->_framebuffer[eFB_DEPTH] );
            //gdi->clearBuffers( 0.f, 0.f, 0.f, 1.f, 0.f, 1, 0 );
            sortListColorSubmit( gdi, view, scene, colorList, colorChunk.begin, colorChunk.current );
        }

        {
            gfxToneMapDraw( cmdq, &ctx->_toneMap, ctx->_framebuffer[eFB_TEMP0], ctx->_framebuffer[eFB_COLOR0], 0.016f );
        }

        //{
        //    bxGdiTexture outputTex = ctx->_framebuffer[eFB_TEMP1];
        //    bxGdiTexture albedoTex = ctx->_framebuffer[eFB_ALBEDO];
        //    bxGdiTexture noiseTex = gfxGlobalResourcesGet()->texture.noise;


        //    gdi->changeRenderTargets( &outputTex, 1 );
        //    gdi->clearBuffers( 0.f, 0.f, 0.f, 1.f, 0.f, 1, 0 );
        //    
        //    bxGdiShaderFx_Instance* fxI = ctx->_fxISao;
        //    fxI->setTexture( "texAlbedo", albedoTex );
        //    fxI->setTexture( "texNoise", noiseTex );
        //    fxI->setSampler( "samplerAlbedo", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ) );
        //    fxI->setSampler( "samplerNoise", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ) );
        //    gfxSubmitFullScreenQuad( gdi, fxI, "ambientTransfer" );

        //}

        {
            gdi->changeRenderTargets( &ctx->_framebuffer[eFB_TEMP0], 1, ctx->_framebuffer[eFB_DEPTH] );
            bxGfxDebugDraw::flush( gdi, camera->viewProj );
        }


        gfxRasterizeFramebuffer( gdi, ctx->_framebuffer[eFB_TEMP0], gfxCameraAspect( camera ) );
        //gfxRasterizeFramebuffer( gdi, ctx->_shadow._texDepth, gfxCameraAspect( camera ) );
        
    }

    

    



}///












