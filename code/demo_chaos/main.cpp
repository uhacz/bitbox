#include <system/application.h>
#include <system/window.h>

#include <engine/engine.h>

#include <util/time.h>
#include <util/handle_manager.h>
#include <util/config.h>
//#include <gfx/gfx_camera.h>
//#include <gfx/gfx_debug_draw.h>
#include <rdi/rdi.h>
#include <shaders/shaders/sys/binding_map.h>

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
        gfx::TextureManagerStartUp();


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

        //_shf_texutil = rdi::ShaderFileLoad( "shader/bin/texture_utils.shader", _engine.resource_manager );
        _shf_deffered = rdi::ShaderFileLoad( "shader/bin/deffered.shader", _engine.resource_manager );
        _shf_test = rdi::ShaderFileLoad( "shader/bin/test.shader", _engine.resource_manager );
        
        //{
        //    rdi::PipelineDesc pipeline_desc = {};
        //    pipeline_desc.Shader( _shf_texutil, "copy_rgba" );
        //    _pipeline_copy_texture_rgba = rdi::CreatePipeline( pipeline_desc );
        //}
        
        {
            rdi::PipelineDesc pipeline_desc = {};

            pipeline_desc.Shader( _shf_deffered, "geometry_notexture" );
            _pipeline_geometry_notex = rdi::CreatePipeline( pipeline_desc );

            pipeline_desc.Shader( _shf_deffered, "geometry_texture" );
            _pipeline_geometry_tex = rdi::CreatePipeline( pipeline_desc );
        }

        {
            rdi::PipelineDesc pipeline_desc = {};
            pipeline_desc.Shader( _shf_test, "color" );
            _pipeline_test_color = rdi::CreatePipeline( pipeline_desc );
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
        //{
        //    rdi::RenderTargetDesc rt_desc = {};
        //    rt_desc.Size( 1920, 1080 );
        //    rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );

        //    _rtarget_color = rdi::CreateRenderTarget( rt_desc );
        //}

        //{
        //    _cbuffer_frame_data = rdi::device::CreateConstantBuffer( sizeof( FrameData ) );
        //    _cbuffer_instance_offset = rdi::device::CreateConstantBuffer( sizeof( InstanceOffset ) );
        //    const u32 MAX_INSTANCES = 1024;
        //    _buffer_instance_world = rdi::device::CreateBufferRO( MAX_INSTANCES, rdi::Format( rdi::EDataType::FLOAT, 4 ), rdi::ECpuAccess::WRITE, rdi::EGpuAccess::READ );
        //    _buffer_instance_world_it = rdi::device::CreateBufferRO( MAX_INSTANCES, rdi::Format( rdi::EDataType::FLOAT, 3 ), rdi::ECpuAccess::WRITE, rdi::EGpuAccess::READ );

        //    rdi::ResourceBinding bindings[] =
        //    {
        //        rdi::ResourceBinding( "offset", rdi::EBindingType::UNIFORM ).StageMask( rdi::EStage::VERTEX ).Slot( SLOT_INSTANCE_OFFSET ),
        //        rdi::ResourceBinding( "world", rdi::EBindingType::READ_ONLY ).StageMask( rdi::EStage::VERTEX ).Slot( SLOT_INSTANCE_DATA_WORLD ),
        //        rdi::ResourceBinding( "world_it", rdi::EBindingType::READ_ONLY ).StageMask( rdi::EStage::VERTEX ).Slot( SLOT_INSTANCE_DATA_WORLD_IT ),
        //    };
        //    
        //    rdi::ResourceLayout layout = {};
        //    layout.bindings = bindings;
        //    layout.num_bindings = 1;
        //    _rdesc_instance_offset = rdi::CreateResourceDescriptor( layout );

        //    layout.bindings = &bindings[1];
        //    layout.num_bindings = 2;
        //    _rdesc_instance_data = rdi::CreateResourceDescriptor( layout );

        //    rdi::SetConstantBuffer( _rdesc_instance_offset, "offset", &_cbuffer_instance_offset );
        //    rdi::SetResourceRO( _rdesc_instance_data, "world", &_buffer_instance_world );
        //    rdi::SetResourceRO( _rdesc_instance_data, "world_it", &_buffer_instance_world_it );
        //}

        {
            gfx::VertexTransformDataInit( &_vertex_transform_data, 1024 );
        }

        {
            _cbuffer_frame_data = rdi::device::CreateConstantBuffer( sizeof( FrameData ) );
            rdi::ResourceBinding binding = rdi::ResourceBinding( "frame_data", rdi::EBindingType::UNIFORM ).Slot( SLOT_FRAME_DATA ).StageMask( rdi::EStage::ALL_STAGES_MASK );
            rdi::ResourceLayout layout = {};
            layout.bindings = &binding;
            layout.num_bindings = 1;
            _rdesc_frame_data = rdi::CreateResourceDescriptor( layout );
            rdi::SetConstantBuffer( _rdesc_frame_data, "frame_data", &_cbuffer_frame_data );
        }

        //{
        //    rdi::SamplerDesc samp_desc = {};
        //    
        //    samp_desc.Filter( rdi::ESamplerFilter::NEAREST );
        //    _samp_point = rdi::device::CreateSampler( samp_desc );

        //    samp_desc.Filter( rdi::ESamplerFilter::LINEAR );
        //    _samp_linear = rdi::device::CreateSampler( samp_desc );

        //    samp_desc.Filter( rdi::ESamplerFilter::BILINEAR_ANISO );
        //    _samp_bilinear = rdi::device::CreateSampler( samp_desc );

        //    samp_desc.Filter( rdi::ESamplerFilter::TRILINEAR_ANISO );
        //    _samp_trilinear = rdi::device::CreateSampler( samp_desc );

        //    rdi::ResourceBinding samp_bindings[] =
        //    {
        //        rdi::ResourceBinding( "point", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 0 ),
        //        rdi::ResourceBinding( "linear", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 1 ),
        //        rdi::ResourceBinding( "bilinear_aniso", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 2 ),
        //        rdi::ResourceBinding( "trilinear_aniso", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 3 ),
        //    };
        //    rdi::ResourceLayout resource_layout = {};
        //    resource_layout.bindings = samp_bindings;
        //    resource_layout.num_bindings = sizeof( samp_bindings ) / sizeof( *samp_bindings );
        //    _rdesc_samplers = rdi::CreateResourceDescriptor( resource_layout );

        //    rdi::SetSampler( _rdesc_samplers, "point", &_samp_point );
        //    rdi::SetSampler( _rdesc_samplers, "linear", &_samp_linear );
        //    rdi::SetSampler( _rdesc_samplers, "bilinear_aniso", &_samp_bilinear );
        //    rdi::SetSampler( _rdesc_samplers, "trilinear_aniso", &_samp_trilinear );
        //}

        //{
        //    const float vertices_pos[] =
        //    {
        //        -1.f, -1.f, 0.f,
        //        1.f , -1.f, 0.f,
        //        1.f , 1.f , 0.f,

        //        -1.f, -1.f, 0.f,
        //        1.f , 1.f , 0.f,
        //        -1.f, 1.f , 0.f,
        //    };
        //    const float vertices_uv[] =
        //    {
        //        0.f, 0.f,
        //        1.f, 0.f,
        //        1.f, 1.f,

        //        0.f, 0.f,
        //        1.f, 1.f,
        //        0.f, 1.f,
        //    };

        //    rdi::RenderSourceDesc rsource_desc = {};
        //    rsource_desc.Count( 6 );
        //    rsource_desc.VertexBuffer( rdi::VertexBufferDesc( rdi::EVertexSlot::POSITION ).DataType( rdi::EDataType::FLOAT, 3 ), vertices_pos );
        //    rsource_desc.VertexBuffer( rdi::VertexBufferDesc( rdi::EVertexSlot::TEXCOORD0 ).DataType( rdi::EDataType::FLOAT, 2 ), vertices_uv );
        //    _rsource_fullscreen_quad = rdi::CreateRenderSource( rsource_desc );
        //}

        //{//// poly shapes
        //    bxPolyShape polyShape;
        //    bxPolyShape_createBox( &polyShape, 1 );
        //    _rsource_box = rdi::CreateRenderSourceFromPolyShape( polyShape );
        //    bxPolyShape_deallocateShape( &polyShape );

        //    bxPolyShape_createShpere( &polyShape, 8 );
        //    _rsource_sphere = rdi::CreateRenderSourceFromPolyShape( polyShape );
        //    bxPolyShape_deallocateShape( &polyShape );
        //}

        _rsource_box = _renderer.GetSharedMesh().query( ":box" );
        _rsource_sphere = _renderer.GetSharedMesh().query( ":sphere" );

        _command_buffer = rdi::CreateCommandBuffer();


        _camera.world = Matrix4::translation( Vector3( 0.f, 0.f, 5.f ) );

        


        _box_instances[0] = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, PI / 2, 0.f ) ), Vector3( 2.f, 0.f, 0.f ) );
        _box_instances[1] = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, PI / 3, 0.f ) ), Vector3( 0.f, 0.f, 0.f ) );
        _box_instances[2] = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, PI / 4, 0.f ) ), Vector3(-2.f, 0.f, 0.f ) );

        _sph_instances[0] = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( 2.f, 2.f, 0.f ) );
        _sph_instances[1] = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( 0.f, 0.f, -2.f ) );
        _sph_instances[2] = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( -2.f,-2.f, 0.f ) );

        return true;
    }
    virtual void shutdown()
    {
        //rdi::DestroyRenderSource( &_rsource_sphere );
        //rdi::DestroyRenderSource( &_rsource_box );
        //rdi::DestroyRenderSource( &_rsource_fullscreen_quad );
        //rdi::DestroyResourceDescriptor( &_rdesc_samplers );
        //rdi::device::DestroySampler( &_samp_trilinear );
        //rdi::device::DestroySampler( &_samp_bilinear );
        //rdi::device::DestroySampler( &_samp_linear );
        //rdi::device::DestroySampler( &_samp_point );

        rdi::DestroyCommandBuffer( &_command_buffer );

        _renderer.ShutDown( _engine.resource_manager );

        gfx::VertexTransformDataDeinit( &_vertex_transform_data );
        //rdi::DestroyResourceDescriptor( &_rdesc_instance_data );
        //rdi::DestroyResourceDescriptor( &_rdesc_instance_offset );

        //rdi::device::DestroyBufferRO( &_buffer_instance_world_it );
        //rdi::device::DestroyBufferRO( &_buffer_instance_world );
        //rdi::device::DestroyConstantBuffer( &_cbuffer_instance_offset );
        rdi::DestroyResourceDescriptor( &_rdesc_frame_data );
        rdi::device::DestroyConstantBuffer( &_cbuffer_frame_data );
        //
        //rdi::DestroyRenderTarget( &_rtarget_color );
        rdi::DestroyRenderTarget( &_rtarget_gbuffer );

        rdi::DestroyPipeline( &_pipeline_test_color );
        rdi::DestroyPipeline( &_pipeline_geometry_tex );
        rdi::DestroyPipeline( &_pipeline_geometry_notex );
        //rdi::DestroyPipeline( &_pipeline_copy_texture_rgba );

        rdi::ShaderFileUnload( &_shf_test, _engine.resource_manager );
        rdi::ShaderFileUnload( &_shf_deffered, _engine.resource_manager );
        //rdi::ShaderFileUnload( &_shf_texutil, _engine.resource_manager );

        gfx::TextureManagerShutDown();
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
        rdi::Viewport screen_viewport = gfx::computeViewport( _camera, win->width, win->height, 1920, 1080 );

        rdi::CommandQueue* cmdq = nullptr;
        rdi::frame::Begin( &cmdq );
        _renderer.BeginFrame( cmdq );

        {
            FrameData fdata;
            fdata._view = _camera.view;
            fdata._view_proj = _camera.view_proj;

            rdi::context::UpdateCBuffer( cmdq, _cbuffer_frame_data, &fdata );
            rdi::BindResources( cmdq, _rdesc_frame_data );
        }

        _vertex_transform_data.Map( cmdq );
        u32 box_batch_index = _vertex_transform_data.AddBatch( _box_instances, NUM_INSTANCES );
        u32 sph_batch_index = _vertex_transform_data.AddBatch( _sph_instances, NUM_INSTANCES );
        _vertex_transform_data.Unmap( cmdq );
        _vertex_transform_data.Bind( cmdq );

        rdi::ClearRenderTarget( cmdq, _rtarget_gbuffer, 0.f, 0.f, 0.f, 0.f, 1.f );
        rdi::BindRenderTarget( cmdq, _rtarget_gbuffer, { 0 }, true );

        rdi::ClearCommandBuffer( _command_buffer );
        rdi::BeginCommandBuffer( _command_buffer );

        {
            rdi::Command* instance_cmd = _vertex_transform_data.SetCurrent( _command_buffer, box_batch_index, nullptr );

            rdi::SetPipelineCmd* pipeline_cmd = rdi::AllocateCommand<rdi::SetPipelineCmd>( _command_buffer, instance_cmd );
            pipeline_cmd->pipeline = _pipeline_test_color;

            rdi::DrawCmd* draw_cmd = rdi::AllocateCommand< rdi::DrawCmd >( _command_buffer, pipeline_cmd );
            draw_cmd->rsource = _rsource_box;
            draw_cmd->num_instances = NUM_INSTANCES;

            rdi::SubmitCommand( _command_buffer, instance_cmd, 0 );
        }

        {
            rdi::Command* instance_cmd = _vertex_transform_data.SetCurrent( _command_buffer, sph_batch_index, nullptr );

            rdi::SetPipelineCmd* pipeline_cmd = rdi::AllocateCommand<rdi::SetPipelineCmd>( _command_buffer, instance_cmd );
            pipeline_cmd->pipeline = _pipeline_test_color;

            rdi::DrawCmd* draw_cmd = rdi::AllocateCommand< rdi::DrawCmd >( _command_buffer, pipeline_cmd );
            draw_cmd->rsource = _rsource_sphere;
            draw_cmd->num_instances = NUM_INSTANCES;

            rdi::SubmitCommand( _command_buffer, instance_cmd, 0 );
        }

        rdi::EndCommandBuffer( _command_buffer );

        rdi::SubmitCommandBuffer( cmdq, _command_buffer );

        //rdi::BindPipeline( cmdq, _pipeline_test_color );
        //_vertex_transform_data.SetCurrent( cmdq, box_batch_index );
        //rdi::BindRenderSource( cmdq, _rsource_box );
        //rdi::SubmitRenderSourceInstanced( cmdq, _rsource_box, NUM_INSTANCES );

        //_vertex_transform_data.SetCurrent( cmdq, sph_batch_index );
        //rdi::BindRenderSource( cmdq, _rsource_sphere );
        //rdi::SubmitRenderSourceInstanced( cmdq, _rsource_sphere, NUM_INSTANCES );


        rdi::TextureRW texture = rdi::GetTexture( _rtarget_gbuffer, 0 );
        _renderer.RasterizeFramebuffer( cmdq, texture, _camera, win->width, win->height );

        //rdi::context::ChangeToMainFramebuffer( cmdq );
        //rdi::context::SetViewport( cmdq, screen_viewport );
        //rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_copy_texture_rgba );
        //rdi::SetResourceRO( rdesc, "gtexture", &rdi::GetTexture( _rtarget_color, 0 ) );
        //rdi::SetSampler( rdesc, "gsampler", &_samp_point );
        //rdi::BindPipeline( cmdq, _pipeline_copy_texture_rgba );
        //rdi::BindResources( cmdq, rdesc );
        //
        ///// draw fullscreen quad
        //rdi::BindRenderSource( cmdq, _rsource_fullscreen_quad );
        //rdi::SubmitRenderSource( cmdq, _rsource_fullscreen_quad );

        _renderer.EndFrame( cmdq );
        rdi::frame::End( &cmdq );
        
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
    gfx::Renderer _renderer;

    //rdi::ShaderFile* _shf_texutil = nullptr;
    //rdi::Pipeline _pipeline_copy_texture_rgba = BX_RDI_NULL_HANDLE;
    
    rdi::ShaderFile* _shf_deffered = nullptr;
    rdi::Pipeline _pipeline_geometry_notex = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_geometry_tex = BX_RDI_NULL_HANDLE;

    rdi::ShaderFile* _shf_test = nullptr;
    rdi::Pipeline _pipeline_test_color = BX_RDI_NULL_HANDLE;
    
    rdi::RenderTarget _rtarget_gbuffer = BX_RDI_NULL_HANDLE;
    //rdi::RenderTarget _rtarget_color = BX_RDI_NULL_HANDLE;


    gfx::VertexTransformData _vertex_transform_data;
    //rdi::ResourceDescriptor _rdesc_instance_offset = BX_RDI_NULL_HANDLE;
    //rdi::ResourceDescriptor _rdesc_instance_data = BX_RDI_NULL_HANDLE;
    //rdi::ConstantBuffer _cbuffer_instance_offset = {};
    //rdi::BufferRO _buffer_instance_world = {};
    //rdi::BufferRO _buffer_instance_world_it = {};
    
    rdi::ResourceDescriptor _rdesc_frame_data;
    rdi::ConstantBuffer _cbuffer_frame_data = {};
    struct FrameData
    {
        Matrix4 _view;
        Matrix4 _view_proj;
    } _frame_data;

    //rdi::ResourceDescriptor _rdesc_samplers;
    //rdi::Sampler _samp_point = {};
    //rdi::Sampler _samp_linear = {};
    //rdi::Sampler _samp_bilinear = {};
    //rdi::Sampler _samp_trilinear = {};

    //rdi::RenderSource _rsource_fullscreen_quad = BX_RDI_NULL_HANDLE;
    rdi::RenderSource _rsource_box = BX_RDI_NULL_HANDLE;
    rdi::RenderSource _rsource_sphere = BX_RDI_NULL_HANDLE;

    rdi::CommandBuffer _command_buffer = BX_RDI_NULL_HANDLE;

    gfx::Camera _camera = {};
    gfx::CameraInputContext _camera_input_ctx = {};

    gfx::MeshID _boxes;
    gfx::MeshID _spheres;

    //static const int NUM_INSTANCES = 3;
    //Matrix4 _box_instances[NUM_INSTANCES];
    //Matrix4 _sph_instances[NUM_INSTANCES];
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