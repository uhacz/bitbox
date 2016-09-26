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
#include "motion_matching.h"

#include <cstdio>
#include "util/signal_filter.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static bx::GameScene __scene = {};

static bx::GfxCamera* g_camera = nullptr;
static bx::gfx::CameraInputContext cameraInputCtx = {};
static bx::motion_matching::DynamicState dynamic_state = {};
static bx::motion_matching::Context mm = {};
static u32 pose_index = UINT32_MAX;


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bx::Engine::StartupInfo engine_startup_info;
        engine_startup_info.cfg_filename = "demo/global.cfg";
        bx::Engine::startup( &_engine, engine_startup_info );
        gameSceneStartup( &__scene, &_engine );

        //bxGame::flock_loadResources( __scene.flock, _engine.gdiDevice, _engine.resourceManager );

        //bx::CameraManagerSceneScriptCallback cameraScriptCallback;
        //cameraScriptCallback._menago = __scene.cameraManager;
        //cameraScriptCallback._gfx = _engine.gfxContext;

        bx::DesignBlockSceneScriptCallback dblockScriptCallback;
        dblockScriptCallback.dblock = __scene.dblock;

        bxAsciiScript sceneScript;
        _engine._camera_script_callback->addCallback( &sceneScript );
        dblockScriptCallback.addCallback( &sceneScript );

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
                    g_camera = camera;
                }
            }
        }

        //const char skelFile[] = "anim/motion_fields/uw_cap6_005m_s.skel";
        const char skelFile[] = "anim/motion_fields/2/skeleton.skel";
        //const char skelFile[] = "anim/motion_fields/1/T.skel";
        //const char skelFile[] = "anim/motion_fields/run_circles.skel";


        const bx::motion_matching::AnimClipInfo animFiles[] = 
        {
            //{ "anim/motion_fields/1/idle.anim"            ,1 }, 
            //{ "anim/motion_fields/1/run.anim"             ,1 }, 
            //{ "anim/motion_fields/1/run_turn_left_1.anim" ,0 }, 
            //{ "anim/motion_fields/1/run_turn_left_2.anim" ,0 }, 
            //{ "anim/motion_fields/1/run_turn_right_1.anim",0 }, 
            //{ "anim/motion_fields/1/run_turn_right_2.anim",0 }, 
            //{ "anim/motion_fields/1/walk.anim"            ,1 }, 

            { "anim/motion_fields/2/idle.anim"              , 1 },
            { "anim/motion_fields/2/walking0.anim"          , 1 },
            { "anim/motion_fields/2/walking1.anim"          , 1 },
            { "anim/motion_fields/2/running.anim"           , 1 },
            { "anim/motion_fields/2/walking_left_turn.anim" , 0 },
            { "anim/motion_fields/2/walking_right_turn.anim", 0 },
            { "anim/motion_fields/2/running_left_turn.anim" , 0 },
            { "anim/motion_fields/2/running_right_turn.anim", 0 },
            //{ "anim/motion_fields/2/running_jump.anim"      , 0 },
            { "anim/motion_fields/2/running_stop.anim"      , 0 },
            //{ "anim/motion_fields/2/running_back.anim"      , 1 },
            { "anim/motion_fields/2/walking_180_turn.anim"  , 0 },
            { "anim/motion_fields/2/walking_start.anim"     , 0 },
            { "anim/motion_fields/2/walking_back.anim"      , 1 },
        };
        
        const unsigned numAnimFiles = sizeof( animFiles ) / sizeof( *animFiles );
        mm.load( skelFile, animFiles, numAnimFiles );

        
        bx::motion_matching::ContextPrepareInfo ctx_prepare_info{};
        ctx_prepare_info.matching_joint_names[0] = "Hips";
        ctx_prepare_info.matching_joint_names[1] = "LeftToeBase";
        ctx_prepare_info.matching_joint_names[2] = "RightToeBase";
        mm.prepare( ctx_prepare_info );

        return true;
    }
    virtual void shutdown()
    {
        mm.unprepare();
        mm.unload();

        gameSceneShutdown( &__scene, &_engine );
        bx::gfxCameraDestroy( &g_camera );
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

		rmt_BeginCPUSample(FRAME_UPDATE);

        bx::GfxCamera* camera = _engine.camera_manager->stack()->top();

        {
            bxInput* input = &win->input;
            bxInput_Mouse* inputMouse = &input->mouse;
            bxInput_PadState* inputPad = input->pad.currentState();

            if( inputPad->connected && !useDebugCamera )
            {
                const float sensitivity = 3.f;
                cameraInputCtx.updateInput( -inputPad->analog.right_X * sensitivity, inputPad->analog.right_Y * sensitivity, deltaTime );
            }
            else
            {
                cameraInputCtx.updateInput( inputMouse->currentState()->lbutton
                                            , inputMouse->currentState()->mbutton
                                            , inputMouse->currentState()->rbutton
                                            , inputMouse->currentState()->dx
                                            , inputMouse->currentState()->dy
                                            , 0.01f
                                            , deltaTime );
            }
            const Matrix4 newCameraMatrix = cameraInputCtx.computeMovement( bx::gfxCameraWorldMatrixGet( camera ), 0.15f );
            bx::gfxCameraWorldMatrixSet( camera, newCameraMatrix );

        }
        bx::gfxCameraComputeMatrices( camera );

        __scene.dblock->manageResources( __scene.gfx_scene(), __scene.phx_scene() );
        
        {
            bx::phxSceneSync( __scene.phx_scene() );
        }
        
        {//// game update
			__scene.dblock->tick();
            bx::terrainTick( __scene.terrain, &__scene, _engine.gdi_context->backend(), deltaTime * 2.f );
            //bx::characterTick( __scene.character, _engine.gdiContext->backend(), &__scene, win->input, deltaTime * 2.f );
			__scene.cct->tick( &__scene, camera, win->input, deltaTime * 2.f );
			bx::charAnimControllerTick( __scene.canim, &__scene, deltaTimeUS );
            bx::charGfxTick( __scene.cgfx, &__scene );
			//bx::charAnimTick( __scene.canim, __scene.cct->worldPoseFoot(), deltaTime );
			
            if( !useDebugCamera )
            {
                const Vector3 characterPos = __scene.cct->worldPose().getTranslation();
                const Vector3 characterUp  = __scene.cct->upDirection();
				const Vector3 characterDPos = __scene.cct->deltaPosition();
				//const Vector3 characterPos = bx::characterPoseGet( __scene.character ).getTranslation();
				//const Vector3 characterUp = bx::characterUpVectorGet( __scene.character );
                __scene.cameraController.follow( camera, characterPos, characterUp, characterDPos, deltaTime, cameraInputCtx.anyMovement() );
            }

            //bxGfxDebugDraw::addAxes( appendScale( Matrix4::identity(), Vector3( 5.f ) ) );
        }


        {
            bx::motion_matching::DynamicState::Input dynamic_state_input = {};
            {

                const float spd01 = dynamic_state._speed01;
                float anim_vel = bx::curve::evaluate_catmullrom( mm._data.velocity_curve, spd01 );
                if( mm.currentPose( &dynamic_state_input.anim_pose ) && mm.currentSpeed( &dynamic_state_input.anim_velocity ) )
                {
                    dynamic_state_input.data_valid = 1;
                }
                //if( spd01 > 0.1f )
                //{
                //    float anim_vel1 = 0.f;
                //    if( mm.currentSpeed( &anim_vel1 ) )
                //    {
                //        //anim_vel = lerp( spd01, anim_vel, anim_vel1 );
                //        anim_vel = anim_vel1;
                //    }
                //}
                dynamic_state._max_speed = signalFilter_lowPass( anim_vel, dynamic_state._max_speed, 0.01f, deltaTime );

                //if( mm._state.num_clips )
                //{
                //    float clip_duration = mm._data.clips[mm._state.clip_index[0]]->duration;
                //    dynamic_state._trajectory_integration_time = clip_duration;
                //}
            }
            
            dynamic_state.tick( win->input, dynamic_state_input, deltaTime );
            dynamic_state.debugDraw( 0xFF0000FF );
            
            bx::motion_matching::Input input = {};
            bx::motion_matching::motionMatchingCollectInput( &input, dynamic_state );

            mm.tick( input, deltaTime );

            if( ImGui::Begin( "MotionMatching" ) )
            {
                const int PLOT_POINTS = 16;
                float v[PLOT_POINTS];
                for( u32 i = 0; i < PLOT_POINTS; ++i )
                {
                    float t = (float)i / (float)( PLOT_POINTS - 1 );
                    v[i] = bx::curve::evaluate_catmullrom( mm._data.velocity_curve, t );
                }
                ImGui::PlotLines( "velocity curve", v, PLOT_POINTS, 0, nullptr, 0.f, 1.f );
                ImGui::SliderFloat( "current velocity", &dynamic_state._speed01, 0.f, 1.f );
                ImGui::SliderFloat( "max velocity", &dynamic_state._max_speed, 0.f, 1.f );

                if( mm._anim_player._num_clips )
                {
                    u64 clip_index = UINT64_MAX;
                    if( mm._anim_player.userData( &clip_index, 0 ) )
                    {
                        ImGui::Text( "current clip: %s", mm._data.clip_infos[clip_index].name.c_str() );
                        
                        const bx::anim::SimplePlayer::Clip& c = mm._anim_player._clips[0];
                        float eval_time = c.eval_time;
                        ImGui::SliderFloat( "current clip time", &eval_time, 0.f, c.clip->duration );
                    }
                }

                if( ImGui::CollapsingHeader( "Poses" ) )
                {
                    for( size_t i = 0; i < mm._data.poses.size(); ++i )
                    {
                        const bx::motion_matching::Pose& pose = mm._data.poses[i];
                        
                        char button_name[256] = {};
                        _snprintf_s( button_name, 256, "%s [%f]", mm._data.clip_infos[pose.params.clip_index].name.c_str(), pose.params.clip_start_time );
                        
                        if( ImGui::RadioButton( button_name, pose_index == (u32)i ) )
                        {
                            pose_index = (u32)i;
                        }
                    }

                    if( pose_index != UINT32_MAX )
                    {
                        mm._DebugDrawPose( pose_index, 0x0000FFFF, Matrix4::identity() );
                    }
                }


            }
            ImGui::End();
        }

        bx::gfxCameraComputeMatrices( camera );

        {
            bx::phxSceneSimulate( __scene.phx_scene(), deltaTime );
        }

        bx::gfxContextTick( _engine.gfx_context, _engine.gdi_device );

		rmt_EndCPUSample();

        bx::GfxCommandQueue* cmdq = nullptr;
        bx::gfxCommandQueueAcquire( &cmdq, _engine.gfx_context, _engine.gdi_context );
        bx::gfxContextFrameBegin( _engine.gfx_context, _engine.gdi_context );

		rmt_BeginCPUSample(FRAME_DRAW);
		rmt_BeginCPUSample(scene);
		bx::gfxSceneDraw(__scene.gfx_scene(), cmdq, camera);
		rmt_EndCPUSample();
        
		rmt_BeginCPUSample(gui);
		bxGfxGUI::draw( _engine.gdi_context );
		rmt_EndCPUSample();
		rmt_EndCPUSample();

        bx::gfxContextFrameEnd( _engine.gfx_context, _engine.gdi_context );
        bx::gfxCommandQueueRelease( &cmdq );

        

        //const bxGfxCamera& currentCamera = bxGfx::camera_current( __scene._cameraManager );
        //{
        //    __scene.dblock->tick( __scene.collisionSpace );
        //    bxGame::flock_tick( __scene.flock, deltaTime );
        //    bxGame::character_tick( __scene.character, __scene.collisionSpace, _engine.gdiContext->backend(), currentCamera, win->input, deltaTime * 2.f );
        //    bxGame::characterCamera_follow( topCamera, __scene.character, deltaTime, bxGfx::cameraUtil_anyMovement( cameraInputCtx ) );
        //}
        //
        //bxGfx::cameraManager_update( __scene._cameraManager, deltaTime );


        //{
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::xAxis(), 0xFF0000FF, true );
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::yAxis(), 0x00FF00FF, true );
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::zAxis(), 0x0000FFFF, true );

        //    //bxPhx::collisionSpace_debugDraw( bxPhx::__cspace );
        //}

        

        _time += deltaTime;
        rmt_EndCPUSample();
        return true;
    }
    float _time = 0.f;
    bx::Engine _engine;
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