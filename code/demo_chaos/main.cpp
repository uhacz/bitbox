#include <system/application.h>
#include <system/window.h>

#include <engine/engine.h>

#include <util/time.h>
#include <util/handle_manager.h>
#include <util/config.h>
//#include <gfx/gfx_camera.h>
//#include <gfx/gfx_debug_draw.h>
#include <rdi/rdi.h>

//#include <gfx/gfx_gui.h>
//#include <gdi/gdi_shader.h>

//#include "scene.h"
#include "renderer.h"
#include "resource_manager/resource_manager.h"
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
using namespace bx;
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
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

        
        _shf_texutil = rdi::ShaderFileLoad( "shader/bin/texture_utils.shader", _engine.resource_manager );
        _shf_deffered = rdi::ShaderFileLoad( "shader/bin/deffered.shader", _engine.resource_manager );
        
        {
            rdi::PipelineDesc pipeline_desc = {};
            pipeline_desc.Shader( _shf_texutil, "copy_rgba" );
            _pipeline_copy_texture_rgba = rdi::CreatePipeline( pipeline_desc );
        }

        {
            rdi::PipelineDesc pipeline_desc = {};

            pipeline_desc.Shader( _shf_deffered, "geometry_notexture" );
            _pipeline_geometry_notex = rdi::CreatePipeline( pipeline_desc );

            pipeline_desc.Shader( _shf_deffered, "geometry_texture" );
            _pipeline_geometry_tex = rdi::CreatePipeline( pipeline_desc );
        }

        {
            rdi::RenderTargetDesc rt_desc = {};
            rt_desc.Size( 1920, 1080 );
            rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
            rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
            rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
            rt_desc.Depth( rdi::EDataType::DEPTH32F );

            _rtarget_gbuffer = rdi::CreateRenderTarget( rt_desc );
        }
        {
            rdi::RenderTargetDesc rt_desc = {};
            rt_desc.Size( 1920, 1080 );
            rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );

            _rtarget_color = rdi::CreateRenderTarget( rt_desc );
        }

        {
            
        }

        //const bxGdiFormat texture_formats[] =
        //{
        //    { bx::gdi::eTYPE_FLOAT, 4 },
        //};
        //bx::gfx::RenderPassDesc render_pass_desc = {};
        //render_pass_desc.num_color_textures = 1;
        //render_pass_desc.color_texture_formats = texture_formats;
        //render_pass_desc.depth_texture_type = bx::gdi::eTYPE_DEPTH32F;
        //render_pass_desc.width = 1920;
        //render_pass_desc.height = 1080;

        //_render_pass = bx::gfx::createRenderPass( render_pass_desc );


        //_native_shader_module = bx::gdi::shaderFx_createFromFile( _engine.gdi_device, _engine.resource_manager, "native2" );

        //const bxGdiVertexStreamBlock stream_descs[2] =
        //{
        //    { bx::gdi::eSLOT_POSITION, bx::gdi::eTYPE_FLOAT, 3 },
        //    { bx::gdi::eSLOT_NORMAL, bx::gdi::eTYPE_FLOAT, 3 },
        //};

        //bx::gfx::PipelineDesc pipeline_desc = {};
        //pipeline_desc.shaders[0] = _native_shader_module->vertexShader( 0 );
        //pipeline_desc.shaders[1] = _native_shader_module->pixelShader( 0 );
        //pipeline_desc.num_vertex_stream_descs = 2;
        //pipeline_desc.vertex_stream_descs = stream_descs;
        //_pipeline_native_pos_nrm_solid = bx::gfx::createPipeline( pipeline_desc );

        //{
        //    bx::gfx::ResourceBinding bindings[] = 
        //    {
        //        { bx::gfx::eRESOURCE_TYPE_UNIFORM  , bx::gdi::eSTAGE_MASK_VERTEX | bx::gdi::eSTAGE_MASK_PIXEL, 0, 1 }, // frame data
        //        { bx::gfx::eRESOURCE_TYPE_UNIFORM  , bx::gdi::eSTAGE_MASK_VERTEX, 1, 1 }, // instance offset
        //        { bx::gfx::eRESOURCE_TYPE_READ_ONLY, bx::gdi::eSTAGE_MASK_VERTEX, 1, 2 }, // instance data
        //        { bx::gfx::eRESOURCE_TYPE_UNIFORM  , bx::gdi::eSTAGE_MASK_PIXEL, 3, 1 }, // material data
        //    };
        //    bx::gfx::ResourceLayout layout = {};
        //    layout.bindings = bindings;
        //    layout.num_bindings = 1;
        //    _frame_data_rdesc = bx::gfx::createResourceDescriptor( layout );

        //    layout.bindings = &bindings[1];
        //    layout.num_bindings = 3;
        //    _instance_data_rdesc = bx::gfx::createResourceDescriptor( layout );

        //    layout.bindings = &bindings[4];
        //    layout.num_bindings = 1;
        //    _material_data_rdesc = bx::gfx::createResourceDescriptor( layout );
        //}

        return true;
    }
    virtual void shutdown()
    {
        rdi::DestroyRenderTarget( &_rtarget_color );
        rdi::DestroyRenderTarget( &_rtarget_gbuffer );

        rdi::DestroyPipeline( &_pipeline_geometry_tex );
        rdi::DestroyPipeline( &_pipeline_geometry_notex );
        rdi::DestroyPipeline( &_pipeline_copy_texture_rgba );

        rdi::ShaderFileUnload( &_shf_deffered, _engine.resource_manager );
        rdi::ShaderFileUnload( &_shf_texutil, _engine.resource_manager );

        //bx::gfx::destroyResourceDescriptor( &_material_data_rdesc );
        //bx::gfx::destroyResourceDescriptor( &_instance_data_rdesc );
        //bx::gfx::destroyResourceDescriptor( &_frame_data_rdesc );

        //bx::gfx::destroyPipeline( &_pipeline_native_pos_nrm_solid, _engine.gdi_device );
        //bx::gdi::shaderFx_release( _engine.gdi_device, _engine.resource_manager, &_native_shader_module );
        //bx::gfx::destroyRenderPass( &_render_pass, _engine.gdi_device );

        //bx::game_scene::shutdown( &_scene, &_engine );
        //bx::gfxCameraDestroy( &_camera );
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

        //bx::GfxCamera* camera = _engine.camera_manager->stack()->top();

        //{
        //    bxInput* input = &win->input;
        //    bxInput_Mouse* inputMouse = &input->mouse;
        //    bxInput_PadState* inputPad = input->pad.currentState();

        //    if( inputPad->connected && !useDebugCamera )
        //    {
        //        const float sensitivity = 3.f;
        //        _cameraInputCtx.updateInput( -inputPad->analog.right_X * sensitivity, inputPad->analog.right_Y * sensitivity, deltaTime );
        //    }
        //    else
        //    {
        //        _cameraInputCtx.updateInput( inputMouse->currentState()->lbutton
        //                                    , inputMouse->currentState()->mbutton
        //                                    , inputMouse->currentState()->rbutton
        //                                    , inputMouse->currentState()->dx
        //                                    , inputMouse->currentState()->dy
        //                                    , 0.01f
        //                                    , deltaTime );
        //    }
        //    const Matrix4 newCameraMatrix = _cameraInputCtx.computeMovement( bx::gfxCameraWorldMatrixGet( camera ), 0.15f );
        //    bx::gfxCameraWorldMatrixSet( camera, newCameraMatrix );
        //}

        //bx::gfxCameraComputeMatrices( camera );
        //
        //{
        //    bx::phxSceneSync( _scene.phx_scene() );
        //}
        //
        //bx::gfxCameraComputeMatrices( camera );
        //
        //{
        //    bx::phxSceneSimulate( _scene.phx_scene(), deltaTime );
        //}
        //
        //bx::gfxContextTick( _engine.gfx_context, _engine.gdi_device );

        rmt_EndCPUSample();

        //bx::GfxCommandQueue* cmdq = nullptr;
        //bx::gfxCommandQueueAcquire( &cmdq, _engine.gfx_context, _engine.gdi_context );
        //bx::gfxContextFrameBegin( _engine.gfx_context, _engine.gdi_context );

        rmt_BeginCPUSample( FRAME_DRAW );
        rmt_BeginCPUSample( scene );
        //bx::gfxSceneDraw( _scene.gfx_scene(), cmdq, camera );
        rmt_EndCPUSample();

        rmt_BeginCPUSample( gui );
        //bxGfxGUI::draw( _engine.gdi_context );
        rmt_EndCPUSample();
        rmt_EndCPUSample();

        //bx::gfxContextFrameEnd( _engine.gfx_context, _engine.gdi_context );
        //bx::gfxCommandQueueRelease( &cmdq );

        _time += deltaTime;
        rmt_EndCPUSample();
        return true;
    }
    float _time = 0.f;
    bx::Engine _engine;
    //bx::GameScene _scene;
    //bx::GfxCamera* _camera = nullptr;
    //bx::gfx::CameraInputContext _cameraInputCtx = {};

    bx::rdi::ShaderFile* _shf_texutil = nullptr;
    bx::rdi::Pipeline _pipeline_copy_texture_rgba = BX_RDI_NULL_HANDLE;
    
    bx::rdi::ShaderFile* _shf_deffered = nullptr;
    bx::rdi::Pipeline _pipeline_geometry_notex = BX_RDI_NULL_HANDLE;
    bx::rdi::Pipeline _pipeline_geometry_tex = BX_RDI_NULL_HANDLE;

    bx::rdi::RenderTarget _rtarget_gbuffer = BX_RDI_NULL_HANDLE;
    bx::rdi::RenderTarget _rtarget_color = BX_RDI_NULL_HANDLE;

    bx::rdi::ConstantBuffer _cbuffer_instance_offset = {};
    bx::rdi::BufferRO _buffer_instance_world = {};
    bx::rdi::BufferRO _buffer_instance_world_it = {};
    
    bx::rdi::ResourceDescriptor _rdesc_frame_data;
    bx::rdi::ConstantBuffer _cbuffer_frame_data = {};
    struct FrameData
    {
        Matrix4 _view;
        Matrix4 _view_proj;
    } _frame_data;

    bx::rdi::ResourceDescriptor _rdesc_samplers;
    bx::rdi::Sampler _samp_point = {};
    bx::rdi::Sampler _samp_linear = {};
    bx::rdi::Sampler _samp_bilinear = {};
    bx::rdi::Sampler _samp_trilinear = {};


    //bx::gfx::RenderPass _render_pass = BX_GFX_NULL_HANDLE;
    //bx::gfx::Pipeline _pipeline_native_pos_nrm_solid = BX_GFX_NULL_HANDLE;
    //bx::gfx::Pipeline _pipeline_screenquad = BX_GFX_NULL_HANDLE;
    //bx::gfx::ResourceDescriptor _frame_data_rdesc = BX_GFX_NULL_HANDLE;
    //bx::gfx::ResourceDescriptor _instance_data_rdesc = BX_GFX_NULL_HANDLE;
    //bx::gfx::ResourceDescriptor _material_data_rdesc = BX_GFX_NULL_HANDLE;

    //bxGdiShaderFx* _native_shader_module = nullptr;
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