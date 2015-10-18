#include "gfx_private.h"
#include <util/buffer_utils.h>
#include <util/hashmap.h>
#include <util/hash.h>

#include <gdi/gdi_context.h>

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
    GfxCamera::GfxCamera() 
        : world( Matrix4::identity() )
        , view( Matrix4::identity() )
        , proj( Matrix4::identity() )
        , viewProj( Matrix4::identity() )
        , hAperture( 1.8f )
        , vAperture( 1.f )
        , focalLength( 50.f )
        , zNear( 0.25f )
        , zFar( 250.f )
        , orthoWidth( 10.f )
        , orthoHeight( 10.f )

        , _ctx( nullptr )
        , _internalHandle( 0 )
    {

    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxScene::GfxScene()
        : _ctx( nullptr )
        , _internalHandle( 0 )
        , _sListColor( nullptr )
        , _sListDepth( nullptr )
    {
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
    void gfxViewCameraSet( bxGdiContext* gdi, GfxView* view, const GfxCamera* camera, int rtw, int rth )
    {
        GfxViewFrameParams viewParams;
        gfxViewFrameParamsFill( &viewParams, camera, rtw, rth );
        gdi->backend()->updateCBuffer( view->_viewParamsBuffer, &viewParams );
    }
    void gfxViewEnable( bxGdiContext* gdi, GfxView* view )
    {
        gdi->setBufferRO( view->_instanceWorldBuffer, 0, bxGdi::eSTAGE_MASK_VERTEX );
        gdi->setBufferRO( view->_instanceWorldITBuffer, 1, bxGdi::eSTAGE_MASK_VERTEX );
        gdi->setCbuffer( view->_viewParamsBuffer, 0, bxGdi::eSTAGE_MASK_VERTEX | bxGdi::eSTAGE_MASK_PIXEL );
        gdi->setCbuffer( view->_instanceOffsetBuffer, 1, bxGdi::eSTAGE_MASK_VERTEX );
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
                params.diffuseCoeff = 0.6f;
                params.roughnessCoeff = 0.2f;
                params.specularCoeff = 0.9f;
                params.ambientCoeff = 0.2f;

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
                params.diffuseCoeff = 0.7f;
                params.roughnessCoeff = 0.7f;

                gfxMaterialManagerCreateMaterial( mm, dev, resourceManager, "blue", params );
            }
            {
                params.diffuseColor = float3_t( 1.f, 1.f, 1.f );
                params.diffuseCoeff = 1.0f;
                params.roughnessCoeff = 1.0f;
                params.specularCoeff = 0.1f;

                gfxMaterialManagerCreateMaterial( mm, dev, resourceManager, "white", params );
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
    GfxGlobalResources* GfxContext::_globalResources = nullptr;
    GfxMaterialManager* GfxContext::_materialManager = nullptr;

    GfxContext::GfxContext() 
        : _fxISky( nullptr )
        , _fxISao( nullptr )
        , _allocMesh( nullptr )
        , _allocCamera( nullptr )
        , _allocScene( nullptr )
        , _allocIDataMulti( nullptr )
        , _allocIDataSingle( nullptr )
    {
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

        globalResources[0] = gr;

    }
    void gfxGlobalResourcesShutdown( GfxGlobalResources** globalResources, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        GfxGlobalResources* gr = globalResources[0];
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
