#include "terrain.h"
#include <util\array.h>
#include <rdi\rdi_debug_draw.h>

namespace bx{ namespace terrain{ 

typedef array_t<Vector3F> Vector3FArray;
typedef array_t<float2_t> Float2FArray;

inline u32 GetFlatIndex( u32 x, u32 y, u32 width )
{
    return y * width + x;
}

struct Instance
{
    CreateInfo _info = {};

    Vector3FArray _tiles_world_offsets;
    Vector3F _observer_pos  { 0.f };
    Vector3F _up_axis       { 0.f, 1.f, 0.f };

    void DebugDraw( u32 color )
    {
        const Vector3F ext = Vector3F( _info.tile_side_length, 0.2f, _info.tile_side_length ) * 0.5f;
        for( const Vector3F& pos : _tiles_world_offsets )
        {
            rdi::debug_draw::AddBox( Matrix4F::translation( pos ), ext, color, 1 );
        }
    }

    void Tick( )
    {
        // initialization
        if( array::empty( _tiles_world_offsets ) )
        {
            const f32 max_radius = _info.radius[CreateInfo::NUM_LODS - 1];
            const f32 side_length = _info.tile_side_length;
            
            u32 num_tiles_per_side = ( u32 )::ceil( max_radius / side_length );

            array::reserve( _tiles_world_offsets, num_tiles_per_side * num_tiles_per_side );

            const f32 off = max_radius * 0.5f;
            const Vector3F zero_offset( -off, 0.f, -off );
            const Vector3F observer_offset = projectVectorOnPlane( _observer_pos, _up_axis );

            for( u32 iz = 0; iz < num_tiles_per_side; ++iz )
            {
                for( u32 ix = 0; ix < num_tiles_per_side; ++ix )
                {
                    const float x = (f32)ix;
                    const float y = 0.f;
                    const float z = (f32)iz;
                    const Vector3F pos( x, y, z );
                    const Vector3F final_pos = pos + zero_offset + observer_offset;
                    array::push_back( _tiles_world_offsets, final_pos );
                }
            }
        }



    }

};

Instance* Create( const CreateInfo& info )
{
    Instance* inst = BX_NEW( bxDefaultAllocator(), Instance );

    inst->_info = info;
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
}

Vector3F ClosestPoint( const Instance* inst, const Vector3F& point )
{
    return Vector3F( 0.f );
}

}
}//