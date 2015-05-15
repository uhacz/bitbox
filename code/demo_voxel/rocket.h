#pragma once

#include <voxel/voxel_type.h>
#include <util/vectormath/vectormath.h>

struct bxRocket_DynamicState
{
    Vector3 force;
    Vector3 torque;

    Vector3 acceleration;
    Vector3 momentum;

    Vector3 velocity_linear;
    Vector3 velocity_angular;

    Vector3 position;
    Quat    rotation;
};

struct bxRocekt_Controller
{
    
};

struct bxRocket_Entity
{
    bxRocket_DynamicState state0;
    
    
    bxVoxel_ObjectId _vxId;
};