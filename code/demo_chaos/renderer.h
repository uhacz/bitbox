#pragma once

#include <rdi/rdi_backend.h>
#include "renderer_type.h"
#include "renderer_camera.h"
#include "renderer_material.h"
#include "renderer_shared_mesh.h"
#include "renderer_scene.h"

namespace bx{ namespace gfx{

    struct MaterialDesc
    {
        float3_t diffuse_color = {0.5f, 0.5f, 0.5f};
        f32 diffuse = 0.1f;
        f32 specular = 0.5f;
        f32 roughness = 0.5f;
        f32 metallic = 0.f;
    };
    struct MaterialTextureNames
    {
        const char* diffuse_tex = nullptr;
        const char* specular_tex = nullptr;
        const char* roughness_tex = nullptr;
        const char* metallic_tex = nullptr;
    };
    
    typedef struct RendererImpl* Renderer;
    namespace renderer
    {
        Renderer startup();
        void shutdown( Renderer* rnd );

        MaterialID createMaterial( const char* name, const MaterialDesc& desc, const MaterialTextureNames* textures );
        void destroyMaterial( MaterialID* m );
        MaterialID findMaterial( const char* name );

        void addSharedRenderSource( const char* name, rdi::RenderSource rsource );
        rdi::RenderSource findSharedRenderSource( const char* name );

        Scene createScene( const char* name );
        void destroyScene( Scene* scene );
        void drawScene( Scene scene, const Camera& camera );

        MeshID createMeshInstance( Scene scene, unsigned numInstances, const char* name = nullptr );
        void destroyMeshInstance( MeshID* mi );
        void setRenderSource( MeshID mi, rdi::RenderSource rsource );
        void setMaterial( MeshID mi, MaterialID m );
    }///

    namespace renderer_util
    {
        void copyTexture( const rdi::TextureRW* destination, const rdi::ResourceRO* source );
        void rasterizeFramebuffer( const rdi::ResourceRO* source );
    }///
}}///

namespace bx{ namespace gfx{

struct RendererStartUpInfo
{
    u16 framebuffer_width = 1920;
    u16 framebuffer_height = 1080;
};

class Renderer
{
public:
    void StartUp( const RendererStartUpInfo& info );
    void ShutDown();
    
    MaterialID CreateMaterial( const char* name, const MaterialDesc& desc, const MaterialTextureNames* textures );
    void DestroyMaterial( MaterialID id );
    MaterialID FindMaterial( const char* name );



private:



};

}}///

