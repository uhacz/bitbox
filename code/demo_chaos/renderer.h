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

namespace EFramebuffer
{
    enum Enum
    {
        COLOR,
        SWAP,
    };
}//

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
    rdi::TextureRW GetFramebuffer( EFramebuffer::Enum index ) const { return rdi::GetTexture( _render_target, index ); }
    
    void RasterizeFramebuffer( rdi::CommandQueue* cmdq, const rdi::ResourceRO source, const Camera& camera, u32 windowW, u32 windowH );
    static void DrawFullScreenQuad( rdi::CommandQueue* cmdq );
    static void DebugDraw( rdi::CommandQueue* cmdq, rdi::TextureRW targetColor, rdi::TextureDepth targetDepth, const Camera& camera );
    
private:
    RendererDesc _desc = {};

    rdi::Pipeline _pipeline_copy_texture_rgba = BX_RDI_NULL_HANDLE;
    rdi::RenderTarget _render_target = BX_RDI_NULL_HANDLE;

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
namespace EGBuffer
{
    enum E : u8
    {
        ALBEDO_SPEC = 0,
        WPOS_ROUGH,
        WNRM_METAL,
    };
}
class GeometryPass
{
public:
    void PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera );
    void Flush( rdi::CommandQueue* cmdq );

    static void _StartUp( GeometryPass* pass, const RendererDesc& rndDesc );
    static void _ShutDown( GeometryPass* pass );

    rdi::RenderTarget GBuffer() const { return _rtarget_gbuffer; }

private:

    struct FrameData
    {
        Matrix4 _camera_world;
        Matrix4 _view;
        Matrix4 _view_proj;
    };

    rdi::RenderTarget _rtarget_gbuffer = BX_RDI_NULL_HANDLE;

    rdi::ResourceDescriptor  _rdesc_frame_data;
    rdi::ConstantBuffer      _cbuffer_frame_data = {};
    rdi::BufferRO            _particle_buffer = {};
    gfx::VertexTransformData _vertex_transform_data;
    rdi::CommandBuffer       _command_buffer;
};

//////////////////////////////////////////////////////////////////////////
class ShadowPass
{
public:
    struct LightMatrices
    {
        Matrix4 world;
        Matrix4 view;
        Matrix4 proj;
    };

public:
    bool PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera );
    void Flush( rdi::CommandQueue* cmdq, rdi::TextureDepth sceneDepthTex, rdi::ResourceRO sceneNormalsTex );


    static void _StartUp ( ShadowPass* pass, const RendererDesc& rndDesc, u32 shadowMapSize = 2048 );
    static void _ShutDown( ShadowPass* pass );

    rdi::TextureDepth DepthMap () const { return _depth_map; }
    rdi::TextureRW    ShadowMap() const { return _shadow_map; }
    
    const LightMatrices& GetMatrices() const { return _matrices; }

private:
    void _ComputeLightMatrixOrtho( LightMatrices* matrices, const Vector3 wsFrustumCorners[8], const Vector3 wsLightDirection );

private:
    LightMatrices _matrices;

#include <shaders/shaders/shadow_data.h>
    rdi::TextureDepth _depth_map      = {};
    rdi::TextureRW    _shadow_map     = {};
    rdi::Sampler      _sampler_shadow = {};

    rdi::Pipeline _pipeline_depth   = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_resolve = BX_RDI_NULL_HANDLE;

    rdi::ConstantBuffer      _cbuffer    = {};
    gfx::VertexTransformData _vertex_transform_data;
    rdi::CommandBuffer       _cmd_buffer = BX_RDI_NULL_HANDLE;
};

//////////////////////////////////////////////////////////////////////////
class SsaoPass
{
public:
    void PrepareScene( rdi::CommandQueue* cmdq, const Camera& camera );
    void Flush( rdi::CommandQueue* cmdq, rdi::TextureDepth depthTexture, rdi::ResourceRW normalsTexture );

    rdi::TextureRW SsaoTexture() const { return _ssao_texture; }

    static void _StartUp( SsaoPass* pass, const RendererDesc& rndDesc, bool halfRes = true );
    static void _ShutDown( SsaoPass* pass );

private:
#include <shaders/shaders/ssao_data.h>
    rdi::TextureRW _ssao_texture = {};
    rdi::TextureRW _temp_texture = {};
    rdi::ConstantBuffer _cbuffer_mdata = {};

    rdi::Pipeline _pipeline_ssao = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_blurx = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_blury = BX_RDI_NULL_HANDLE;

    bool _halfRes = false;
};

//////////////////////////////////////////////////////////////////////////
class LightPass
{
public:
    void PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera );
    void Flush( rdi::CommandQueue* cmdq, rdi::TextureRW outputTexture, rdi::RenderTarget gbuffer, rdi::ResourceRO shadowMap, rdi::ResourceRO ssaoMap );

    static void _StartUp( LightPass* pass );
    static void _ShutDown( LightPass* pass );

private:
#include <shaders/shaders/deffered_lighting_data.h>
    rdi::Pipeline _pipeline = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_skybox = BX_RDI_NULL_HANDLE;
    rdi::ConstantBuffer _cbuffer_fdata = {};

    u8 _has_skybox = 0;

    //TextureHandle _sky_cubemap = {};
};

//////////////////////////////////////////////////////////////////////////
class PostProcessPass
{
public:
    static void _StartUp( PostProcessPass* pass );
    static void _ShutDown( PostProcessPass* pass );

    void DoToneMapping( rdi::CommandQueue* cmdq, rdi::TextureRW outTexture, rdi::TextureRW inTexture, float deltaTime );


    struct ToneMapping
    {
    #include <shaders/shaders/tone_mapping_data.h>
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

