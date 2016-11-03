#pragma once

#include <rdi/rdi_backend.h>
#include "renderer_type.h"
#include "renderer_camera.h"
#include "renderer_material.h"
#include "renderer_shared_mesh.h"
#include "renderer_scene.h"

//namespace bx{ namespace gfx{
//
//    struct MaterialDesc
//    {
//        float3_t diffuse_color = {0.5f, 0.5f, 0.5f};
//        f32 diffuse = 0.1f;
//        f32 specular = 0.5f;
//        f32 roughness = 0.5f;
//        f32 metallic = 0.f;
//    };
//    struct MaterialTextureNames
//    {
//        const char* diffuse_tex = nullptr;
//        const char* specular_tex = nullptr;
//        const char* roughness_tex = nullptr;
//        const char* metallic_tex = nullptr;
//    };
//    
//    typedef struct RendererImpl* Renderer;
//    namespace renderer
//    {
//        Renderer startup();
//        void shutdown( Renderer* rnd );
//
//        MaterialID createMaterial( const char* name, const MaterialDesc& desc, const MaterialTextureNames* textures );
//        void destroyMaterial( MaterialID* m );
//        MaterialID findMaterial( const char* name );
//
//        void addSharedRenderSource( const char* name, rdi::RenderSource rsource );
//        rdi::RenderSource findSharedRenderSource( const char* name );
//
//        Scene createScene( const char* name );
//        void destroyScene( Scene* scene );
//        void drawScene( Scene scene, const Camera& camera );
//
//        MeshID createMeshInstance( Scene scene, unsigned numInstances, const char* name = nullptr );
//        void destroyMeshInstance( MeshID* mi );
//        void setRenderSource( MeshID mi, rdi::RenderSource rsource );
//        void setMaterial( MeshID mi, MaterialID m );
//    }///
//
//    namespace renderer_util
//    {
//        void copyTexture( const rdi::TextureRW* destination, const rdi::ResourceRO* source );
//        void rasterizeFramebuffer( const rdi::ResourceRO* source );
//    }///
//}}///

namespace bx{ namespace gfx{

struct RendererDesc
{
    u16 framebuffer_width = 1920;
    u16 framebuffer_height = 1080;
};

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

    SharedMeshContainer& GetSharedMesh() { return _shared_mesh; }
    
private:
    RendererDesc _desc = {};

    rdi::ShaderFile* _shf_texutil = nullptr;
    rdi::Pipeline _pipeline_copy_texture_rgba = BX_RDI_NULL_HANDLE;
    rdi::RenderTarget _render_target = BX_RDI_NULL_HANDLE;

    SharedMeshContainer _shared_mesh = {};

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
};

//////////////////////////////////////////////////////////////////////////
struct GeometryPass
{
    struct FrameData
    {
        Matrix4 _view;
        Matrix4 _view_proj;
    };
    
    rdi::RenderTarget _rtarget_gbuffer = BX_RDI_NULL_HANDLE;

    rdi::ShaderFile* _shf_deffered = nullptr;
    rdi::Pipeline _pipeline_geometry_notex = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_geometry_tex = BX_RDI_NULL_HANDLE;

    rdi::ResourceDescriptor _rdesc_frame_data;
    rdi::ConstantBuffer _cbuffer_frame_data = {};
        
    gfx::VertexTransformData _vertex_transform_data;
};

struct PostProcessPass
{

};


}}///

