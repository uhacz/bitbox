#pragma once

#include <rdi/rdi_backend.h>
#include "renderer_type.h"
#include "renderer_camera.h"

namespace bx{
namespace gfx{

    struct MaterialDesc
    {
        float3_t diffuse_color = {0.5f, 0.5f, 0.5f};
        f32 diffuse = 0.1f;
        f32 specular = 0.5f;
        f32 roughness = 0.5f;
        f32 metallic = 0.f;
    };
    struct MaterialTextures
    {
        rdi::TextureRO diffuse_tex;
        rdi::TextureRO specular_tex;
        rdi::TextureRO roughness_tex;
        rdi::TextureRO metallic_tex;
    };

    namespace renderer
    {
        void startup();
        void shutdown();

        Material createMaterial( const char* name, const MaterialDesc& desc, const MaterialTextures* textures );
        void destroyMaterial( Material* m );
        Material findMaterial( const char* name );

        void addSharedRenderSource( const char* name, rdi::RenderSource rsource );
        rdi::RenderSource findSharedRenderSource( const char* name );

        Scene createScene( const char* name );
        void destroyScene( Scene* scene );
        void drawScene( Scene scene, const Camera& camera );

        MeshInstance createMeshInstance( Scene scene, unsigned numInstances, const char* name = nullptr );
        void destroyMeshInstance( MeshInstance* mi );
        void setRenderSource( MeshInstance mi, rdi::RenderSource rsource );
        void setMaterial( MeshInstance mi, Material m );
    }///

    namespace renderer_util
    {
        void copyTexture( const rdi::TextureRW* destination, const rdi::ResourceRO* source );
        void rasterizeFramebuffer( const rdi::ResourceRO& source );
    }///
}}///
