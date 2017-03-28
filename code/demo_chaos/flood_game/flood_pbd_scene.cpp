#include "flood_pbd_scene.h"
#include <util/debug.h>
#include <util/array.h>
#include <util/id_array.h>

namespace bx{ namespace flood{

    template< typename T >
    static inline id_t MakeInternalId( T id )
    {
        return make_id( id.i );
    }

PBDScene::PBDScene( float particleRadius )
    : _pt_radius( particleRadius )
    , _pt_radius_inv( 1.f / particleRadius )
{
    SYS_ASSERT( _pt_radius > FLT_EPSILON );
}

PBDActorId PBDScene::CreateDynamic( u32 numParticles )
{
    id_t iid = id_array::create( _id_array );
    PBDActorId id = { iid.hash };

    PBDActor actor = _AllocateActor( id, numParticles );

    return id;
}

void PBDScene::Destroy( PBDActorId id )
{
    _DeallocateActor( id );

    id_t iid = MakeInternalId( id );
    id_array::destroy( _id_array, iid );
}
PBDActor PBDScene::_AllocateActor( PBDActorId id, u32 numParticles )
{
    PBDActor actor = {};
    actor.begin = _x.size;
    actor.count = numParticles;

    for( u32 i = 0; i < numParticles; ++i )
    {
        array::push_back( _x, Vec3( 0.f ) );
        array::push_back( _p, Vec3( 0.f ) );
        array::push_back( _v, Vec3( 0.f ) );
        array::push_back( _w, 0.f );
        array::push_back( _vdamping, 0.1f );
        array::push_back( _grid_index, UINT32_MAX );
        array::push_back( _actor_id, id );
    }

    {
        id_t iid = MakeInternalId( id );
        _active_actors[iid.index] = actor;
    }

    return actor;
}

void PBDScene::_DeallocateActor( PBDActorId id )
{
    id_t iid = MakeInternalId( id );

    PBDActor actor = _active_actors[iid.index];
    _active_actors[iid.index] = PBDActor::Invalid();

    array::push_back( _free_actors, actor );

    PBDActorData data;
    GetActorData( &data, id );

    for( u32 i = actor.begin; i < actor.count; ++i )
    {
        SYS_ASSERT( _actor_id[i] == id );
        _actor_id[i] = PBDActorId::Invalid();
    }
}

void PBDScene::_Defragment()
{
    // TODO
}


void PBDScene::PredictPosition( const Vec3& gravity, float deltaTime )
{
    for( PBDActor actor : _active_actors )
    {
        const u32 begin = actor.begin;
        const u32 end = actor.end();

        for( u32 i = begin; i < end; ++i )
        {
            Vec3 v = _v[i] + gravity * deltaTime;
            v *= ::powf( 1.f - _vdamping[i], deltaTime );

            _p[i] += v * deltaTime;
            _v[i] = v;
        }
    }
}

void PBDScene::UpdateSpatialMap()
{
    const u32 spatial_map_size = ( _x.size * 3 ) / 2;
    hash_grid::Build( &_hash_grid, _grid_index.begin(), _x.begin(), _x.size, spatial_map_size, _pt_radius * 2.f );
}

void PBDScene::UpdateVelocity( float deltaTime )
{
    const float delta_time_inv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;

    for( PBDActor actor : _active_actors )
    {
        const u32 begin = actor.begin;
        const u32 end = actor.end();

        for( u32 i = begin; i < end; ++i )
        {
            const Vec3& p0 = _x[i];
            const Vec3& p1 = _p[i];

            _v[i] = ( p1 - p0 ) * delta_time_inv;
            _x[i] = p1;
        }
    }
}

bool PBDScene::GetActorData( PBDActorData* data, PBDActorId id )
{
    id_t iid = MakeInternalId( id );
    if( !id_array::has( _id_array, iid ) )
        return false;

    const PBDActor& actor = _active_actors[iid.index];

    data->x = _x.begin() + actor.begin;
    data->p = _p.begin() + actor.begin;
    data->v = _v.begin() + actor.begin;
    data->w = _w.begin() + actor.begin;
    data->count = actor.count;

    return true;
}

bool PBDScene::GetGridCell( HashGridStatic::Indices* pointIndices, u32 pointIndex )
{
    if( pointIndex >= _x.size )
        return false;

    const u32 grid_index = _grid_index[pointIndex];
    pointIndices[0] = _hash_grid.Get( grid_index );
    return true;
}

HashGridStatic::Indices PBDScene::GetGridCell( const Vec3& point )
{
    return _hash_grid.Lookup( point );
}

}
}//

#include <util/id_table.h>
#include <util/hashmap.h>
#include <util/buffer_utils.h>
namespace bx{ namespace flood{

PBDCloth::ActorId PBDClothSolver::CreateActor( const PBDCloth::ActorDesc& desc )
{
    PBDActorId scene_actor_id = _scene->CreateDynamic( desc.num_particles );

    const id_t iid = id_table::create( _id_container );
    u32 index = iid.index;

    PBDCloth::Range& cdistance_range = _range_cdistance[index];
    cdistance_range.begin = _cdistance.size;
    cdistance_range.count = desc.num_cdistance;

    PBDCloth::Range& cbending_range = _range_cbending[index];
    cbending_range.begin  = _cbending.size;
    cbending_range.count  = desc.num_cbending;
    
    _actor_id[index].i = iid.hash;

    {
        SYS_ASSERT( !hashmap::lookup( _scene_actor_map, iid.hash ) );
        hashmap_t::cell_t* map_cell = hashmap::insert( _scene_actor_map, iid.hash );
        map_cell->value = scene_actor_id.i;
    }
    {
        SYS_ASSERT( !hashmap::lookup( _cloth_actor_map, scene_actor_id.i ) );
        hashmap_t::cell_t* map_cell = hashmap::insert( _cloth_actor_map, scene_actor_id.i );
        map_cell->value = iid.hash;
    }

    PBDActorData scene_actor_data;
    bool bres = _scene->GetActorData( &scene_actor_data, scene_actor_id );
    SYS_ASSERT( bres == true );

    const float particle_mass_inv = ( desc.particle_mass > FLT_EPSILON ) ? 1.f / desc.particle_mass : 0.f;
    
    for( u32 i = 0; i < scene_actor_data.count; ++i )
    {
        scene_actor_data.x[i] = desc.positions[i];
        scene_actor_data.p[i] = desc.positions[i];
        scene_actor_data.v[i] = Vec3( 0.f );
        scene_actor_data.w[i] = particle_mass_inv;
    }

    return PBDCloth::ActorId::Make( iid.hash );
}

void PBDClothSolver::DestroyActor( PBDCloth::ActorId id )
{
    id_t iid = MakeInternalId( id );
    
    if( !id_table::has( _id_container, iid ) )
        return;



}

void PBDClothSolver::_Defragment()
{

}

void PBDClothSolver::SolveConstraints( u32 numIterations /*= 4 */ )
{

}

PBDActorId PBDClothSolver::SceneActorId( PBDCloth::ActorId id ) const
{
    const hashmap_t::cell_t* cell = hashmap::lookup( _scene_actor_map, id.i );
    return ( cell ) ? PBDActorId::Make( (u32)cell->value ) : PBDActorId::Invalid();
}

PBDCloth::ActorId PBDClothSolver::ClothActorId( PBDActorId id ) const
{
    const hashmap_t::cell_t* cell = hashmap::lookup( _cloth_actor_map, id.i );
    return ( cell ) ? PBDCloth::ActorId::Make( (u32)cell->value ) : PBDCloth::ActorId::Invalid();
}

}
}//