#include "gfx_private.h"
#include <util/buffer_utils.h>
#include <util/hashmap.h>
#include <util/hash.h>
#include "util/float16.h"

#include <gdi/gdi_context.h>
#include "gfx/gfx_debug_draw.h"

#include <algorithm>
#include <util/string_util.h>

namespace bx
{
    GfxMeshInstanceData::GfxMeshInstanceData()
        : rsource( nullptr )
        , fxInstance( nullptr )
        , mask( 0 )
    {
        localAABB[0] = localAABB[1] = localAABB[2] = -0.5f;
        localAABB[3] = localAABB[4] = localAABB[5] =  0.5f;
    }

    GfxMeshInstanceData& GfxMeshInstanceData::renderSourceSet( bxGdiRenderSource* rsrc )
    {
        rsource = rsrc;
        mask |= eMASK_RENDER_SOURCE;
        return *this;
    }
    GfxMeshInstanceData& GfxMeshInstanceData::fxInstanceSet( bxGdiShaderFx_Instance* fxI )
    {
        fxInstance = fxI;
        mask |= eMASK_SHADER_FX;
        return *this;
    }
    GfxMeshInstanceData& GfxMeshInstanceData::locaAABBSet( const Vector3& pmin, const Vector3& pmax )
    {
        storeXYZ( pmin, &localAABB[0] );
        storeXYZ( pmax, &localAABB[3] );
        mask |= eMASK_LOCAL_AABB;
        return *this;
    }

    //////////////////////////////////////////////////////////////////////////
    GfxCameraParams::GfxCameraParams()
        : hAperture( 1.8f )
        , vAperture( 1.f )
        , focalLength( 50.f )
        , zNear( 0.25f )
        , zFar( 250.f )
        , orthoWidth( 10.f )
        , orthoHeight( 10.f )
    {
    
    }
    GfxCamera::GfxCamera() 
        : world( Matrix4::identity() )
        , view( Matrix4::identity() )
        , proj( Matrix4::identity() )
        , viewProj( Matrix4::identity() )
        , _ctx( nullptr )
        , _internalHandle( 0 )
    {

    }

    //////////////////////////////////////////////////////////////////////////
    GfxSunLight::GfxSunLight()
        : _direction( 0.f, -1.f, 0.f )
        , _angularRadius()
        , _sunIlluminanceInLux( 110000.f )
        , _skyIlluminanceInLux( 30000.f )
        
        , _ctx( nullptr )
        , _internalHandle( 0 )
    {}
    
    //////////////////////////////////////////////////////////////////////////
    GfxScene::GfxScene()
        : _instancesCount( 0 )
        , _ctx( nullptr )
        , _internalHandle( 0 )
        , _sListColor( nullptr )
        , _sListDepth( nullptr )
    {
        _aabb = bxAABB( Vector3( 0.f ), Vector3( 0.f ) );
        memset( &_data, 0x00, sizeof( GfxScene::Data ) );
    }
    void gfxSceneDataAllocate( GfxScene::Data* data, int newsize, bxAllocator* alloc )
    {
        int memSize = 0;
        memSize += newsize * sizeof( *data->meshHandle );
        memSize += newsize * sizeof( *data->rsource);
        memSize += newsize * sizeof( *data->fxInstance );
        memSize += newsize * sizeof( *data->bbox );
        memSize += newsize * sizeof( *data->idata );

        void* mem = BX_MALLOC( alloc, memSize, 16 );
        memset( mem, 0x00, memSize );

        GfxScene::Data newdata;
        memset( &newdata, 0x00, sizeof( GfxScene::Data ) );
        newdata.alloc = alloc;
        newdata.memoryHandle = mem;
        newdata.size = data->size;
        newdata.capacity = newsize;

        bxBufferChunker chunker( mem, memSize );
        newdata.bbox = chunker.add< bxAABB >( newsize );
        newdata.rsource = chunker.add< bxGdiRenderSource* >( newsize );
        newdata.fxInstance = chunker.add< bxGdiShaderFx_Instance* >( newsize );
        newdata.idata = chunker.add< GfxInstanceData >( newsize );
        newdata.meshHandle = chunker.add< u32 >( newsize );
        chunker.check();

        if( data->size )
        {
            BX_CONTAINER_COPY_DATA( &newdata, data, meshHandle );
            BX_CONTAINER_COPY_DATA( &newdata, data, rsource );
            BX_CONTAINER_COPY_DATA( &newdata, data, fxInstance);
            BX_CONTAINER_COPY_DATA( &newdata, data, idata );
            BX_CONTAINER_COPY_DATA( &newdata, data, bbox );
        }

        gfxSceneDataFree( data );

        data[0] = newdata;

    }

