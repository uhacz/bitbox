#include <system/application.h>
#include <system/window.h>

#include <engine/engine.h>

#include <util/time.h>
#include <util/handle_manager.h>
#include <util/config.h>
//#include <gfx/gfx_camera.h>
//#include <gfx/gfx_debug_draw.h>
#include <rdi/rdi.h>
#include <rdi/rdi_debug_draw.h>

//#include <gfx/gfx_gui.h>
//#include <gdi/gdi_shader.h>

//#include "scene.h"
#include "renderer.h"
#include "renderer_scene.h"
#include "resource_manager/resource_manager.h"
#include "util/poly/poly_shape.h"
#include "renderer_texture.h"
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
using namespace bx;

// void bx_purecall_handler()
// {
//     SYS_ASSERT( false );
// }

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        //_set_purecall_handler( bx_purecall_handler );
        bx::Engine::StartupInfo engine_startup_info = {};
        engine_startup_info.cfg_filename = "demo_chaos/global.cfg";
        engine_startup_info.start_gdi = false;
        engine_startup_info.start_gfx = false;
        engine_startup_info.start_physics = false;
        engine_startup_info.start_graph = false;

        bx::Engine::startup( &_engine, engine_startup_info );
        //bx::game_scene::startup( &_scene, &_engine );
        bxWindow* win = bxWindow_get();
        rdi::Startup( (uptr)win->hwnd, win->width, win->height, win->full_screen );
        
        bxAsciiScript sceneScript;
        if( _engine._camera_script_callback )
            _engine._camera_script_callback->addCallback( &sceneScript );

        const char* sceneName = bxConfig::global_string( "scene" );
        bxFS::File scriptFile = _engine.resource_manager->readTextFileSync( sceneName );

        if( scriptFile.ok() )
        {
            bxScene::script_run( &sceneScript, scriptFile.txt );
        }
        scriptFile.release();

        //{
        //    char const* cameraName = bxConfig::global_string( "camera" );
        //    if( cameraName )
        //    {
        //        bx::GfxCamera* camera = _engine.camera_manager->find( cameraName );
        //        if( camera )
        //        {
        //            _engine.camera_manager->stack()->push( camera );
        //            _camera = camera;
        //        }
        //    }
        //}

        gfx::RendererDesc renderer_desc = {};
        _renderer.StartUp( renderer_desc, _engine.resource_manager );

        gfx::GeometryPass::_StartUp( &_geometry_pass );
        gfx::ShadowPass::_StartUp( &_shadow_pass, _renderer.GetDesc(), 1024 * 4 );
        gfx::LightPass::_StartUp( &_light_pass );
        gfx::PostProcessPass::_StartUp( &_post_pass );

        _camera.world = Matrix4( Matrix3::rotationY(PI), Vector3( -4.21f, 5.f, -20.6f ) );
        _camera.world *= Matrix4::rotationX( -0.3f );

        {
            gfx::MaterialDesc mat_desc;
            mat_desc.data.diffuse_color = float3_t( 1.f, 0.f, 0.f );
            mat_desc.data.diffuse = 0.5f;
            mat_desc.data.roughness = 0.01f;
            mat_desc.data.specular = 0.9f;
            mat_desc.data.metallic = 0.0f;
            gfx::GMaterialManager()->Create( "red", mat_desc );

            mat_desc.data.diffuse_color = float3_t( 0.f, 1.f, 0.f );
            mat_desc.data.roughness = 0.21f;
            mat_desc.data.specular = 0.91f;
            mat_desc.data.metallic = 0.0f;
            gfx::GMaterialManager()->Create( "green", mat_desc );

            mat_desc.data.diffuse_color = float3_t( 0.f, 0.f, 1.f );
            mat_desc.data.roughness = 0.91f;
            mat_desc.data.specular = 0.19f;
            mat_desc.data.metallic = 0.0f;
            gfx::GMaterialManager()->Create( "blue", mat_desc );


            mat_desc.data.diffuse_color = float3_t( 0.4f, 0.4f, 0.4f );
            mat_desc.data.roughness = 0.5f;
            mat_desc.data.specular = 0.5f;
            mat_desc.data.metallic = 0.0f;
            gfx::GMaterialManager()->Create( "grey", mat_desc );
        }

        _gfx_scene = _renderer.CreateScene( "test" );
        _gfx_scene->EnableSunSkyLight();
        _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
        _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );
        //_gfx_scene->GetSunSkyLight()->sky_intensity = 1.f;
        //_gfx_scene->GetSunSkyLight()->sun_intensity = 1.f;

        {
            gfx::ActorID actor = _gfx_scene->Add( "ground", 1 );
            _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "grey" ) );
            _gfx_scene->SetMesh( actor, gfx::GMeshManager()->Find( ":box" ) );

            Matrix4 pose = Matrix4::translation( Vector3( 0.f, -1.0f, 0.f ) );
            pose = appendScale( pose, Vector3( 100.f, 1.f, 100.f ) );

            _gfx_scene->SetMatrices( actor, &pose, 1 );
        }

        gfx::MeshHandle mesh_handle = gfx::GMeshManager()->Find( ":sphere" );

        for( u32 ir = 0; ir <= 10; ++ir )
        {
            for( u32 is = 0; is <= 10; ++is )
            {
                gfx::MaterialDesc mat_desc;
                mat_desc.data.diffuse_color = float3_t( 0.2f, 0.2f, 0.2f );
                mat_desc.data.diffuse = 0.95f;
                mat_desc.data.roughness = ir * 0.1f;
                mat_desc.data.specular = is * 0.1f;
                mat_desc.data.metallic = 0.0f;

                char nameBuff[32];
                sprintf_s( nameBuff, 32, "mat%u%u", ir, is );
                gfx::MaterialHandle hmat = gfx::GMaterialManager()->Create( nameBuff, mat_desc );

                sprintf_s( nameBuff, 32, "actor%u%u", ir, is );
                gfx::ActorID actor = _gfx_scene->Add( nameBuff, 1 );

                const Vector3 pos = Vector3( -10.f + (float)is, 0.f, -10.f + (float)ir );
                const Matrix4 pose( Matrix3::identity(), pos );

                _gfx_scene->SetMesh( actor, mesh_handle );
                _gfx_scene->SetMaterial( actor, hmat );
                _gfx_scene->SetMatrices( actor, &pose, 1 );
            }
        }

        //_boxes = _gfx_scene->Add( "boxes", NUM_INSTANCES );
        //_spheres = _gfx_scene->Add( "spheres", NUM_INSTANCES );
        //
        //Matrix4 box_instances[ NUM_INSTANCES ] = 
        //{
        //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, PI / 2, 0.f ) ), Vector3( 2.f, -1.f, 2.f ) ),
        //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, PI / 3, 0.f ) ), Vector3( 2.f, -2.f, 2.f ) ),
        //    appendScale( Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( 0.f, -2.f, 0.f ) ), Vector3( 10.f, 0.1f, 10.f ) ),
        //};
        //
        //Matrix4 sph_instances[ NUM_INSTANCES ] = 
        //{
        //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( -2.f, 1.f,-2.f ) ),
        //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( -2.f, 0.f,-2.f ) ),
        //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( -2.f,-2.f,-2.f ) ),
        //};
        //
        //_gfx_scene->SetMatrices( _boxes, box_instances, NUM_INSTANCES );
        //_gfx_scene->SetMatrices( _spheres, sph_instances, NUM_INSTANCES );
        //
        //_gfx_scene->SetMesh( _boxes, gfx::GMeshManager()->Find( ":box" ));
        //_gfx_scene->SetMesh( _spheres, gfx::GMeshManager()->Find( ":sphere" ) );
        //
        //gfx::MaterialHandle material_id = gfx::GMaterialManager()->Find( "red" );
        //_gfx_scene->SetMaterial( _boxes, material_id );
        //
        //material_id = gfx::GMaterialManager()->Find( "green" );
        //_gfx_scene->SetMaterial( _spheres, material_id );
        
        //gfx::TextureHandle htex0 = gfx::GTextureManager()->CreateFromFile( "texture/kozak.dds" );
        //gfx::TextureHandle htex1 = gfx::GTextureManager()->CreateFromFile( "texture/sky_cubemap.DDS" );

        return true;
    }
    virtual void shutdown()
    {
        //_gfx_scene->Remove( &_spheres );
        //_gfx_scene->Remove( &_boxes );

        _renderer.DestroyScene( &_gfx_scene );
        
        //gfx::GMaterialManager()->DestroyByName( "red" );
        //gfx::GMaterialManager()->DestroyByName( "green" );
        //gfx::GMaterialManager()->DestroyByName( "blue" );

        gfx::PostProcessPass::_ShutDown( &_post_pass );
        gfx::LightPass::_ShutDown( &_light_pass );
        gfx::ShadowPass::_ShutDown( &_shadow_pass );
        gfx::GeometryPass::_ShutDown( &_geometry_pass );
        _renderer.ShutDown( _engine.resource_manager );

        rdi::Shutdown();
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

        //bxGfxGUI::newFrame( (float)deltaTimeS );

        //{
        //    ImGui::Begin( "System" );
        //    ImGui::Text( "deltaTime: %.5f", deltaTime );
        //    ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
        //    ImGui::End();
        //}

        rmt_BeginCPUSample( FRAME_UPDATE );

        {
            bxInput* input = &win->input;
            bxInput_Mouse* inputMouse = &input->mouse;
            bxInput_PadState* inputPad = input->pad.currentState();

            if( inputPad->connected && !useDebugCamera )
            {
                const float sensitivity = 3.f;
                _camera_input_ctx.updateInput( -inputPad->analog.right_X * sensitivity, inputPad->analog.right_Y * sensitivity, deltaTime );
            }
            else
            {
                _camera_input_ctx.updateInput( inputMouse->currentState()->lbutton
                                               , inputMouse->currentState()->mbutton
                                               , inputMouse->currentState()->rbutton
                                               , inputMouse->currentState()->dx
                                               , inputMouse->currentState()->dy
                                               , 0.01f
                                               , deltaTime );
            }
            const Matrix4 new_camera_world = _camera_input_ctx.computeMovement( _camera.world, 0.15f );
            _camera.world = new_camera_world;
        }

        gfx::computeMatrices( &_camera );

        rdi::CommandQueue* cmdq = nullptr;
        rdi::frame::Begin( &cmdq );
        _renderer.BeginFrame( cmdq );

        _geometry_pass.PrepareScene( cmdq, _gfx_scene, _camera );
        _geometry_pass.Flush( cmdq );

        _shadow_pass.PrepareScene( cmdq, _gfx_scene, _camera );
        _shadow_pass.Flush( cmdq, rdi::GetTextureDepth( _geometry_pass.GBuffer() ) );

        _light_pass.PrepareScene( cmdq, _gfx_scene, _camera );
        _light_pass.Flush( cmdq, _renderer.GetFramebuffer( gfx::EFramebuffer::SWAP ), _geometry_pass.GBuffer() );


        rdi::TextureRW srcColor = _renderer.GetFramebuffer( gfx::EFramebuffer::SWAP );
        rdi::TextureRW dstColor = _renderer.GetFramebuffer( gfx::EFramebuffer::COLOR );
        _post_pass.DoToneMapping( cmdq, dstColor, srcColor, deltaTime );


        //rdi::TextureRW texture = rdi::GetTexture( _geometry_pass.GBuffer(), 2 );
        //rdi::TextureRW texture = _post_pass._tm.initial_luminance;
        rdi::TextureRW texture = dstColor;
        //rdi::ResourceRO texture = _shadow_pass.DepthMap();
        _renderer.RasterizeFramebuffer( cmdq, texture, _camera, win->width, win->height );

        rdi::debug_draw::AddAxes( Matrix4::identity() );

        rdi::debug_draw::_Flush( cmdq, _camera.view, _camera.proj );
        _renderer.EndFrame( cmdq );

        rdi::frame::End( &cmdq );
        

        rmt_EndCPUSample();

        //rmt_BeginCPUSample( FRAME_DRAW );
        //rmt_BeginCPUSample( scene );
        //rmt_EndCPUSample();

        //rmt_BeginCPUSample( gui );
        ////bxGfxGUI::draw( _engine.gdi_context );
        //rmt_EndCPUSample();
        //rmt_EndCPUSample();

        //bx::gfxContextFrameEnd( _engine.gfx_context, _engine.gdi_context );
        //bx::gfxCommandQueueRelease( &cmdq );

        _time += deltaTime;
        rmt_EndCPUSample();
        return true;
    }
    float _time = 0.f;
    bx::Engine _engine;
    gfx::Renderer _renderer;
    gfx::Scene _gfx_scene = nullptr;
    
    gfx::GeometryPass _geometry_pass;
    gfx::ShadowPass _shadow_pass;
    gfx::LightPass _light_pass;
    gfx::PostProcessPass _post_pass;

    gfx::Camera _camera = {};
    gfx::CameraInputContext _camera_input_ctx = {};

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