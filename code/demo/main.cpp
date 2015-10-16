#include <system/application.h>
#include <system/window.h>

#include <engine/engine.h>

#include <util/time.h>
#include <util/handle_manager.h>
#include <util/config.h>
#include <gfx/gfx_camera.h>
#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>


#include "scene.h"
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
static bxDemoScene __scene;

static bx::GfxCamera* camera = nullptr;
static bx::GfxScene* scene = nullptr;
static bx::gfx::CameraInputContext cameraInputCtx;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxEngine_startup( &_engine );
//         bxGfx::startup( _engine.gdiDevice );
// 
//         const int fbWidth = 1920;
//         const int fbHeight = 1080;
//         __framebuffer.textures[bxDemoFramebuffer::eCOLOR] = _engine.gdiDevice->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
//         __framebuffer.textures[bxDemoFramebuffer::eDEPTH] = _engine.gdiDevice->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );

        bxDemoScene_startup( &__scene, &_engine );

        bxGame::flock_loadResources( __scene.flock, _engine.gdiDevice, _engine.resourceManager, __scene.gfxWorld );

        //bxGfxCamera_SceneScriptCallback cameraScriptCallback;
        //cameraScriptCallback._menago = __scene._cameraManager;

        //bxDesignBlock_SceneScriptCallback dblockScriptCallback;
        //dblockScriptCallback.dblock = __scene.dblock;

        //bxAsciiScript sceneScript;
        //bxScene::script_addCallback( &sceneScript, "camera", &cameraScriptCallback );
        //bxScene::script_addCallback( &sceneScript, "camera_push", &cameraScriptCallback );

        //bxScene::script_addCallback( &sceneScript, "dblock", &dblockScriptCallback );
        //bxScene::script_addCallback( &sceneScript, "dblock_commit", &dblockScriptCallback );

        //const char* sceneName = bxConfig::global_string( "scene" );
        //bxFS::File scriptFile = _engine.resourceManager->readTextFileSync( sceneName );

        //if( scriptFile.ok() )
        //{
        //    bxScene::script_run( &sceneScript, scriptFile.txt );
        //}
        //scriptFile.release();

        //{
        //    char const* cameraName = bxConfig::global_string( "camera" );
        //    if( cameraName )
        //    {
        //        bxGfxCamera_Id id = bxGfx::camera_find( __scene._cameraManager, cameraName );
        //        if ( id.hash != bxGfx::camera_top( __scene._cameraManager ).hash )
        //        {
        //            bxGfx::camera_push( __scene._cameraManager, id );
        //        }
        //    }
        //}

        bx::gfxCameraCreate( &camera, __scene.gfx );
        bx::gfxSceneCreate( &scene, __scene.gfx );

        bx::gfxCameraWorldMatrixSet( camera, Matrix4( Matrix3::identity(), Vector3( 0.f, 0.f, 15.f ) ) );

        bx::GfxGlobalResources* gr = bx::gfxGlobalResourcesGet();
        bxGdiShaderFx_Instance* matFx[] =
        {
            bx::gfxMaterialFind( "white" ),
            bx::gfxMaterialFind( "red" ),
            bx::gfxMaterialFind( "green" ),
            bx::gfxMaterialFind( "blue" ),
        };
        const int N_MAT = sizeof( matFx ) / sizeof( *matFx );
        bxGdiRenderSource* rsource[] = 
        {
            gr->mesh.sphere,
            gr->mesh.box,
        };
        const int N_MESH = sizeof( rsource ) / sizeof( *rsource );
        
        const int N = 5;
        const float N_HALF = (float)N * 0.5f;
        int counter = 0;
        for( int iz = 0; iz < N; ++iz )
        {
            const float z = -N_HALF + (float)iz;
            for( int iy = 0; iy < N; ++iy )
            {
                const float y = -N_HALF + (float)iy;
                for( int ix = 0; ix < N; ++ix, ++counter )
                {
                    const float x = -N_HALF + (float)ix;
                    const Vector3 pos( x, y, z );

                    bx::GfxMeshInstanceData meshData;
                    meshData.renderSourceSet( rsource[ counter % N_MESH ] );
                    meshData.fxInstanceSet( matFx[ counter % N_MAT ] );
                    meshData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );

                    bx::GfxMeshInstance* meshI = nullptr;
                    bx::gfxMeshInstanceCreate( &meshI, __scene.gfx );

                    bx::gfxMeshInstanceDataSet( meshI, meshData );
                    bx::gfxMeshInstanceWorldMatrixSet( meshI, &Matrix4::translation( pos ), 1 );

                    bx::gfxSceneMeshInstanceAdd( scene, meshI );
                }
            }
        }
        return true;
    }
    virtual void shutdown()
    {
        bx::gfxSceneDestroy( &scene );
        bx::gfxCameraDestroy( &camera );

        bx::gfxContextTick( __scene.gfx, _engine.gdiDevice, _engine.resourceManager );
        
        bxDemoScene_shutdown( &__scene, &_engine );

//         for ( int ifb = 0; ifb < bxDemoFramebuffer::eCOUNT; ++ifb )
//         {
//             _engine.gdiDevice->releaseTexture( &__framebuffer.textures[ifb] );
//         }
        
        //bxGfx::shutdown( _engine.gdiDevice, _engine.resourceManager );
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

        {
            bxInput* input = &win->input;
            bxInput_Mouse* inputMouse = &input->mouse;
            cameraInputCtx.updateInput( inputMouse->currentState()->lbutton
                                        , inputMouse->currentState()->mbutton
                                        , inputMouse->currentState()->rbutton
                                        , inputMouse->currentState()->dx
                                        , inputMouse->currentState()->dy
                                        , 0.01f
                                        , deltaTime );

            const Matrix4 newCameraMatrix = cameraInputCtx.computeMovement( bx::gfxCameraWorldMatrixGet( camera ), 0.15f );
            bx::gfxCameraWorldMatrixSet( camera, newCameraMatrix );

        }
        bx::gfxCameraComputeMatrices( camera );

        bx::gfxContextTick( __scene.gfx, _engine.gdiDevice, _engine.resourceManager );
        bx::gfxContextFrameBegin( __scene.gfx, _engine.gdiContext );

        bx::GfxCommandQueue* cmdq = nullptr;
        bx::gfxCommandQueueAcquire( &cmdq, __scene.gfx, _engine.gdiContext );
        
        bx::gfxSceneDraw( scene, cmdq, camera );

        bx::gfxCommandQueueRelease( &cmdq );
        bx::gfxContextFrameEnd( __scene.gfx, _engine.gdiContext );


        //bxGfx::frameBegin( _engine.gdiDevice, _engine.resourceManager );
        //__scene.dblock->manageResources( _engine.gdiDevice, _engine.resourceManager, __scene.collisionSpace, __scene.gfxWorld );
        //bxGfxGUI::newFrame( (float)deltaTimeS );

        //{
        //    ImGui::Begin( "System" );
        //    ImGui::Text( "deltaTime: %.5f", deltaTime );
        //    ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
        //    ImGui::End();
        //}

        //bxGfxCamera_InputContext* cameraInputCtx = &__scene.cameraInputCtx;

        //bxGfx::cameraUtil_updateInput( cameraInputCtx, &win->input, 0.1f, deltaTime );
        //bxGfxCamera* topCamera = bxGfx::camera_get( __scene._cameraManager, bxGfx::camera_top( __scene._cameraManager ) );
        //topCamera->matrix.world = bxGfx::cameraUtil_movement( topCamera->matrix.world
        //                                                      , cameraInputCtx->leftInputX * 0.25f
        //                                                      , cameraInputCtx->leftInputY * 0.25f
        //                                                      , cameraInputCtx->rightInputX * deltaTime * 20.f
        //                                                      , cameraInputCtx->rightInputY * deltaTime * 20.f
        //                                                      , cameraInputCtx->upDown * 0.25f );

        //const bxGfxCamera& currentCamera = bxGfx::camera_current( __scene._cameraManager );
        //{
        //    __scene.dblock->tick( __scene.collisionSpace );
        //    bxGame::flock_tick( __scene.flock, deltaTime );
        //    bxGame::character_tick( __scene.character, __scene.collisionSpace, _engine.gdiContext->backend(), currentCamera, win->input, deltaTime * 2.f );
        //    bxGame::characterCamera_follow( topCamera, __scene.character, deltaTime, bxGfx::cameraUtil_anyMovement( cameraInputCtx ) );
        //}
        //
        //bxGfx::cameraManager_update( __scene._cameraManager, deltaTime );



        //
        //bxGdiContext* gdiContext = _engine.gdiContext;

        //gdiContext->clear();
        //gdiContext->changeRenderTargets( &__framebuffer.textures[0], 1, __framebuffer.textures[bxDemoFramebuffer::eDEPTH] );
        //gdiContext->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
        //bxGdi::context_setViewport( gdiContext, __framebuffer.textures[0] );

        //{
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::xAxis(), 0xFF0000FF, true );
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::yAxis(), 0x00FF00FF, true );
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::zAxis(), 0x0000FFFF, true );

        //    //bxPhx::collisionSpace_debugDraw( bxPhx::__cspace );
        //}

        //bxGfx::worldDraw( gdiContext, __scene.gfxWorld, currentCamera );

        //bxGfxDebugDraw::flush( gdiContext, currentCamera.matrix.viewProj );
        //bxGfx::rasterizeFramebuffer( gdiContext, __framebuffer.textures[bxDemoFramebuffer::eCOLOR], currentCamera );

        //bxGfxGUI::draw( gdiContext );
        //gdiContext->backend()->swap();

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