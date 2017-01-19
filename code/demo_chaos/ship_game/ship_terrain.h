#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bx{namespace ship{

struct Terrain
{
    u32 _num_samples_x = 0;
    u32 _num_samples_z = 0;
    u16* _samples = nullptr; // 0 -> 0xFFFF
    f32 _sample_scale_xz = 1.f;
    f32 _sample_scale_y = 1.f;

    u32 _num_tiles_x = 10;
    u32 _num_tiles_z = 10;
    
    rdi::RenderSource* _rsources = nullptr;

    void CreateFromFile( const char* filename );
    void Destroy();

    float GetHeightAtPoint( const Vector3 wsPoint );
    
};

}}//
