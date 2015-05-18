#include "physics.h"
#include <util/array.h>

namespace bxPhysics
{
    enum EShape
    {
        eSHAPE_SPHERE = 0,
        eSHAPE_CAPSULE,
        eSHAPE_BOX,
        eSHAPE_PLANE,

        eSHAPE_COUNT,
    };
}///

struct bxPhysics_ShapeSphere
{
    f32 radius;
};
struct bxPhysics_ShapeBox
{
    f32 halfWidth;
    f32 halfHeight;
    f32 halfDepth;
};

struct bxPhysics_ShapeCapsule
{
    f32 radius;
    f32 halfHeight;
};

struct bxPhysics_ShapeData
{
    f32* shapeData[bxPhysics::eSHAPE_COUNT];
    i32 count[bxPhysics::eSHAPE_COUNT];
    i32 capacity[bxPhysics::eSHAPE_COUNT];
};

////
//
struct bxPhysics_ActorShapeEntry
{
    u32 type : 3;
    u32 nextIndex : 13;
    u32 dataIndex : 16;
};

struct bxPhysics_ActorData
{
    Vector3* pos0;
    Vector3* pos1;
    Quat* rot0;
    Quat* rot1;
    Vector3* linearVelocity;
    Vector3* angularVelocity;
    Vector3* acceleration;
    Vector3* momentum;
    
    f32* massInv;
    f32* linearDampingCoeff;
    f32* angularDampingCoeff;
    bxPhysics_ActorShapeEntry* shapes;


    i32 count;
    i32 capacity;
};

struct bxPhysics_Actor
{
      
};



struct bxPhysics_Context
{
    //// shapes
   

};

namespace bxPhysics
{
    void _Context_allocateShapeData( bxPhysics_Context* ctx, int shapeType, int newCapacity )
    {
    }


}///