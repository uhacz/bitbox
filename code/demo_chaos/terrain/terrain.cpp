#include "terrain.h"
#include "terrain_instance.h"

#include <util\debug.h>

namespace bx{ namespace terrain{ 

Instance* Create( const CreateInfo& info )
{
    Instance* inst = BX_NEW( bxDefaultAllocator(), Instance );

    inst->_info = info;
    SYS_ASSERT( info.tile_side_length > FLT_EPSILON );
    inst->_tile_side_length_inv = 1.f / info.tile_side_length;

    if( inst->_info.radius[0] < FLT_EPSILON )
    {
        inst->_info.radius[0] = 2.f;
        inst->_info.radius[1] = 4.f;
        inst->_info.radius[2] = 8.f;
        inst->_info.radius[3] = 16.f;
    }

    return inst;
}

void Destroy( Instance** i )
{
    BX_DELETE0( bxDefaultAllocator(), i[0] );
}

void Tick( Instance* inst, const Matrix4F& observer )
{
    inst->_observer_pos = observer.getTranslation();
    inst->Tick();
    inst->DebugDraw( 0xFFFF00FF );
    inst->Gui();
}

Vector3F ClosestPoint( const Instance* inst, const Vector3F& point )
{
    return Vector3F( 0.f );
}

}
}//