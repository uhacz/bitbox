#pragma once

#include <util/type.h>
#include <util/containers.h>
#include "flood_helpers.h"
#include "../spatial_hash_grid.h"

namespace bx{ namespace flood{

//////////////////////////////////////////////////////////////////////////

template< typename T >
struct TPBDActorId
{
    T i;

    bool IsValid() const { return i != 0; }
    static TPBDActorId<T> Make( u32 hash ) { TPBDActorId<T> id = { hash }; return id; }
    static TPBDActorId<T> Invalid() { return Make( 0 ); }
};

typedef TPBDActorId<u32> PBDActorId;
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
        
        const u16* cdistance_indices = nullptr; // 2 indices per constraint
        const u16* cbending_indices = nullptr;  // 4 indices per constraint

        u32 num_particles = 0;
        u32 num_cdistance_indices = 0;
        u32 num_cbending_indices = 0;

        f32 particle_mass = 1.f;
    };

    struct Range
    {
        u32 begin = 0;
        u32 count = 0;

        u32 end() const { return begin + count; }
    };

    //struct Actor
    //{
    //    PBDActorId pbd_actor;

    //    u32 begin_cdistance;
    //    u32 begin_cbending;
    //    u32 count_cdistance;
    //    u32 count_cbending;
    //};
    typedef TPBDActorId<u32> ActorId;
    //typedef array_t<Actor> ActorArray;
    //typedef array_t<Actor*> ActorPtrArray;
    typedef array_t<ActorId> ActorIdArray;
};

struct PBDClothSolver
{
    PBDClothSolver( PBDScene* scene )
        : _scene( scene )
    {}

    PBDCloth::ActorId CreateActor( const PBDCloth::ActorDesc& desc );
    void DestroyActor( PBDCloth::ActorId id );
    void _Defragment();

    void SolveConstraints( u32 numIterations = 4 );
    
    PBDScene*         Scene        ()                              { return _scene; }
    PBDActorId        SceneActorId ( PBDCloth::ActorId id ) const;
    PBDCloth::ActorId ClothActorId ( PBDActorId id )        const;

    // --- shared arrays
    PBDCloth::CDistanceArray _cdistance;
    PBDCloth::CBendingArray  _cbending;
    
    // --- per actor arrays
    enum { eMAX_ACTORS = PBDScene::eMAX_ACTORS };
    PBDCloth::Range      _range_cdistance[eMAX_ACTORS] = {};
    PBDCloth::Range      _range_cbending [eMAX_ACTORS] = {};
    PBDCloth::ActorId    _actor_id       [eMAX_ACTORS] = {};
    PBDActorId           _scene_actor_id [eMAX_ACTORS] = {};
    hashmap_t                       _scene_actor_map;
    hashmap_t                       _cloth_actor_map;

    id_table_t<eMAX_ACTORS> _id_container;
    array_t<PBDCloth::ActorId>  _active_actor_indices;
    array_t<PBDCloth::ActorId>  _free_actor_indices; // used only for defragmentation purposes


    //PBDActorIdArray         _scene_actor_id;
    //PBDCloth::ActorIdArray  _cloth_actor_id;
    //PBDCloth::ActorPtrArray _actors;


    //id_table_t<eMAX_ACTORS> _id_table;
    //PBDCloth::Actor*        _actors[eMAX_ACTORS] = {};
    //PBDCloth::ActorId       _actors_id[eMAX_ACTORS] = {};
    //


    
    PBDScene* _scene = nullptr;
};
}}//

