#include "terrain_instance.h"
#include <util/array.h>
#include <rdi\rdi_debug_draw.h>

#include "../imgui/imgui.h"

namespace bx{ namespace terrain{

void Instance::DebugDraw( u32 color )
{
    const Vector3F ext = Vector3F( _info.tile_side_length, 0.2f, _info.tile_side_length ) * 0.5f;
    for( const Vector3F& pos : _tiles_world_pos )
    {
        rdi::debug_draw::AddBox( Matrix4F::translation( pos ), ext, color, 1 );
    }
}

void Instance::Gui()
{
    if( ImGui::Begin( "terrain" ) )
    {
        ImGui::Text( "grid center pos: %d, %d", _grid_center_pos.x, _grid_center_pos.y );
        ImGui::Text( "grid observer pos: %d, %d", _grid_observer_pos.x, _grid_observer_pos.y );
        ImGui::Text( "data center: %u, %u", _data_center.x, _data_center.y );
    }
    ImGui::End();
}

void Instance::_Init()
{
    const f32 side_length = _info.tile_side_length;
    const f32 max_radius = _info.radius[CreateInfo::NUM_LODS - 1] * side_length;

    u32 num_tiles_per_side = ( u32 )::ceil( max_radius / side_length );
    num_tiles_per_side += ( num_tiles_per_side % 2 ) ? 0 : 1;
    _num_tiles_per_side = num_tiles_per_side;
    _num_tiles_in_radius = num_tiles_per_side / 2;

    array::reserve( _tiles_world_pos, num_tiles_per_side * num_tiles_per_side );
    array::reserve( _tiles_flags, _tiles_world_pos.capacity );

    _grid_center_pos = _ComputePosOnGrid( _observer_pos );
    _grid_observer_pos = _grid_center_pos;
    _data_center = Vector2UI( _num_tiles_in_radius, _num_tiles_in_radius );

    const f32 off = max_radius * 0.5f;
    const Vector3F zero_offset = Vector3F( -off, 0.f, -off );
    const Vector3F observer_offset = toVector3Fxz( _grid_observer_pos ) * side_length;// projectVectorOnPlane( _observer_pos, _up_axis );

    for( u32 iz = 0; iz < num_tiles_per_side; ++iz )
    {
        for( u32 ix = 0; ix < num_tiles_per_side; ++ix )
        {
            const float x = (f32)ix;
            const float y = 0.f;
            const float z = (f32)iz;
            const Vector3F pos( x, y, z );
            const Vector3F final_pos = ( pos * side_length ) + zero_offset + observer_offset;
            array::push_back( _tiles_world_pos, final_pos );
            array::push_back( _tiles_flags, u8( 0 ) );
        }
    }
}

namespace
{
    inline u32 _Increment( u32 value, u32 lastIndex, int direction )
    {
        return ( direction > 0 )
            ? wrap_inc_u32( value, 0, lastIndex )
            : wrap_dec_u32( value, 0, lastIndex );
    }
}

void Instance::_ComputeTiles()
{
    const Vector2I new_grid_observer_pos = _ComputePosOnGrid( _observer_pos );
    const Vector2I delta = maxPerElem( -Vector2I( _num_tiles_in_radius ), minPerElem( Vector2I( _num_tiles_in_radius ), new_grid_observer_pos - _grid_center_pos ) );
    const u32 last_index = _num_tiles_per_side - 1;

    // determine new data center coords
    if( delta.x > 0 )
    {
        for( int i = 0; i < delta.x; ++i )
            _data_center.x = wrap_inc_u32( _data_center.x, 0, last_index );
    }
    else if( delta.x < 0 )
    {
        for( int i = delta.x; i < 0; ++i )
            _data_center.x = wrap_dec_u32( _data_center.x, 0, last_index );
    }
    if( delta.y > 0 )
    {
        for( int i = 0; i < delta.y; ++i )
            _data_center.y = wrap_inc_u32( _data_center.y, 0, last_index );
    }
    else if( delta.y < 0 )
    {
        for( int i = delta.y; i < 0; ++i )
            _data_center.y = wrap_dec_u32( _data_center.y, 0, last_index );
    }

    const Vector2I delta_abs = absPerElem( delta );
    const Vector2I n_jumps = maxPerElem( Vector2I( 1 ), Vector2I( _num_tiles_in_radius ) - delta_abs );
    const i32 radius = (i32)_num_tiles_in_radius;

    if( delta.x )
    {
        u32 data_col_begin = _Increment( _data_center.x, last_index, delta.x );
        i32 xoffset = 1;
        for( i32 j = 0; j < n_jumps.x; ++j, ++xoffset )
            data_col_begin = _Increment( data_col_begin, last_index, delta.x );

        if( delta.x < 0 )
            xoffset = -xoffset;

        u32 data_row = _data_center.y;
        for( i32 i = 0; i < radius; ++i )
            data_row = wrap_dec_u32( data_row, 0, last_index );

        for( i32 zoffset = -radius; zoffset <= radius; ++zoffset )
        {
            u32 data_col = data_col_begin;
            for( i32 i = 0; i < delta_abs.x; ++i )
            {
                u32 data_index = GetFlatIndex( Vector2UI( data_col, data_row ), _num_tiles_per_side );
                SYS_ASSERT( data_index < _tiles_world_pos.size );

                Vector2I grid_pos = new_grid_observer_pos + Vector2I( xoffset, zoffset );
                Vector3F world_pos = toVector3Fxz( grid_pos ) * _info.tile_side_length;
                _tiles_world_pos[data_index] = world_pos;
                _tiles_flags[data_index] |= ETileFlag::NEED_REBUILD;

                data_col = _Increment( data_col, last_index, delta.x );
            }
            data_row = wrap_inc_u32( data_row, 0, last_index );
        }

    }
    if( delta.y )
    {
        u32 data_row_begin = _Increment( _data_center.y, last_index, delta.y );
        i32 zoffset = 1;
        for( i32 j = 0; j < n_jumps.y; ++j, ++zoffset )
            data_row_begin = _Increment( data_row_begin, last_index, delta.y );

        if( delta.y < 0 )
            zoffset = -zoffset;

        u32 data_col = _data_center.x;
        for( i32 i = 0; i < radius; ++i )
            data_col = wrap_dec_u32( data_col, 0, last_index );

        for( i32 xoffset = -radius; xoffset <= radius; ++xoffset )
        {
            u32 data_row = data_row_begin;
            for( i32 i = 0; i < delta_abs.y; ++i )
            {
                u32 data_index = GetFlatIndex( Vector2UI( data_col, data_row ), _num_tiles_per_side );
                SYS_ASSERT( data_index < _tiles_world_pos.size );
                if( _tiles_flags[data_index] & ETileFlag::NEED_REBUILD )
                    continue;

                Vector2I grid_pos = new_grid_observer_pos + Vector2I( xoffset, zoffset );
                Vector3F world_pos = toVector3Fxz( grid_pos ) * _info.tile_side_length;
                _tiles_world_pos[data_index] = world_pos;
                _tiles_flags[data_index] |= ETileFlag::NEED_REBUILD;

                data_row = _Increment( data_row, last_index, delta.y );
            }
            data_col = wrap_inc_u32( data_col, 0, last_index );
        }
    }

    _grid_observer_pos = new_grid_observer_pos;
    _grid_center_pos = _grid_observer_pos;
}

void Instance::Tick()
{
    // initialization
    if( array::empty( _tiles_world_pos ) )
        _Init();

    _ComputeTiles();

    for( u32 i = 0; i < _tiles_flags.size; ++i )
    {
        _tiles_flags[i] &= ~ETileFlag::NEED_REBUILD;
    }
}

}
}//