#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/filesystem.h>
#include <rdi/rdi_type.h>

namespace bx{namespace ship{

struct Terrain
{
    bxFS::File _file = {};
    u32 _num_samples_x = 0;
    u32 _num_samples_z = 0;
    f32* _samples = nullptr;
    f32 _sample_scale_xz = 1.f;
    f32 _sample_scale_y = 1.f;

    u32 _num_tiles_x = 32;
    u32 _num_tiles_z = 32;
    
    rdi::RenderSource* _rsources = nullptr;

    void CreateFromFile( const char* filename );
    void Destroy();

    float GetHeightAtPoint( const Vector3 wsPoint );
    
};

}}//
