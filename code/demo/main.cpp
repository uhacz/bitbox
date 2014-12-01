#include <system/application.h>
#include <system/window.h>

#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>
#include <gfx/gfx.h>
#include "util/time.h"

#include <util/indexer_packed.h>

static bxGdiShaderFx* fx = 0;
static bxGdiShaderFx_Instance* fxI = 0;
static bxGdiRenderSource* rsource = 0;
static bxGfxRenderList* rList = 0;
static bxGfxCamera camera;
static bxGfxCameraInputContext cameraInputCtx;

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxIndexer_Packed indexer;

        u32 idx0 = indexer.add();
        u32 idx1 = indexer.add();
        u32 idx2 = indexer.add();
        u32 idx3 = indexer.add();
        u32 idx4 = indexer.add();

        indexer.remove( idx2 );

        idx2 = indexer.add();

        bxWindow* win = bxWindow_get();
        _resourceManager = bxResourceManager::startup( "d:/dev/code/bitBox/assets/" );
        //_resourceManager = bxResourceManager::startup( "d:/tmp/bitBox/assets/" );
        bxGdi::backendStartup( &_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

        _gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
        _gdiContext->_Startup( _gdiDevice );

        _gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
        _gfxContext->startup( _gdiDevice, _resourceManager );

        fxI = bxGdi::shaderFx_createWithInstance( _gdiDevice, _resourceManager, "native" );
        
        rsource = bxGfxContext::shared()->rsource.box;

        rList = bxGfx::renderList_new( 128, 256, bxDefaultAllocator() );

        camera.matrix.world = Matrix4::translation( Vector3( 0.f, 0.5f, 15.f ) );

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
    virtual bool update( u64 deltaTimeUS )
    {
        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;


        bxWindow* win = bxWindow_get();
        if( bxInput_isPeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        bxGfx::cameraUtil_updateInput( &cameraInputCtx, &win->input, 1.f, deltaTime );
        camera.matrix.world = bxGfx::cameraUtil_movement( camera.matrix.world
            , cameraInputCtx.leftInputX
            , cameraInputCtx.leftInputY
            , cameraInputCtx.rightInputX * deltaTime * 10.f
            , cameraInputCtx.rightInputY * deltaTime * 10.f
            , cameraInputCtx.upDown * deltaTime );
        
        
        rList->clear();

        {
            const Matrix4 world[] = 
            {
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 1.f, 0.f ) ), Vector3( -2.f, 0.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f, 2.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f,-2.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 0.f, 1.f ) ), Vector3(  2.f, 0.f, 0.f ) ),
            };
            const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
            int dataIndex = rList->renderDataAdd( rsource, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
            u32 surfaceIndex = rList->surfacesAdd( &surf, 1 );
            u32 instanceIndex = rList->instancesAdd( world, 4 );
            rList->itemSubmit( dataIndex, surfaceIndex, instanceIndex );
        }


        //bxGdiContextBackend* gdiContext = _gdiDevice->ctx;

        //float clearColor[5] = { 1.f, 0.f, 0.f, 1.f, 1.f };
        //_gdiContext->clearBuffers( clearColor, 1, 1 );
        //gdiContext->clearBuffers( 0, 0, bxGdiTexture(), clearColor, 1, 1 );

        bxGfx::cameraMatrix_compute( &camera.matrix, camera.params, camera.matrix.world, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

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