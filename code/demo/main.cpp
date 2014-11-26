#include <system/application.h>
#include <system/window.h>

#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>
#include <gfx/gfx.h>


static bxGdiShaderFx* fx = 0;
static bxGdiShaderFx_Instance* fxI = 0;
static bxGdiRenderSource* rsource = 0;
static bxGfxRenderList* rList = 0;
static bxGfxCamera camera;

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        //_resourceManager = bxResourceManager::startup( "d:/dev/code/bitBox/assets/" );
        _resourceManager = bxResourceManager::startup( "d:/tmp/bitBox/assets/" );
        bxGdi::backendStartup( &_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

        _gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
        _gdiContext->_Startup( _gdiDevice );

        _gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
        _gfxContext->startup( _gdiDevice, _resourceManager );

        fxI = bxGdi::shaderFx_createWithInstance( _gdiDevice, _resourceManager, "native" );
        
        rsource = bxGfxContext::shared()->rsource.box;

        rList = bxGfx::renderList_new( 128, 256, bxDefaultAllocator() );

        return true;
    }
    virtual void shutdown()
    {
        bxGfx::renderList_delete( &rList, bxDefaultAllocator() );
        rsource = 0;
        bxGdi::shaderFx_releaseWithInstance( _gdiDevice, &fxI );

        _gfxContext->shutdown( _gdiDevice, _resourceManager );
        BX_DELETE0( bxDefaultAllocator(), _gfxContext );
        
        _gdiContext->_Shutdown();
        BX_DELETE0( bxDefaultAllocator(), _gdiContext );

        bxGdi::backendShutdown( &_gdiDevice );
        bxResourceManager::shutdown( &_resourceManager );
    }
    virtual bool update()
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isPeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        rList->clear();

        {
            const Matrix4 world = Matrix4::identity();
            const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
            int dataIndex = rList->renderDataAdd( rsource, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
            u32 surfaceIndex = rList->surfacesAdd( &surf, 1 );
            u32 instanceIndex = rList->instancesAdd( &world, 1 );
            rList->itemSubmit( dataIndex, surfaceIndex, instanceIndex );
        }


        //bxGdiContextBackend* gdiContext = _gdiDevice->ctx;

        //float clearColor[5] = { 1.f, 0.f, 0.f, 1.f, 1.f };
        //_gdiContext->clearBuffers( clearColor, 1, 1 );
        //gdiContext->clearBuffers( 0, 0, bxGdiTexture(), clearColor, 1, 1 );

        _gfxContext->frameBegin( _gdiContext );

        _gfxContext->frameDraw( _gdiContext, camera, &rList, 1 );

        _gfxContext->frameEnd( _gdiContext );

        return true;
    }

    bxGdiDeviceBackend* _gdiDevice;
    bxGdiContext* _gdiContext;
    bxGfxContext* _gfxContext;
    bxResourceManager* _resourceManager;
};

int main( int argc, const char* argv[] )
{
    bxWindow* window = bxWindow_create( "demo", 1280, 720, false, 0 );
    if( window )
    {
        bxDemoApp app;
        if( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }
    
    
    return 0;
}