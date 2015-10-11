#include "gfx_private.h"
#include "util/buffer_utils.h"

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
            gfxSceneDataAllocate( data, data->size * 2 + 8, data->alloc );
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
        view->_maxInstances = maxInstances;
    }

    void gfxViewDestroy( GfxView* view, bxGdiDeviceBackend* dev )
    {
        view->_maxInstances = 0;
        dev->releaseBuffer( &view->_instanceWorldITBuffer );
        dev->releaseBuffer( &view->_instanceWorldBuffer );
        dev->releaseBuffer( &view->_viewParamsBuffer );
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxCommandQueue::GfxCommandQueue() : _acquireCounter( 0 )
        , _ctx( nullptr )
        , _device( nullptr )
    {
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    GfxContext::GfxContext() : _allocMesh( nullptr )
        , _allocCamera( nullptr )
        , _allocScene( nullptr )
        , _allocIDataMulti( nullptr )
        , _allocIDataSingle( nullptr )
    {
    }








}///