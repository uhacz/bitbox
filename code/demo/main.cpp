#include <system/application.h>
#include <system/window.h>

#include <engine/engine.h>

#include <util/time.h>
#include <util/handle_manager.h>
#include <util/config.h>
#include <gfx/gfx_camera.h>
#include <gfx/gfx_debug_draw.h>
#include "gfx/gfx_gui.h"
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct bxDemoFramebuffer
{
    enum
    {
        eCOLOR = 0,
        eDEPTH,
        eCOUNT,
    };

    bxGdiTexture textures[eCOUNT];

    int width()  const { return textures[0].width; }
    int height() const { return textures[0].height; }
};
static bxDemoFramebuffer __framebuffer;

struct bxDemoScene
{
    bxGfxCamera_Manager* _cameraManager;
    bxGfxCamera_InputContext cameraInputCtx;
};
static bxDemoScene __scene;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxEngine_startup( &_engine );

        const int fbWidth = 1920;
        const int fbHeight = 1080;
        __framebuffer.textures[bxDemoFramebuffer::eCOLOR] = _engine.gdiDevice->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        __framebuffer.textures[bxDemoFramebuffer::eDEPTH] = _engine.gdiDevice->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );

        __scene._cameraManager = bxGfx::cameraManager_new();
        
        bxGfxCamera_SceneScriptCallback cameraScriptCallback;
        cameraScriptCallback._menago = __scene._cameraManager;

        bxAsciiScript sceneScript;
        bxScene::script_addCallback( &sceneScript, "camera", &cameraScriptCallback );
        bxScene::script_addCallback( &sceneScript, "camera_push", &cameraScriptCallback );

        const char* sceneName = bxConfig::global_string( "scene" );
        bxFS::File scriptFile = _engine.resourceManager->readTextFileSync( sceneName );

        if ( scriptFile.ok() )
        {
            bxScene::script_run( &sceneScript, scriptFile.txt );
        }
        scriptFile.release();

        return true;
    }
    virtual void shutdown()
    {
        bxGfx::cameraManager_delete( &__scene._cameraManager );
        for ( int ifb = 0; ifb < bxDemoFramebuffer::eCOUNT; ++ifb )
        {
            _engine.gdiDevice->releaseTexture( &__framebuffer.textures[ifb] );
        }
        bxEngine_shutdown( &_engine );
    }

    virtual bool update( u64 deltaTimeUS )
    {
        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;
        
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        bxGfxGUI::newFrame( (float)deltaTimeS );

        {
            ImGui::Begin( "System" );
            ImGui::Text( "deltaTime: %.5f", deltaTime );
            ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
            ImGui::End();
        }

        bxGfxCamera_InputContext* cameraInputCtx = &__scene.cameraInputCtx;

        bxGfx::cameraUtil_updateInput( cameraInputCtx, &win->input, 0.1f, deltaTime );
        bxGfxCamera* topCamera = bxGfx::camera_get( __scene._cameraManager, bxGfx::camera_top( __scene._cameraManager ) );
        topCamera->matrix.world = bxGfx::cameraUtil_movement( topCamera->matrix.world
                                                              , cameraInputCtx->leftInputX * 0.25f
                                                              , cameraInputCtx->leftInputY * 0.25f
                                                              , cameraInputCtx->rightInputX * deltaTime * 20.f
                                                              , cameraInputCtx->rightInputY * deltaTime * 20.f
                                                              , cameraInputCtx->upDown * 0.25f );

        bxGfx::cameraManager_update( __scene._cameraManager, deltaTime );


        const bxGfxCamera& currentCamera = bxGfx::camera_current( __scene._cameraManager );

        
        bxGdiContext* gdiContext = _engine.gdiContext;

        gdiContext->clear();
        gdiContext->changeRenderTargets( &__framebuffer.textures[0], 1, __framebuffer.textures[bxDemoFramebuffer::eDEPTH] );
        gdiContext->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
        bxGdi::context_setViewport( gdiContext, __framebuffer.textures[0] );

        {
            bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::xAxis(), 0xFF0000FF, true );
            bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::yAxis(), 0x00FF00FF, true );
            bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::zAxis(), 0x0000FFFF, true );
        }

        bxGfxDebugDraw::flush( gdiContext, currentCamera.matrix.viewProj );
        bxGfx::rasterizeFramebuffer( gdiContext, __framebuffer.textures[bxDemoFramebuffer::eCOLOR], currentCamera );

        bxGfxGUI::draw( gdiContext );
        gdiContext->backend()->swap();

        time += deltaTime;

        return true;
    }
    float time;
    //bxDemoEngine _dengine;
    bxEngine _engine;
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