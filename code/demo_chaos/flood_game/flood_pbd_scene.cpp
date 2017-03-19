#include "flood_pbd_scene.h"
#include <util\debug.h>

namespace bx{ namespace flood{

PBDScene::PBDScene( float particleRadius )
    : _pt_radius( particleRadius )
    , _pt_radius_inv( 1.f / particleRadius )
{
    SYS_ASSERT( _pt_radius > FLT_EPSILON );
}

PBDActorId PBDScene::CreateStatic( u32 numParticles )
{

}

PBDActorId PBDScene::CreateDynamic( u32 numParticles )
{

}

void PBDScene::Destroy( PBDActorId id )
{

}

PBDActor PBDScene::_AllocateObject( u32 numParticles )
{

}

void PBDScene::_DeallocateActor( PBDActor obj )
{

}

void PBDScene::_Defragment()
{

}

}
}//