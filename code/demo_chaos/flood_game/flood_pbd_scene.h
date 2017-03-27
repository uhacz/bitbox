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
//     Do not store for future use.
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
    void SolveConstraints();
    void UpdateVelocity( float deltaTime );
    
    bool GetActorData( PBDActorData* data, PBDActorId id );
    bool GetGridCell( HashGridStatic::Indices* pointIndices, u32 pointIndex );
    HashGridStatic::Indices GetGridCell( const Vec3& point );

    enum { eMAX_ACTORS = 1024 };

    // --- particle data
    Vec3Array   _x;          // positions at the beginning of time step
    Vec3Array   _p;          // predicted positions
    Vec3Array   _v;          // velocities
    FloatArray  _w;          // inverse of mass
    FloatArray  _vdamping;   // velocity damping
    UintArray   _grid_index; // spatial grid index for fast lookup
    PBDActorIdArray _actor_id;// owner

                                // --- object data
    id_array_t<eMAX_ACTORS> _id_array;
    PBDActor                _active_actors[eMAX_ACTORS];
    PBDActorArray           _free_actors;

    // --- spatial
    HashGridStatic _hash_grid;

    const f32 _pt_radius;
    const f32 _pt_radius_inv;
};



}}//

namespace bx{ namespace flood{
//////////////////////////////////////////////////////////////////////////
// PBD cloth
namespace PBDCloth
{
    struct CDistance
    {
        u16 i0, i1;
        f32 rl; // rest length
    };
    struct CBending
    {
        u16 i0, i1, i2, i3;
        f32 ra; // rest angle
    };

    typedef array_t<CDistance> CDistanceArray;
    typedef array_t<CBending>  CBendingArray;

    struct ActorDesc
    {
        const Vec3* positions = nullptr;
        const u32* indices = nullptr;

        const u16* cdistance_indices = nullptr; // 2 indices per constraint
        const u16* cbending_indices = nullptr;  // 4 indices per constraint

        u32 num_particles = 0;
        u32 num_cdistance = 0;
        u32 num_cbending = 0;
    };

    struct Range
    {
        u32 begin;
        u32 count;
    };

    struct Actor
    {
        //CDistance* cdistance;
        //CBending*  cbending;
        PBDActorId pbd_actor;

        u32 begin_cdistance;
        u32 begin_cbending;
        u32 count_cdistance;
        u32 count_cbending;
    };

    struct ActorId : PBDActorId
    {};

    typedef array_t<Actor> ActorArray;
    typedef array_t<Actor*> ActorPtrArray;
    typedef array_t<ActorId> ActorIdArray;
};

struct PBDClothSolver
{
    PBDClothSolver( PBDScene* scene )
        : _scene( scene )
    {}

    PBDCloth::ActorId CreateActor( const PBDCloth::ActorDesc& desc );
    void DestroyActor( PBDCloth::ActorId id );

    void SolveConstraints( u32 numIterations = 4 );

    PBDScene* Scene() { return _scene; }

    PBDActorId SceneActorId( PBDCloth::ActorId id ) const;
    bool Has( PBDActorId id ) const;

    // --- shared arrays
    PBDCloth::CDistanceArray _cdistance;
    PBDCloth::CBendingArray _cbending;
    
    // --- per actor arrays
    array_t< PBDCloth::Range >      _range_cdistance;
    array_t< PBDCloth::Range >      _range_cbending;
    array_t< PBDCloth::ActorId >    _actor_id;
    hashmap_t                       _scene_actor_map;

    //PBDActorIdArray         _scene_actor_id;
    //PBDCloth::ActorIdArray  _cloth_actor_id;
    //PBDCloth::ActorPtrArray _actors;

    enum { eMAX_ACTORS = PBDScene::eMAX_ACTORS };

    //id_table_t<eMAX_ACTORS> _id_table;
    //PBDCloth::Actor*        _actors[eMAX_ACTORS] = {};
    //PBDCloth::ActorId       _actors_id[eMAX_ACTORS] = {};
    //


    id_array_t<eMAX_ACTORS> _id_array;
    array_t<u32>  _active_actor_indices;
    array_t<u32>  _free_actor_indices; // used only for defragmentation purposes

    PBDScene* _scene = nullptr;
};
}}//

