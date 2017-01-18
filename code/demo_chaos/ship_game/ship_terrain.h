#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bx{namespace ship{

struct Terrain
{
    u32 _num_samples_x = 0;
    u32 _num_samples_z = 0;
    u16* _samples = nullptr; // 0 -> 0xFFFF

    u32 _num_tiles_x = 10;
    u32 _num_tiles_z = 10;
    f32 _min_height = 0.f;
    f32 _max_height = 10.f;
    rdi::RenderSource* _rsources = nullptr;

    void CreateFromFile( const char* filename );
    void Destroy();

    float GetHeightAtPoint( const Vector3 wsPoint );
};

}}//
