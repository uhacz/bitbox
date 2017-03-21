#pragma once

#include <util/type.h>
#include <util/containers.h>
#include "flood_helpers.h"
#include "../spatial_hash_grid.h"

namespace bx{ namespace flood{

//////////////////////////////////////////////////////////////////////////
struct PBDActorId
{
    u32 i;

    bool IsValid() const { return i != 0; }
    static PBDActorId Invalid() { PBDActorId id = { 0 }; return id; }
};
inline bool operator == ( PBDActorId a, PBDActorId b ) { return a.i == b.i; }
typedef array_t<PBDActorId> PBDActorIdArray;

//////////////////////////////////////////////////////////////////////////
struct PBDActor
{
    u32 begin;
    u32 count;

    u32 end() const { return begin + count; }
    static inline PBDActor Invalid() { PBDActor a = { UINT32_MAX, UINT32_MAX }; return a; }
};
typedef array_t<PBDActor> PBDActorArray;

//////////////////////////////////////////////////////////////////////////
// --- This is temporary object use only in scope of single time step
//     Do not store.
struct PBDActorData
{
    Vec3* x;
    Vec3* p;
    Vec3* v;
    f32*  w;
    u32 count;
};

//////////////////////////////////////////////////////////////////////////
struct PBDGridCell
{
    const u32* indices;
    const PBDActorId* obj_ids;
};

//////////////////////////////////////////////////////////////////////////
struct PBDScene
{
    PBDScene( float particleRadius );

    PBDActorId CreateDynamic( u32 numParticles );
    void       Destroy( PBDActorId id );

    PBDActor _AllocateActor( PBDActorId id, u32 numParticles );
    void     _DeallocateActor( PBDActorId id );
    void     _Defragment();

    void PredictPosition( const Vec3& gravity, float deltaTime );
    void UpdateSpatialMap();
    void UpdateVelocity( float deltaTime );
    
    bool GetActorData( PBDActorData* data, PBDActorId id );
    bool GetGridCell( PBDGridCell* cell, PBDActorId id );
    bool GetGridCell( PBDGridCell* cell, const Vec3& point );

    enum { eMAX_OBJECTS = 1024 };

    // --- particle data
    Vec3Array   _x;          // positions at the beginning of time step
    Vec3Array   _p;          // predicted positions
    Vec3Array   _v;          // velocities
    FloatArray  _w;          // inverse of mass
    FloatArray  _vdamping;   // velocity damping
    UintArray   _grid_index; // spatial grid index for fast lookup
    PBDActorIdArray _actor_id;// owner

                                // --- object data
    id_array_t<eMAX_OBJECTS> _id_array;
    PBDActorArray            _active_actors;
    PBDActorArray            _free_actors;

    // --- spatial
    HashGridStatic _hash_grid;

    const f32 _pt_radius;
    const f32 _pt_radius_inv;
};

}}//

