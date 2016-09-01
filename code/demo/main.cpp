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
#include "motion_fields.h"
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static bx::GameScene __scene = {};

static bx::GfxCamera* g_camera = nullptr;
//static bx::GfxScene* scene = nullptr;
static bx::gfx::CameraInputContext cameraInputCtx = {};

static bx::motion_fields::Data mf_database;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bx::Engine::startup( &_engine );
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
                //bxGfxCamera_Id id = bxGfx::camera_find( __scene._cameraManager, cameraName );
                //if ( id.hash != bxGfx::camera_top( __scene._cameraManager ).hash )
                //{
                //    bxGfx::camera_push( __scene._cameraManager, id );
                //}
            }
        }

        //bx::gfxCameraCreate( &camera, __scene.gfx );
        //bx::gfxSceneCreate( &scene, __scene.gfx );

        //bx::gfxCameraWorldMatrixSet( camera, Matrix4( Matrix3::identity(), Vector3( 0.f, 0.f, 15.f ) ) );

        //bx::GfxGlobalResources* gr = bx::gfxGlobalResourcesGet();
        //bxGdiShaderFx_Instance* matFx[] =
        //{
        //    bx::gfxMaterialFind( "white" ),
        //    bx::gfxMaterialFind( "red" ),
        //    bx::gfxMaterialFind( "grey" ),
        //    bx::gfxMaterialFind( "green" ),
        //    bx::gfxMaterialFind( "blue" ),
        //    bx::gfxMaterialFind( "red" ),
        //};
        //const int N_MAT = sizeof( matFx ) / sizeof( *matFx );
        //bxGdiRenderSource* rsource[] = 
        //{
        //    gr->mesh.sphere,
        //    gr->mesh.box,
        //};
        //const int N_MESH = sizeof( rsource ) / sizeof( *rsource );
        //
        //const int N = 5;
        //const float N_HALF = (float)N * 0.5f;
        //int counter = 0;
        //for( int iz = 0; iz < N; ++iz )
        //{
        //    const float z = -N_HALF + (float)iz * 1.2f;
        //    for( int iy = 0; iy < N; ++iy )
        //    {
        //        const float y = -N_HALF + (float)iy * 1.2f;
        //        for( int ix = 0; ix < N; ++ix, ++counter )
        //        {
        //            const float x = -N_HALF + (float)ix * 1.2f;
        //            const Vector3 pos( x, y, z );

        //            bx::GfxMeshInstanceData meshData;
        //            meshData.renderSourceSet( rsource[ counter % N_MESH ] );
        //            meshData.fxInstanceSet( matFx[ counter % N_MAT ] );
        //            meshData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );

        //            bx::GfxMeshInstance* meshI = nullptr;
        //            bx::gfxMeshInstanceCreate( &meshI, __scene.gfx );

        //            bx::gfxMeshInstanceDataSet( meshI, meshData );
        //            bx::gfxMeshInstanceWorldMatrixSet( meshI, &Matrix4::translation( pos ), 1 );

        //            bx::gfxSceneMeshInstanceAdd( scene, meshI );
        //        }
        //    }
        //}

        //{
        //    bx::GfxMeshInstanceData meshData;
        //    meshData.renderSourceSet( rsource[0] );
        //    meshData.fxInstanceSet( matFx[1] );
        //    meshData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );

        //    bx::GfxMeshInstance* meshI = nullptr;
        //    bx::gfxMeshInstanceCreate( &meshI, __scene.gfx );

        //    bx::gfxMeshInstanceDataSet( meshI, meshData );

        //    Matrix4 pose = Matrix4::translation( Vector3( 0.f, -3.f, 0.f ) );
        //    pose = appendScale( pose, Vector3( 20.f, 1.f, 20.f ) );
        //    bx::gfxMeshInstanceWorldMatrixSet( meshI, &pose, 1 );
        //    bx::gfxSceneMeshInstanceAdd( scene, meshI );
        //}

        const char skelFile[] = "anim/motion_fields/uw_cap6_005.skel";
        const char* animFiles[] = 
        {
            //"anim/motion_fields/uw_cap6_005.anim",
            "anim/motion_fields/uw_cap6_005m.anim",
            //"anim/motion_fields/uw_cap6_023.anim",
            "anim/motion_fields/uw_cap6_023m.anim",
            //"anim/motion_fields/uw_cap6_026.anim",
            "anim/motion_fields/uw_cap6_026m.anim",
            //"anim/motion_fields/uw_cap6_032.anim",
            "anim/motion_fields/uw_cap6_032m.anim",
        };
        const unsigned numAnimFiles = sizeof( animFiles ) / sizeof( *animFiles );
        mf_database.load( skelFile, animFiles, numAnimFiles );

        return true;
    }
    virtual void shutdown()
    {
        //bx::gfxSceneDestroy( &scene );
        //bx::gfxCameraDestroy( &camera );
        mf_database.unload();

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


        const bool useDebugCamera = bxInput_isKeyPressed( &win->input.kbd, bxInput::eKEY_CAPSLOCK );

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

            bxGfxDebugDraw::addAxes( appendScale( Matrix4::identity(), Vector3( 5.f ) ) );
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