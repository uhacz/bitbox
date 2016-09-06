#include <system/application.h>
#include <system/window.h>

#include <engine/engine.h>

#include <util/time.h>
#include <util/handle_manager.h>
#include <util/config.h>
//#include <gfx/gfx_camera.h>
#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>

#include "scene.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bx::Engine::startup( &_engine );
        bx::game_scene::startup( &_scene, &_engine );


        bxAsciiScript sceneScript;
        _engine._camera_script_callback->addCallback( &sceneScript );

        const char* sceneName = bxConfig::global_string( "scene" );
        bxFS::File scriptFile = _engine.resource_manager->readTextFileSync( sceneName );

        if( scriptFile.ok() )
        {
            bxScene::script_run( &sceneScript, scriptFile.txt );
        }
        scriptFile.release();

        {
            char const* cameraName = bxConfig::global_string( "camera" );
            if( cameraName )
            {
                bx::GfxCamera* camera = _engine.camera_manager->find( cameraName );
                if( camera )
                {
                    _engine.camera_manager->stack()->push( camera );
                    _camera = camera;
                }
            }
        }
        return true;
    }
    virtual void shutdown()
    {
        bx::game_scene::shutdown( &_scene, &_engine );
        bx::gfxCameraDestroy( &_camera );
        bx::Engine::shutdown( &_engine );
    }

    virtual bool update( u64 deltaTimeUS )
    {
        rmt_BeginCPUSample( FRAME );
        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;

        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }


        const bool useDebugCamera = true; // bxInput_isKeyPressed( &win->input.kbd, bxInput::eKEY_CAPSLOCK );

        bxGfxGUI::newFrame( (float)deltaTimeS );

        {
            ImGui::Begin( "System" );
            ImGui::Text( "deltaTime: %.5f", deltaTime );
            ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
            ImGui::End();
        }

        rmt_BeginCPUSample( FRAME_UPDATE );

        bx::GfxCamera* camera = _engine.camera_manager->stack()->top();

        {
            bxInput* input = &win->input;
            bxInput_Mouse* inputMouse = &input->mouse;
            bxInput_PadState* inputPad = input->pad.currentState();

            if( inputPad->connected && !useDebugCamera )
            {
                const float sensitivity = 3.f;
                _cameraInputCtx.updateInput( -inputPad->analog.right_X * sensitivity, inputPad->analog.right_Y * sensitivity, deltaTime );
            }
            else
            {
                _cameraInputCtx.updateInput( inputMouse->currentState()->lbutton
                                            , inputMouse->currentState()->mbutton
                                            , inputMouse->currentState()->rbutton
                                            , inputMouse->currentState()->dx
                                            , inputMouse->currentState()->dy
                                            , 0.01f
                                            , deltaTime );
            }
            const Matrix4 newCameraMatrix = _cameraInputCtx.computeMovement( bx::gfxCameraWorldMatrixGet( camera ), 0.15f );
            bx::gfxCameraWorldMatrixSet( camera, newCameraMatrix );

        }
        bx::gfxCameraComputeMatrices( camera );

        {
            bx::phxSceneSync( _scene.phx_scene() );
        }

        bx::gfxCameraComputeMatrices( camera );

        {
            bx::phxSceneSimulate( _scene.phx_scene(), deltaTime );
        }

        bx::gfxContextTick( _engine.gfx_context, _engine.gdi_device );

        rmt_EndCPUSample();

        bx::GfxCommandQueue* cmdq = nullptr;
        bx::gfxCommandQueueAcquire( &cmdq, _engine.gfx_context, _engine.gdi_context );
        bx::gfxContextFrameBegin( _engine.gfx_context, _engine.gdi_context );

        rmt_BeginCPUSample( FRAME_DRAW );
        rmt_BeginCPUSample( scene );
        bx::gfxSceneDraw( _scene.gfx_scene(), cmdq, camera );
        rmt_EndCPUSample();

        rmt_BeginCPUSample( gui );
        bxGfxGUI::draw( _engine.gdi_context );
        rmt_EndCPUSample();
        rmt_EndCPUSample();

        bx::gfxContextFrameEnd( _engine.gfx_context, _engine.gdi_context );
        bx::gfxCommandQueueRelease( &cmdq );

        _time += deltaTime;
        rmt_EndCPUSample();
        return true;
    }
    float _time = 0.f;
    bx::Engine _engine;
    bx::GameScene _scene;
    bx::GfxCamera* _camera = nullptr;
    bx::gfx::CameraInputContext _cameraInputCtx = {};
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