    void gfxSceneDataFree( GfxScene::Data* data )
    {
        if ( !data->alloc )
            return;

        BX_FREE( data->alloc, data->memoryHandle );
        memset( data, 0x00, sizeof( GfxScene::Data ) );
    }

    int gfxSceneDataRefresh( GfxScene::Data* data, GfxContext* ctx, int index )
    {
        SYS_ASSERT( index >= 0 && index < data->size );

        u32 meshHandle = data->meshHandle[index];
        GfxActor* actor = nullptr;
        
        if ( gfxContextHandleActorGet_lock( &actor, ctx, meshHandle ) != 0 )
            return -1;

        GfxMeshInstance* meshInstance = actor->isMeshInstance();

        if ( !meshInstance )
            return -1;

        data->rsource[index] = meshInstance->_rsource;
        data->fxInstance[index] = meshInstance->_fxI;
        data->idata[index] = meshInstance->_idata;
        loadXYZ( data->bbox[index].min, &meshInstance->_localAABB[0] );
        loadXYZ( data->bbox[index].max, &meshInstance->_localAABB[3] );

        return 0;
    }

    void gfxSceneDataRefresh( GfxScene* scene, u32 meshHandle )
    {
        bxScopeRecursiveBenaphore lock( scene->_lockCmd );

        GfxScene::Cmd cmd;
        cmd.op = GfxScene::Cmd::eOP_REFRESH;
        cmd.handle = meshHandle;

        array::push_back( scene->_cmd, cmd );
    }

    int gfxSceneDataAdd( GfxScene::Data* data, GfxMeshInstance* meshInstance )
    {
        if( data->size >= data->capacity )
        {
            bxAllocator* allocator = ( data->alloc ) ? data->alloc : bxDefaultAllocator();
            gfxSceneDataAllocate( data, data->size * 2 + 8, allocator );
        }

        int index = data->size++;
        data->meshHandle[index] = meshInstance->_internalHandle;
        int ierr = gfxSceneDataRefresh( data, meshInstance->_ctx, index );
        SYS_ASSERT( ierr == 0 );
        return index;
    }

    void gfxSceneDataRemove( GfxScene::Data* data, int index )
    {
        if ( index < 0 || index >= data->size )
            return;

        int lastIndex = --data->size;
        data->meshHandle[index] = data->meshHandle[lastIndex];
        data->rsource[index]    = data->rsource[lastIndex];
        data->fxInstance[index] = data->fxInstance[lastIndex];
        data->idata[index]      = data->idata[lastIndex];
        data->bbox[index]       = data->bbox[lastIndex];
    }
    int gfxSceneDataFind( const GfxScene::Data& data, u32 meshHandle )
    {
        for( int i = 0; i < data.size; ++i )
        {
            if ( meshHandle == data.meshHandle[i] )
                return i;
        }
        return -1;
    }
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxMeshInstance::GfxMeshInstance() 
        : _ctx( nullptr )
        , _scene( nullptr )
        , _internalHandle( 0 )

