#pragma once

#include <rdi/rdi_backend.h>
#include "renderer_type.h"
#include "renderer_camera.h"
#include "renderer_material.h"
#include "renderer_shared_mesh.h"
#include "renderer_scene.h"

namespace bx{ namespace gfx{

struct RendererDesc
{
    u16 framebuffer_width = 1920;
    u16 framebuffer_height = 1080;
};

//////////////////////////////////////////////////////////////////////////
struct ActorHandleManager;
//////////////////////////////////////////////////////////////////////////
class Renderer
{
public:
    void StartUp( const RendererDesc& desc, ResourceManager* resourceManager );
    void ShutDown( ResourceManager* resourceManager );
    
    Scene CreateScene( const char* name );
    void DestroyScene( Scene* scene );

    void BeginFrame( rdi::CommandQueue* command_queue );
    void EndFrame( rdi::CommandQueue* command_queue );

    const RendererDesc& GetDesc() const { return _desc; }
    rdi::TextureRW GetFramebuffer() const { return rdi::GetTexture( _render_target, 0 ); }
    
    void RasterizeFramebuffer( rdi::CommandQueue* cmdq, const rdi::ResourceRO source, const Camera& camera, u32 windowW, u32 windowH );
    static void DrawFullScreenQuad( rdi::CommandQueue* cmdq );
    //SharedMeshContainer& GetSharedMesh() { return _shared_mesh; }
    
private:
    RendererDesc _desc = {};

    rdi::ShaderFile* _shf_texutil = nullptr;
    rdi::Pipeline _pipeline_copy_texture_rgba = BX_RDI_NULL_HANDLE;
    rdi::RenderTarget _render_target = BX_RDI_NULL_HANDLE;

    //SharedMeshContainer _shared_mesh = {};

    struct
    {
        rdi::ResourceDescriptor resource_desc = BX_RDI_NULL_HANDLE;
        rdi::Sampler point = {};
        rdi::Sampler linear = {};
        rdi::Sampler bilinear = {};
        rdi::Sampler trilinear = {};
    } _samplers;

    ////
    //
    ActorHandleManager* _actor_handle_manager = nullptr;
};

//////////////////////////////////////////////////////////////////////////
class GeometryPass
{
public:
    void PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera );
    void Flush( rdi::CommandQueue* cmdq );

    static void _StartUp( GeometryPass* pass );
    static void _ShutDown( GeometryPass* pass );

    rdi::RenderTarget GBuffer() const { return _rtarget_gbuffer; }

private:

    struct FrameData
    {
        Matrix4 _view;
        Matrix4 _view_proj;
    };

    rdi::RenderTarget _rtarget_gbuffer = BX_RDI_NULL_HANDLE;

    rdi::ResourceDescriptor _rdesc_frame_data;
    rdi::ConstantBuffer _cbuffer_frame_data = {};
    gfx::VertexTransformData _vertex_transform_data;
    rdi::CommandBuffer _command_buffer;
};

//////////////////////////////////////////////////////////////////////////

class SkyPass
{
public:
private:

};

class LightPass
{
public:
    void PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera );
    void Flush( rdi::CommandQueue* cmdq, rdi::TextureRW outputTexture, rdi::RenderTarget gbuffer );

    static void _StartUp( LightPass* pass );
    static void _ShutDown( LightPass* pass );

private:
    struct FrameData
    {
        float3_t camera_eye;
        f32 padd0_ = 0.f;
        float3_t camera_dir;
        f32 padd1_ = 0.f;
        float3_t sun_color;
        f32 sun_intensity;
        float3_t vs_sun_L;
        f32 padd2_ = 0.f;
    };

    rdi::Pipeline _pipeline = BX_RDI_NULL_HANDLE;
    rdi::ConstantBuffer _cbuffer_fdata = {};

};

//////////////////////////////////////////////////////////////////////////
class PostProcessPass
{
public:
    static void _StartUp( PostProcessPass* pass );
    static void _ShutDown( PostProcessPass* pass );

    void DoToneMapping( rdi::CommandQueue* cmdq, rdi::TextureRW outTexture, rdi::TextureRW inTexture, float deltaTime );

private:
    struct ToneMapping
    {
    #include <shaders/shaders/sys/tone_mapping_data.h>
        MaterialData data;

        rdi::TextureRW adapted_luminance[2] = {};
        rdi::TextureRW initial_luminance = {};
        rdi::Pipeline pipeline_luminance_map = BX_RDI_NULL_HANDLE;
        rdi::Pipeline pipeline_adapt_luminance = BX_RDI_NULL_HANDLE;
        rdi::Pipeline pipeline_composite = BX_RDI_NULL_HANDLE;
        rdi::ConstantBuffer cbuffer_data = {};
        u32 current_luminance_texture = 0;

        rdi::TextureRW CurrLuminanceTexture() { return adapted_luminance[current_luminance_texture]; }
        rdi::TextureRW PrevLuminanceTexture() { return adapted_luminance[!current_luminance_texture]; }

    }_tm;

};

}}///

