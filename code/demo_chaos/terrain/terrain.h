#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bx{ namespace terrain{

struct CreateInfo
{
    enum { NUM_LODS = 4 };
    
    // size of single quad
    f32 tile_width = 1.f;
    f32 tile_height = 1.f;

    // max divisions of single quad
    u32 max_tesselation_levels = 4;
    f32 visibility_radius = 8.0f;
};
    
struct Instance;
Instance* Create( const CreateInfo& info );
void      Destroy( Instance** i );

void Init( Instance* inst );
void Tick( Instance* inst, const Matrix4F& observer );

Vector3F ClosestPoint( const Instance* inst, const Vector3F& point );

}}//
