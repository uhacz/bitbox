#include "flood_pbd_scene.h"
#include <util/debug.h>
#include <util/array.h>
#include <util/id_array.h>

namespace bx{ namespace flood{

    static inline id_t MakeInternalId( PBDActorId id )
    {
        return make_id( id.i );
    }
    static inline PBDActor MakeInvalidActor()
    {
        PBDActor a;
        a.begin = UINT32_MAX;
        a.count = UINT32_MAX;

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
        SYS_ASSERT( iid.index < _active_actors.size );
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

}

void PBDScene::UpdateVelocity( float deltaTime )
{

}

bool PBDScene::GetActorData( PBDActorData* data, PBDActorId id )
{

}

bool PBDScene::GetGridCell( PBDGridCell* cell, PBDActorId id )
{

}

bool PBDScene::GetGridCell( PBDGridCell* cell, const Vec3& point )
{

}



}
}//