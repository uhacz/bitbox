#include "gfx_private.h"

namespace bx
{
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
    GfxMeshInstance::GfxMeshInstance() 
        : _ctx( nullptr )
        , _scene( nullptr )
        , _internalHandle( 0 )

        , _rsource( nullptr )
        , _fxI( nullptr )
    {
        memset( localAABB, 0x00, sizeof( localAABB ) );
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