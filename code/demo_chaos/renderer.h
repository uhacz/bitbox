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
    void PrepareScene( Scene scene );
    void Flush( rdi::CommandQueue* cmdq );

    static void _StartUp( GeometryPass** pass );
    static void _ShutDown( GeometryPass** pass );
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

class LightPass
{
public:
private:
    

};

struct PostProcessPass
{

};


//class Resources
//{
//public:
//    TextureHandle1 CreateTextureFromFile( const char* filename, const char* shortName );
//    void Destroy( TextureHandle1 h );
//    bool Alive( TextureHandle1 h ) const;
//    rdi::TextureRO Texture( TextureHandle1 h ) const;
//    
//    MaterialHandle CreateMaterialFromFile( const char* filename, const char* shortName );
//    void Destroy( MaterialHandle h );
//    bool Alive( MaterialHandle h ) const;
//    MaterialPipeline Pipeline( MaterialHandle h ) const;
//
//    MeshHandle CreateMesh( const char* shortName );
//    void Destroy( MeshHandle h );
//    bool Alive( MeshHandle h ) const;
//
//    void SetRenderSource( MeshHandle h, rdi::RenderSource rsource );
//    void SetLocalAABB( MeshHandle h, const bxAABB& aabb );
//
//    rdi::RenderSource RenderSource( MeshHandle h ) const;
//    bxAABB LocalAABB( MeshHandle h ) const;
//    
//private:
//    static inline TextureHandle1 MakeTextureHandle ( id_t id ) { TextureHandle1 h = { id.hash }; return h; }
//    static inline MaterialHandle MakeMaterialHandle( id_t id ) { MaterialHandle h = { id.hash }; return h; }
//    static inline MeshHandle     MakeMeshHandle    ( id_t id ) { MeshHandle     h = { id.hash }; return h; }
//
//    static const u32 MAX_TEXTURES = 128;
//    static const u32 MAX_MATERIALS = 64;
//    static const u32 MAX_MESHES = 256;
//
//    id_table_t<MAX_TEXTURES> _id_texture;
//    id_table_t<MAX_MATERIALS> _id_material;
//    id_table_t<MAX_MESHES> _id_mesh;
//
//    struct MaterialData
//    {
//        MaterialPipeline         _material_pipeline[MAX_MATERIALS] = {};
//        MaterialData             _data[MAX_MATERIALS] = {};
//        rdi::ConstantBuffer      _data_cbuffer[MAX_MATERIALS] = {};
//        MaterialTextureHandles   _textures[MAX_MATERIALS] = {};
//
//        rdi::Pipeline _pipeline_tex = BX_RDI_NULL_HANDLE;
//        rdi::Pipeline _pipeline_notex = BX_RDI_NULL_HANDLE;
//    };
//
//};

}}///