        , _rsource( nullptr )
        , _fxI( nullptr )
    {
        memset( _localAABB, 0x00, sizeof( _localAABB ) );
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxView::GfxView()
        : _maxInstances( 0 )
    {}
    void gfxViewCreate( GfxView* view, bxGdiDeviceBackend* dev, int maxInstances )
    {
        view->_viewParamsBuffer = dev->createConstantBuffer( sizeof( GfxViewFrameParams ) );
        const int numElements = maxInstances * 3; /// 3 * row
        view->_instanceWorldBuffer = dev->createBuffer( numElements, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
        view->_instanceWorldITBuffer = dev->createBuffer( numElements, bxGdiFormat( bxGdi::eTYPE_FLOAT, 3 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
        view->_instanceOffsetBuffer = dev->createConstantBuffer( sizeof( u32 ) );
        view->_maxInstances = maxInstances;
    }

    void gfxViewDestroy( GfxView* view, bxGdiDeviceBackend* dev )
    {
        view->_maxInstances = 0;
        array::clear( view->_instanceOffsetArray );
        dev->releaseBuffer( &view->_instanceOffsetBuffer );
        dev->releaseBuffer( &view->_instanceWorldITBuffer );
        dev->releaseBuffer( &view->_instanceWorldBuffer );
        dev->releaseBuffer( &view->_viewParamsBuffer );
    }
    void gfxViewCameraSet( bxGdiContext* gdi, const GfxView* view, const GfxCamera* camera, int rtw, int rth )
    {
        GfxViewFrameParams viewParams;
        gfxViewFrameParamsFill( &viewParams, camera, rtw, rth );
        gdi->backend()->updateCBuffer( view->_viewParamsBuffer, &viewParams );
    }
    void gfxViewEnable( bxGdiContext* gdi, const GfxView* view )
    {
        gdi->setBufferRO( view->_instanceWorldBuffer, eRS_BUFFER_INSTANCE_WORLD, bxGdi::eSTAGE_MASK_VERTEX );
        gdi->setBufferRO( view->_instanceWorldITBuffer, eRS_BUFFER_INSTANCE_WORLD_IT, bxGdi::eSTAGE_MASK_VERTEX );
        gdi->setCbuffer( view->_viewParamsBuffer, eRS_CBUFFER_VIEW_PARAMS, bxGdi::eSTAGE_MASK_VERTEX | bxGdi::eSTAGE_MASK_PIXEL );
        gdi->setCbuffer( view->_instanceOffsetBuffer, eRS_CBUFFER_INSTANCE_OFFSET, bxGdi::eSTAGE_MASK_VERTEX );
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    void gfxLightningDataFill( GfxLighningData* ldata, const GfxLights* lights, const GfxSunLight* sunLight )
    {
        if( sunLight )
        {
            ldata->_sunAngularRadius = sunLight->_angularRadius;
            ldata->_sunIlluminanceInLux = sunLight->_sunIlluminanceInLux;
            ldata->_skyIlluminanceInLux = sunLight->_skyIlluminanceInLux;
            ldata->_sunDirection = sunLight->_direction;
        }
    }


    GfxLights::GfxLights()
    {

    }
    void gfxLightsCreate( GfxLights* lights, bxGdiDeviceBackend* dev )
    {
        lights->_lightnigParamsBuffer = dev->createConstantBuffer( sizeof( GfxLighningData ) );
    }

    void gfxLightsDestroy( GfxLights* lights, bxGdiDeviceBackend* dev )
    {
        dev->releaseBuffer( &lights->_lightnigParamsBuffer );
    }

    void gfxLightsUploadData( bxGdiContext* gdi, const GfxLights* lights, const GfxSunLight* sunLight )
    {
        GfxLighningData ldata;
        memset( &ldata, 0x00, sizeof( GfxLighningData ) );
        gfxLightningDataFill( &ldata, lights, sunLight );

        gdi->backend()->updateCBuffer( lights->_lightnigParamsBuffer, &ldata );
    }

    void gfxLightsEnable( bxGdiContext* gdi, const GfxLights* lights )
    {
        gdi->setCbuffer( lights->_lightnigParamsBuffer, eRS_CBUFFER_LIGHTS, bxGdi::eSTAGE_MASK_PIXEL );
    }
    void gfxSunLightCreate( GfxSunLight** sunLight, GfxContext* ctx )
    {
        GfxSunLight* s = BX_NEW( bxDefaultAllocator(), GfxSunLight );
        s->_internalHandle = gfxContextHandleAdd( ctx, s );
        s->_ctx = ctx;

        sunLight[0] = s;
    }

    void gfxSunLightDestroy( GfxSunLight** sunLight )
    {
        GfxSunLight* s = sunLight[0];

        if( !s )
            return;

        if( !gfxContextHandleValid( s->_ctx, s->_internalHandle ) )
            return;

        gfxContextHandleRemove( s->_ctx, s->_internalHandle );
        gfxContextActorRelease( s->_ctx, s );

        sunLight[0] = nullptr;
    }

    void gfxSunLightDirectionSet( GfxSunLight* sunLight, const Vector3& direction )
    {
        const Vector3 L = normalizeSafe( direction );
        m128_to_xyz( sunLight->_direction.xyz, L.get128() );
    }
    Vector3 gfxSunLightDirectionGet( GfxSunLight* sunLight )
    {
        return Vector3( xyz_to_m128( sunLight->_direction.xyz ) );
    }


    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxCommandQueue::GfxCommandQueue() : _acquireCounter( 0 )
        , _ctx( nullptr )
        , _gdiContext( nullptr )
    {
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    void gfxMaterialManagerStartup( GfxMaterialManager** materialManager, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* nativeShaderName /*= "native1" */ )
    {
        SYS_ASSERT( GfxContext::_materialManager == 0 );
        
        GfxMaterialManager* mm = BX_NEW( bxDefaultAllocator(), GfxMaterialManager );

        bxGdiShaderFx* nativeFx = bxGdi::shaderFx_createFromFile( dev, resourceManager, nativeShaderName );
        SYS_ASSERT( nativeFx != nullptr );
        mm->_nativeFx = nativeFx;

        {
            GfxMaterialManager::Material params;
            {
                params.diffuseColor = float3_t( 1.f, 0.f, 0.f );
                params.fresnelColor = float3_t( 0.045593921f );
                params.ambientColor = float3_t( 0.1f );
                params.diffuseCoeff = 0.29f;
                params.roughnessCoeff = 0.1f;
                params.specularCoeff = 0.9f;
                params.ambientCoeff = 0.3f;

                gfxMaterialManagerCreateMaterial( mm, dev, resourceManager, "red", params );
            }
            {
                params.diffuseColor = float3_t( 0.f, 1.f, 0.f );
                params.fresnelColor = float3_t( 0.171968833f );
                params.diffuseCoeff = 0.25f;
                params.roughnessCoeff = 0.5f;
                params.specularCoeff = 0.5f;
                gfxMaterialManagerCreateMaterial( mm, dev, resourceManager, "green", params );
            }
            {
                params.diffuseColor = float3_t( 0.f, 0.f, 1.f );
                params.fresnelColor = float3_t( 0.171968833f );
                params.diffuseCoeff = 0.16f;
                params.roughnessCoeff = 0.1f;
                params.specularCoeff = 0.25f;
                
                gfxMaterialManagerCreateMaterial( mm, dev, resourceManager, "blue", params );
            }
            {
                params.diffuseColor = float3_t( 1.f, 1.f, 1.f );
                params.diffuseCoeff = 1.0f;
                params.roughnessCoeff = 1.0f;
                params.specularCoeff = 0.1f;

                gfxMaterialManagerCreateMaterial( mm, dev, resourceManager, "white", params );
            }

            {
                params.diffuseColor = float3_t( 0.15f, 0.15f, 0.15f );
                params.diffuseCoeff = 0.9f;
                params.roughnessCoeff = 1.0f;
                params.specularCoeff = 0.01f;
                params.ambientColor = float3_t( 0.6f );

                gfxMaterialManagerCreateMaterial( mm, dev, resourceManager, "grey", params );
            }
        }

        materialManager[0] = mm;
    }

    void gfxMaterialManagerShutdown( GfxMaterialManager** materialManager, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxMaterialManager* mm = materialManager[0];
        SYS_ASSERT( mm != nullptr );
        
        hashmap::iterator it( mm->_map );
        while ( it.next() )
        {
            bxGdiShaderFx_Instance* fxI = (bxGdiShaderFx_Instance*)it->value;
            bxGdi::shaderFx_releaseInstance( dev, resourceManager, &fxI );
        }
        hashmap::clear( mm->_map );

        bxGdi::shaderFx_release( dev, resourceManager, &mm->_nativeFx );

        BX_DELETE0( bxDefaultAllocator(), materialManager[0] );
    }
    namespace
    {
        void setMaterialParams( bxGdiShaderFx_Instance* fxI, const GfxMaterialManager::Material& params )
        {
            fxI->setUniform( "diffuseColor", params.diffuseColor );
            fxI->setUniform( "fresnelColor", params.fresnelColor );
            fxI->setUniform( "ambientColor", params.ambientColor );
            fxI->setUniform( "diffuseCoeff", params.diffuseCoeff );
            fxI->setUniform( "roughnessCoeff", params.roughnessCoeff );
            fxI->setUniform( "specularCoeff", params.specularCoeff );
            fxI->setUniform( "ambientCoeff", params.ambientCoeff );
        }
    }

    bxGdiShaderFx_Instance* gfxMaterialManagerCreateMaterial( GfxMaterialManager* materialManager, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* name, const GfxMaterialManager::Material& params )
    {
        const u64 key = gfxMaterialManagerCreateNameHash( name );
        SYS_ASSERT( hashmap::lookup( materialManager->_map, key ) == 0 );

        bxGdiShaderFx_Instance* fxI = bxGdi::shaderFx_createInstance( dev, resourceManager, materialManager->_nativeFx );
        SYS_ASSERT( fxI != 0 );

        setMaterialParams( fxI, params );

        hashmap_t::cell_t* cell = hashmap::insert( materialManager->_map, key );
        cell->value = (size_t)fxI;

        return fxI;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxShadow::GfxShadow()
        : _lightWorld( Matrix4::identity() )
        , _lightView( Matrix4::identity() )
        , _lightProj( Matrix4::identity() )
        , _sortList( nullptr )
    {

    }

    void gfxShadowCreate( GfxShadow* shd, bxGdiDeviceBackend* dev, int shadowMapSize )
    {
        shd->_texDepth = dev->createTexture2Ddepth( shadowMapSize, shadowMapSize, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );
    }

    void gfxShadowDestroy( GfxShadow* shd, bxGdiDeviceBackend* dev )
    {
        dev->releaseTexture( &shd->_texDepth );
        bxGdi::sortList_delete( &shd->_sortList );
    }

    Matrix3 computeBasis1( const Vector3& dir )
    {
        // Derive two remaining vectors
        Vector3 right, up;
        if ( ::abs( dir.getY().getAsFloat() ) > 0.9999f )
        {
            right = Vector3( 1.0f, 0.0f, 0.0f );
        }
        else
        {
            right = normalize( cross( Vector3( 0.0f, 1.0f, 0.0f ), dir ) );
        }

        up = cross( dir, right );
        return Matrix3( right, up, dir );
    }

    void gfxShadowComputeMatrices( GfxShadow* shd, const Vector3 wsCorners[8], const Vector3& lightDirection )
    {
        Vector3 wsCenter( 0.f );
        for( int i = 0; i < 8; ++i )
        {
            wsCenter += wsCorners[i];
        }
        wsCenter *= 1.f / 8.f;

        Matrix3 lRot = computeBasis1( -lightDirection );
        Matrix4 lWorld( lRot, wsCenter );
        Matrix4 lView = orthoInverse( lWorld );
        //bxGfxDebugDraw::addAxes( lWorld );

        bxAABB lsAABB = bxAABB::prepare();
        for( int i = 0; i < 8; ++i )
        {
            Vector3 lsCorner = mulAsVec4( lView, wsCorners[i] );
            lsAABB = bxAABB::extend( lsAABB, lsCorner );
        }

        Vector3 lsAABBsize = bxAABB::size( lsAABB ) * halfVec;
        float3_t lsMin, lsMax, lsExt;
        m128_to_xyz( lsMin.xyz, lsAABB.min.get128() );
        m128_to_xyz( lsMax.xyz, lsAABB.max.get128() );
        m128_to_xyz( lsExt.xyz, lsAABBsize.get128() );



        const Matrix4 lProj = bx::gfx::cameraMatrixOrtho( lsMin.x, lsMax.x, lsMin.y, lsMax.y, -lsExt.z, lsExt.z );

        shd->_lightWorld = lWorld;
        shd->_lightView = lView;
        shd->_lightProj = lProj;

        //bxGfxDebugDraw::addFrustum( lProj * lView, 0xFFFF00FF, true );
    }

    void gfxShadowSortListBuild( GfxShadow* shd, bxChunk* chunk, const GfxScene* scene )
    {
        const GfxScene::Data& data = scene->_data;

        if ( !shd->_sortList || shd->_sortList->capacity < scene->_instancesCount )
        {
            bxGdi::sortList_delete( &shd->_sortList );
            bxGdi::sortList_new( &shd->_sortList, scene->_instancesCount, bxDefaultAllocator() );
        }

        GfxSortListShadow* sortList = shd->_sortList;

        //bxChunk colorChunk, depthChunk;
        bxChunk_create( chunk, 1, scene->_instancesCount );

        const Matrix4 viewProj = shd->_lightProj * shd->_lightView;
        GfxViewFrustum frustum = bx::gfx::viewFrustumExtract( viewProj );

        const int n = data.size;
        for ( int iitem = chunk->begin; iitem < chunk->end; ++iitem )
        {
            const GfxInstanceData& idata = data.idata[iitem];
            const bxAABB& localAABB = data.bbox[iitem];

            for ( int ii = 0; ii < idata.count; ++ii )
            {
                const Matrix4& world = idata.pose[ii];
                bxAABB worldAABB = bxAABB::transform( world, localAABB );

                bool inFrustum = bx::gfx::viewFrustumAABBIntersect( frustum, worldAABB.min, worldAABB.max ).getAsBool();
                if ( !inFrustum )
                    continue;

                float depth = bx::gfx::cameraDepth( shd->_lightWorld, world.getTranslation() ).getAsFloat();
                u16 depth16 = float_to_half_fast3( fromF32( depth ) ).u;

                GfxSortItemShadow sortItem;
                sortItem.key.cascade = 0;
                sortItem.key.depth = depth16;
                sortItem.index = iitem;
                
                bxGdi::sortList_chunkAdd( sortList, chunk, sortItem );
            }
        }
        bxGdi::sortList_sortLess( sortList, *chunk );
    }

    void gfxShadowSortListSubmit( bxGdiContext* gdi, const GfxView& view, const GfxScene::Data& data, const GfxSortListShadow* sList, int begin, int end )
    {
        for ( int i = begin; i < end; ++i )
        {
            const GfxSortListShadow::ItemType& item = sList->items[i];
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

    void gfxShadowDraw( GfxCommandQueue* cmdq, GfxShadow* shd, const GfxScene* scene, const GfxCamera* mainCamera, const Vector3& lightDirection )
    {
        const bxAABB& swAABB = scene->_aabb;

        const Vector3 swCorners[8] =
        {
            swAABB.min,
            Vector3( swAABB.max.getX(), swAABB.min.getY(), swAABB.min.getZ() ),
            Vector3( swAABB.max.getX(), swAABB.max.getY(), swAABB.min.getZ() ),
            Vector3( swAABB.min.getX(), swAABB.max.getY(), swAABB.min.getZ() ),

            Vector3( swAABB.min.getX(), swAABB.min.getY(), swAABB.max.getZ() ),
            Vector3( swAABB.max.getX(), swAABB.min.getY(), swAABB.max.getZ() ),
            Vector3( swAABB.max.getX(), swAABB.max.getY(), swAABB.max.getZ() ),
            Vector3( swAABB.min.getX(), swAABB.max.getY(), swAABB.max.getZ() ),
        };

        gfxShadowComputeMatrices( shd, swCorners, lightDirection );

        bxChunk chunk;
        memset( &chunk, 0x00, sizeof( bxChunk ) );
        gfxShadowSortListBuild( shd, &chunk, scene );
        
        bxGdiContext* gdi = cmdq->_gdiContext;
        GfxContext* ctx = cmdq->_ctx;
        GfxView* view = &cmdq->_view;

        gfxViewUploadInstanceData( gdi, view, scene->_data, shd->_sortList, chunk.begin, chunk.current );
        gdi->changeRenderTargets( nullptr, 0, shd->_texDepth );
        gdi->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 0, 1 );

        const Matrix4 sc = Matrix4::scale( Vector3( 1, 1, 0.5f ) );
        const Matrix4 tr = Matrix4::translation( Vector3( 0, 0, 1 ) );
        const Matrix4 proj = sc * tr * shd->_lightProj;

        bxGdiShaderFx_Instance* fxI = ctx->_fxIShadow;
        fxI->setUniform( "lightViewProj", proj * shd->_lightView );
        fxI->setUniform( "lightDirectionWS", lightDirection );
        bxGdi::shaderFx_enable( gdi, fxI, "shadowDepthPass" );
        
        gfxShadowSortListSubmit( gdi, *view, scene->_data, shd->_sortList, chunk.begin, chunk.current );
    }

    void gfxShadowResolve( GfxCommandQueue* cmdq, bxGdiTexture output, const bxGdiTexture sceneHwDepth, const GfxShadow* shd, const GfxCamera* mainCamera )
    {
        bxGdiContext* gdi = cmdq->_gdiContext;
        GfxContext* ctx = cmdq->_ctx;

        const Matrix4 sc = Matrix4::scale( Vector3( 0.5f, 0.5f, 0.5f ) );
        const Matrix4 tr = Matrix4::translation( Vector3( 1.f, 1.f, 1.f ) );
        const Matrix4 proj = sc * tr * shd->_lightProj;

        bxGdiShaderFx_Instance* fxI = ctx->_fxIShadow;
        fxI->setUniform( "lightViewProj", proj * shd->_lightView );
        fxI->setUniform( "shadowMapSize", float2_t( (float)shd->_texDepth.width, (float)shd->_texDepth.height ) );
        fxI->setTexture( "shadowMap", shd->_texDepth );
        fxI->setTexture( "sceneDepthTex", sceneHwDepth );
        fxI->setSampler( "sampl", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP ) );
        fxI->setSampler( "samplShadowMap", bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_LEQUAL ) );

        gdi->changeRenderTargets( &output, 1 );
        gdi->clearBuffers( 1.f, 1.f, 1.f, 1.f, 0.f, 1, 0 );

        gfxSubmitFullScreenQuad( gdi, fxI, "shadowResolvePass" );
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxToneMap::GfxToneMap() 
        : currentLuminanceTexture( 0 ), tau( 15.f ), autoExposureKeyValue( 0.30f )
        , camera_aperture( 16.f ), camera_shutterSpeed( 1.f / 100.f ), camera_iso( 200.f )
        , useAutoExposure( 1 )
        , fxI( nullptr )
    {}

    void gfxToneMapCreate( GfxToneMap* tm, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        tm->fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "tone_mapping" );
        const int lumiTexSize = 1024;
        tm->adaptedLuminance[0] = dev->createTexture2D( lumiTexSize, lumiTexSize, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        tm->adaptedLuminance[1] = dev->createTexture2D( lumiTexSize, lumiTexSize, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        tm->initialLuminance = dev->createTexture2D( lumiTexSize, lumiTexSize, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    }

    void gfxToneMapDestroy( GfxToneMap* tm, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        dev->releaseTexture( &tm->initialLuminance );
        dev->releaseTexture( &tm->adaptedLuminance[1] );
        dev->releaseTexture( &tm->adaptedLuminance[0] );

        bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &tm->fxI );
    }

    void gfxToneMapDraw( GfxCommandQueue* cmdq, GfxToneMap* tm, bxGdiTexture outTexture, bxGdiTexture inTexture, float deltaTime )
    {
        bxGdiContext* gdi = cmdq->_gdiContext;

        bxGdiShaderFx_Instance* fxI = tm->fxI;

        fxI->setUniform( "input_size0", float2_t( (f32)inTexture.width, (f32)inTexture.height ) );
        fxI->setUniform( "delta_time", deltaTime );
        fxI->setUniform( "lum_tau", tm->tau );
        fxI->setUniform( "auto_exposure_key_value", tm->autoExposureKeyValue );
        fxI->setUniform( "camera_aperture", tm->camera_aperture );
        fxI->setUniform( "camera_shutterSpeed", tm->camera_shutterSpeed );
        fxI->setUniform( "camera_iso", tm->camera_iso );
        fxI->setUniform( "useAutoExposure", tm->useAutoExposure );

        gdi->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 0, bxGdi::eSTAGE_MASK_PIXEL );
        gdi->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_LINEAR, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 1, bxGdi::eSTAGE_MASK_PIXEL );

        //
        //
        gdi->changeRenderTargets( &tm->initialLuminance, 1, bxGdiTexture() );
        gdi->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
        gdi->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
        gfxSubmitFullScreenQuad( gdi, fxI, "luminance_map" );

        //
        //
        gdi->changeRenderTargets( &tm->adaptedLuminance[tm->currentLuminanceTexture], 1, bxGdiTexture() );
        gdi->setTexture( tm->adaptedLuminance[!tm->currentLuminanceTexture], 0, bxGdi::eSTAGE_MASK_PIXEL );
        gdi->setTexture( tm->initialLuminance, 1, bxGdi::eSTAGE_MASK_PIXEL );
        gfxSubmitFullScreenQuad( gdi, fxI, "adapt_luminance" );
        gdi->backend()->generateMipmaps( tm->adaptedLuminance[tm->currentLuminanceTexture] );

        //
        //
        gdi->changeRenderTargets( &outTexture, 1, bxGdiTexture() );
        gdi->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );

        gdi->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
        gdi->setTexture( tm->adaptedLuminance[tm->currentLuminanceTexture], 1, bxGdi::eSTAGE_MASK_PIXEL );

        gfxSubmitFullScreenQuad( gdi, fxI, "composite" );

        tm->currentLuminanceTexture = !tm->currentLuminanceTexture;

        gdi->setTexture( bxGdiTexture(), 0, bxGdi::eSTAGE_PIXEL );
        gdi->setTexture( bxGdiTexture(), 1, bxGdi::eSTAGE_PIXEL );
    }


    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxGlobalResources* GfxContext::_globalResources = nullptr;
    GfxMaterialManager* GfxContext::_materialManager = nullptr;

    GfxContext::GfxContext() 
        : _sunLight( nullptr )
        , _fxISky( nullptr )
        , _fxISao( nullptr )
        , _fxIShadow( nullptr )
        , _allocMesh( nullptr )
        , _allocCamera( nullptr )
        , _allocScene( nullptr )
        , _allocIDataMulti( nullptr )
        , _allocIDataSingle( nullptr )
    {
    }

    GfxContext::~GfxContext()
    {
        int a = 0;
    }

    void gfxGlobalResourcesStartup( GfxGlobalResources** globalResources, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxGlobalResources* gr = BX_NEW( bxDefaultAllocator(), GfxGlobalResources );
        memset( gr, 0x00, sizeof( GfxGlobalResources ) );
        {
            gr->fx.utils = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "utils" );
            gr->fx.texUtils = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "texutils" );
        }

        {//// fullScreenQuad
            const float vertices[] =
            {
                -1.f, -1.f, 0.f, 0.f, 0.f,
                1.f, -1.f, 0.f, 1.f, 0.f,
                1.f, 1.f, 0.f, 1.f, 1.f,

                -1.f, -1.f, 0.f, 0.f, 0.f,
                1.f, 1.f, 0.f, 1.f, 1.f,
                -1.f, 1.f, 0.f, 0.f, 1.f,
            };
            bxGdiVertexStreamDesc vsDesc;
            vsDesc.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 3 );
            vsDesc.addBlock( bxGdi::eSLOT_TEXCOORD0, bxGdi::eTYPE_FLOAT, 2 );
            bxGdiVertexBuffer vBuffer = dev->createVertexBuffer( vsDesc, 6, vertices );

            gr->mesh.fullScreenQuad = bxGdi::renderSource_new( 1 );
            bxGdi::renderSource_setVertexBuffer( gr->mesh.fullScreenQuad, vBuffer, 0 );
        }
        {//// poly shapes
            bxPolyShape polyShape;
            bxPolyShape_createBox( &polyShape, 1 );
            gr->mesh.box = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
            bxPolyShape_deallocateShape( &polyShape );

            bxPolyShape_createShpere( &polyShape, 8 );
            gr->mesh.sphere = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
            bxPolyShape_deallocateShape( &polyShape );
        }

        {//// textures
            gfxLoadTextureFromFile( &gr->texture.noise, dev, resourceManager, "texture/noise256.dds" );
        }

        globalResources[0] = gr;

    }
    void gfxGlobalResourcesShutdown( GfxGlobalResources** globalResources, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxGlobalResources* gr = globalResources[0];

        {
            dev->releaseTexture( &gr->texture.noise );
        }

        {
            bxGdi::renderSource_releaseAndFree( dev, &gr->mesh.sphere );
            bxGdi::renderSource_releaseAndFree( dev, &gr->mesh.box );
            bxGdi::renderSource_releaseAndFree( dev, &gr->mesh.fullScreenQuad );
        }
        {
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &gr->fx.texUtils );
            bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &gr->fx.utils );
        }

        BX_DELETE0( bxDefaultAllocator(), globalResources[0] );
    }



    





}///
