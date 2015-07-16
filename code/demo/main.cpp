#include <system/application.h>
#include <system/window.h>

#include <engine/engine.h>

#include <util/time.h>
#include <util/handle_manager.h>
#include <util/config.h>
#include <gfx/gfx_camera.h>
#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>

#include "physics.h"
#include "renderer.h"
#include "game.h"
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
    bxGfx_HWorld gfxWorld;

    bxGame::Character* character;
    bxGame::Flock* flock;

    bxDesignBlock* dblock;
    //bxPhx_HShape collisionBox;
    //bxPhx_HShape collisionPlane;
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
        bxGfx::startup( _engine.gdiDevice );

        const int fbWidth = 1920;
        const int fbHeight = 1080;
        __framebuffer.textures[bxDemoFramebuffer::eCOLOR] = _engine.gdiDevice->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        __framebuffer.textures[bxDemoFramebuffer::eDEPTH] = _engine.gdiDevice->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );

        __scene._cameraManager = bxGfx::cameraManager_new();

        __scene.gfxWorld = bxGfx::world_create();
        
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

        
        bxPhx::collisionSpace_new();
        __scene.character = bxGame::character_new();
        bxGame::character_init( __scene.character, Matrix4( Matrix3::rotationZ( 0.5f ), Vector3( 0.f, 2.f, 0.f ) ) );
        
        __scene.dblock = bxDesignBlock_new();

        __scene.dblock->create( "ground", 
                                Matrix4::translation( Vector3(0.f, -2.f, 0.f ) ),
                                bxDesignBlock::Shape( 100.f, 0.1f, 100.f ) 
                                );

        __scene.dblock->create( "box0",
                                Matrix4::translation( Vector3( 0.f, -1.5f, 0.f ) ),
                                bxDesignBlock::Shape( 1.f, 1.f, 1.f )
                                );

        //bxPhx_HShape hplane = bxPhx::shape_createPlane( bxPhx::__cspace, makePlane( Vector3::yAxis(), Vector3( 0.f, -2.f, 0.f ) ) );
        //bxPhx_HShape hbox0 = bxPhx::shape_createBox( bxPhx::__cspace, Vector3( 0.5f, -2.0f, 0.f ), Quat::identity(), Vector3( 100.0f, 0.1f, 100.f ) );
        //bxPhx_HShape hbox1 = bxPhx::shape_createBox( bxPhx::__cspace, Vector3( 0.5f, -1.5f, 0.f ), Quat::identity(), Vector3( 1.0f ) );
        //bxGdiRenderSource* boxRsource = bxGfx::globalResources()->mesh.box;
        //bxGdiShaderFx_Instance* redFxI = bxGfx::globalResources()->fx.materialRed;
        //bxGdiShaderFx_Instance* greenFxI = bxGfx::globalResources()->fx.materialGreen;
        //{
        //    bxGfx_HMesh hmesh = bxGfx::mesh_create();
        //    bxGfx_HInstanceBuffer hibuff = bxGfx::instanceBuffer_create( 1 );
        //    bxGfx::mesh_setStreams( hmesh, _engine.gdiDevice, boxRsource );
        //    bxGfx::mesh_setShader( hmesh, _engine.gdiDevice, _engine.resourceManager, greenFxI );
        //    bxGfx_HMeshInstance hmeshi = bxGfx::world_meshAdd( __scene.gfxWorld, hmesh, hibuff );

        //    bxDesignBlock::Handle h = __scene.dblock->create( "ground" );
        //    __scene.dblock->assignMesh( h, hmeshi );
        //    __scene.dblock->assignCollisionShape( h, hbox0 );
        //}
        //{
        //    bxGfx_HMesh hmesh = bxGfx::mesh_create();
        //    bxGfx_HInstanceBuffer hibuff = bxGfx::instanceBuffer_create( 1 );
        //    bxGfx::mesh_setStreams( hmesh, _engine.gdiDevice, boxRsource );
        //    bxGfx::mesh_setShader( hmesh, _engine.gdiDevice, _engine.resourceManager, redFxI );
        //    bxGfx_HMeshInstance hmeshi = bxGfx::world_meshAdd( __scene.gfxWorld, hmesh, hibuff );

        //    bxDesignBlock::Handle h = __scene.dblock->create( "box0" );
        //    __scene.dblock->assignMesh( h, hmeshi );
        //    __scene.dblock->assignCollisionShape( h, hbox0 );
        //}


        __scene.flock = bxGame::flock_new();

        bxGame::flock_init( __scene.flock, 128, Vector3( 0.f ), 5.f );
        bxGame::flock_loadResources( __scene.flock, _engine.gdiDevice, _engine.resourceManager, __scene.gfxWorld );
        return true;
    }
    virtual void shutdown()
    {
        bxGame::flock_delete( &__scene.flock );
        __scene.dblock->cleanUp();
        __scene.dblock->manageResources( _engine.gdiDevice, _engine.resourceManager, bxPhx::__cspace, __scene.gfxWorld );

        bxDesignBlock_delete( &__scene.dblock );
        //bxPhx::shape_release( bxPhx::__cspace, &__scene.collisionBox );
        //bxPhx::shape_release( bxPhx::__cspace, &__scene.collisionPlane );
        bxGame::character_delete( &__scene.character );
        bxPhx::collisionSpace_delete( &bxPhx::__cspace );
                
        bxGfx::world_release( &__scene.gfxWorld );

        bxGfx::cameraManager_delete( &__scene._cameraManager );
        for ( int ifb = 0; ifb < bxDemoFramebuffer::eCOUNT; ++ifb )
        {
            _engine.gdiDevice->releaseTexture( &__framebuffer.textures[ifb] );
        }
        
        bxGfx::shutdown( _engine.gdiDevice, _engine.resourceManager );
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

        bxGfx::frameBegin( _engine.gdiDevice, _engine.resourceManager );
        __scene.dblock->manageResources( _engine.gdiDevice, _engine.resourceManager, bxPhx::__cspace, __scene.gfxWorld );
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

        const bxGfxCamera& currentCamera = bxGfx::camera_current( __scene._cameraManager );
        {
            __scene.dblock->tick( bxPhx::__cspace, __scene.gfxWorld );
            bxGame::flock_tick( __scene.flock, deltaTime );
            bxGame::character_tick( __scene.character, currentCamera, win->input, deltaTime * 2.f );
            bxGame::characterCamera_follow( topCamera, __scene.character, deltaTime, bxGfx::cameraUtil_anyMovement( cameraInputCtx ) );
        }
        
        bxGfx::cameraManager_update( __scene._cameraManager, deltaTime );



        
        bxGdiContext* gdiContext = _engine.gdiContext;

        gdiContext->clear();
        gdiContext->changeRenderTargets( &__framebuffer.textures[0], 1, __framebuffer.textures[bxDemoFramebuffer::eDEPTH] );
        gdiContext->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
        bxGdi::context_setViewport( gdiContext, __framebuffer.textures[0] );

        {
            bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::xAxis(), 0xFF0000FF, true );
            bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::yAxis(), 0x00FF00FF, true );
            bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::zAxis(), 0x0000FFFF, true );

            bxPhx::collisionSpace_debugDraw( bxPhx::__cspace );
        }

        bxGfx::world_draw( gdiContext, __scene.gfxWorld, currentCamera );

        bxGfxDebugDraw::flush( gdiContext, currentCamera.matrix.viewProj );
        bxGfx::rasterizeFramebuffer( gdiContext, __framebuffer.textures[bxDemoFramebuffer::eCOLOR], currentCamera );

        bxGfxGUI::draw( gdiContext );
        gdiContext->backend()->swap();

        _time += deltaTime;

        return true;
    }
    float _time;
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