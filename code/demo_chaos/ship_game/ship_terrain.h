#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/filesystem.h>
#include <util/containers.h>
#include <rdi/rdi_backend.h>

#include "../renderer_type.h"


namespace bx{namespace ship{

struct Terrain
{
    bxFS::File _file = {};
    u32 _num_samples_x = 0;
    u32 _num_samples_z = 0;
    f32* _samples = nullptr;
    f32 _sample_scale_xz = 1.0f;
    f32 _sample_scale_y = 100.f;
    
    u32 _num_tiles_x = 16;
    u32 _num_tiles_z = 16;
    
    array_t<float3_t> _debug_points;
    array_t<float3_t> _debug_normals;

    array_t<rdi::RenderSource> _rsources = {};
    array_t<gfx::ActorID > _actors = {};
    rdi::IndexBuffer _index_buffer_tile = {};

    void CreateFromFile( const char* filename, gfx::Scene scene );
    void Destroy();

    float GetHeightAtPoint( const Vector3 wsPoint );

    void DebugDraw( u32 color );
    
};

}}//
