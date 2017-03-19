#pragma once

#include <util/type.h>
#include <util/containers.h>
#include "flood_helpers.h"
#include "../spatial_hash_grid.h"

namespace bx{ namespace flood{




//////////////////////////////////////////////////////////////////////////
struct RigidBodyId
{
    u32 id;
};

struct RigidBodyData
{
    Vec3* x;
    Vec3* p;
    Vec3* v;
};

struct RigidBody
{
    void SetParticleRadius( float value );

    RigidBodyId Create( u32 numParticles );
    void BuildSpatialMap();

    

    Vec3Array _x;
    Vec3Array _p;
    Vec3Array _v;

    Vec3Array _xr; // rest positions
};
}}//
