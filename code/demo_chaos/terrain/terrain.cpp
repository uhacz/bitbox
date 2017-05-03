#include "terrain.h"
#include <util\array.h>
#include <util\debug.h>

#include <rdi\rdi_debug_draw.h>
#include "../imgui/imgui.h"
#include "util\common.h"

namespace bx{ namespace terrain{ 


template< typename T >
struct TVector2
{
    union
    {
        T xy[2];
        struct{ T x, y; };
    };

    TVector2() {}
    TVector2( T v ): x(v), y(v) {}
    TVector2( T v0, T v1 ): x( v0 ), y( v1 ) {}

    TVector2& operator = ( const TVector2& v )
    {
        x = v.x; y = v.y;
        return *this;
    }
    TVector2 operator + ( const TVector2& v ) const { return TVector2( x + v.x, y + v.y ); }
    TVector2 operator - ( const TVector2& v ) const { return TVector2( x - v.x, y - v.y ); }
    TVector2 operator * ( float v )           const { return TVector2( (T)( x * v ), (T)( y * v ) ); }
    TVector2 operator / ( float v )           const { return TVector2( (T)( x / v ), (T)( y / v ) ); }
    TVector2 operator - ()                    const { return TVector2( -x, -y ); }
};
template< typename T > inline TVector2<T> mulPerElem  ( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( a.x * b.x, a.y * b.y ); }
template< typename T > inline TVector2<T> divPerElem  ( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( a.x / b.x, a.y / b.y ); }
template< typename T > inline TVector2<T> minPerElem  ( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( minOfPair( a.x, b.x ), minOfPair( a.y, b.y ) ); }
template< typename T > inline TVector2<T> maxPerElem  ( const TVector2<T>& a, const TVector2<T>& b ) { return TVector2<T>( maxOfPair( a.x, b.x ), maxOfPair( a.y, b.y ) ); }
template< typename T > inline TVector2<T> absPerElem  ( const TVector2<T>& a )                       { return TVector2<T>( ::abs( a.x ), ::abs( a.y ) ); }

template< typename T > inline TVector2<T> toVector2xy ( const Vector3F& v ) { return TVector2<T>( (T)v.x, (T)v.y ); }
template< typename T > inline TVector2<T> toVector2xz ( const Vector3F& v ) { return TVector2<T>( (T)v.x, (T)v.z ); }
template< typename T > inline TVector2<T> toVector2yz ( const Vector3F& v ) { return TVector2<T>( (T)v.y, (T)v.z ); }
template< typename T > inline Vector3F toVector3Fxy( const TVector2<T>& v ) { return Vector3F( (f32)v.x, (f32)v.y, 0.f ); }
template< typename T > inline Vector3F toVector3Fxz( const TVector2<T>& v ) { return Vector3F( (f32)v.x, 0.f, (f32)v.y ); }
template< typename T > inline Vector3F toVector3Fyz( const TVector2<T>& v ) { return Vector3F( 0.f, (f32)v.x, (f32)v.y ); }

typedef TVector2<i32> Vector2I;
typedef TVector2<u32> Vector2UI;

typedef array_t<Vector3F> Vector3FArray;
typedef array_t<Vector2I> Vector2IArray;
typedef array_t<u32>      U32Array;
typedef array_t<u8 >      U8Array;

inline u32       GetFlatIndex( const Vector2UI& v, u32 width ) { return v.y * width + v.x; }
inline Vector2UI GetCoords   ( u32 index, u32 w )              { return Vector2UI( index % w, index / w ); }

struct VirtualGrid
{
    struct Cell
    {
        Vector2I xy;
        u32 dataIndex;
    };

    array_t<Cell> _cells;
    u32 _side_length = 0;

    void Init( const Vector2I* inputPos, u32 count )
    {
        const double root = ::sqrt( (double)count );
        SYS_ASSERT( (root - ::floor( root )) < FLT_EPSILON );

        _side_length = (u32)root;

        array::clear( _cells );
        array::reserve( _cells, count*count );

        for( u32 i = 0; i < count; ++i )
        {
            Cell cell;
            cell.xy = inputPos[i];
            cell.dataIndex = i;
            array::push_back( _cells, cell );
        }
    }

    void update( i32 dx, i32 dy )
    {
        
    }

};

namespace ETileFlag
{
    enum Enum : u8
    {
        NEED_REBUILD = 0x1,
    };
}//
typedef u32( *JumpFunction )( u32, u32, u32 );
struct Instance
{
    CreateInfo _info = {};
    f32 _tile_side_length_inv = 0.f;
    u32 _num_tiles_per_side = 0;
    u32 _num_tiles_in_radius = 0;

    Vector3FArray _tiles_world_pos;
    //Vector2IArray _tiles_grid_pos;
    //U32Array      _tiles_data_index;
    U8Array       _tiles_flags;
    

    Vector3F _observer_pos  { 0.f };
    Vector3F _up_axis       { 0.f, 1.f, 0.f };

    Vector2I _grid_center_pos{ 0 };
    Vector2I _grid_observer_pos{ 0 };
    Vector2UI _data_center{ 0 };

    Vector2I _ComputeGridPos( const Vector3F& posWS ) { return toVector2xz<i32>( projectVectorOnPlane( posWS, _up_axis ) * _tile_side_length_inv ); }

    void DebugDraw( u32 color )
    {
        const Vector3F ext = Vector3F( _info.tile_side_length, 0.2f, _info.tile_side_length ) * 0.5f;
        for( const Vector3F& pos : _tiles_world_pos )
        {
            rdi::debug_draw::AddBox( Matrix4F::translation( pos ), ext, color, 1 );
        }
    }

    void Gui()
    {
        if( ImGui::Begin( "terrain" ) )
        {
            ImGui::Text( "grid center pos: %d, %d", _grid_center_pos.x, _grid_center_pos.y );
            ImGui::Text( "grid observer pos: %d, %d", _grid_observer_pos.x, _grid_observer_pos.y );
            ImGui::Text( "data center: %u, %u", _data_center.x, _data_center.y );
        }
        ImGui::End();
    }

    void Tick( )
    {
        // initialization
        if( array::empty( _tiles_world_pos ) )
        {
            const f32 side_length = _info.tile_side_length;
            const f32 max_radius = _info.radius[CreateInfo::NUM_LODS - 1] * side_length;
            
            u32 num_tiles_per_side = ( u32 )::ceil( max_radius / side_length );
            num_tiles_per_side += ( num_tiles_per_side % 2 ) ? 0 : 1;
            _num_tiles_per_side = num_tiles_per_side;
            _num_tiles_in_radius = num_tiles_per_side / 2;

            array::reserve( _tiles_world_pos, num_tiles_per_side * num_tiles_per_side );
            array::reserve( _tiles_flags, _tiles_world_pos.capacity );

            const f32 off = max_radius * 0.5f;
            const Vector3F zero_offset( -off, 0.f, -off );
            const Vector3F observer_offset = projectVectorOnPlane( _observer_pos, _up_axis );

            _grid_center_pos = _ComputeGridPos( _observer_pos );
            _grid_observer_pos = _grid_center_pos;
            _data_center = Vector2UI( _num_tiles_in_radius, _num_tiles_in_radius);

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

        const Vector2I new_grid_observer_pos = _ComputeGridPos( _observer_pos );
        const Vector2I delta =  maxPerElem( -Vector2I( _num_tiles_in_radius ), minPerElem( Vector2I( _num_tiles_in_radius ),  new_grid_observer_pos - _grid_center_pos ) );
        const Vector2I delta_abs = absPerElem( delta );
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

        const Vector2I n_jumps = maxPerElem( Vector2I( 1 ), Vector2I( _num_tiles_in_radius ) - delta_abs );
        const i32 radius = (i32)_num_tiles_in_radius;

        if( delta.x )
        {
            JumpFunction jump_func = ( delta.x > 0 ) ? wrap_inc_u32 : wrap_dec_u32;
            u32 data_col_begin = (*jump_func)( _data_center.x, 0, last_index );
            i32 xoffset = 1;
            for( i32 j = 0; j < n_jumps.x; ++j, ++xoffset )
                data_col_begin = (*jump_func)( data_col_begin, 0, last_index );

            if( delta.x < 0 )
                xoffset = -xoffset;

            for( i32 zoffset = -radius, data_row = 0; zoffset <= radius; ++zoffset, ++data_row )
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

                    data_col = (*jump_func)( data_col, 0, last_index );
                }
            }

        }
        if( delta.y  )
        {
            JumpFunction jump_func = ( delta.y > 0 ) ? wrap_inc_u32 : wrap_dec_u32;
            u32 data_row_begin = ( *jump_func )( _data_center.y, 0, last_index );
            i32 zoffset = 1;
            for( i32 j = 0; j < n_jumps.y; ++j, ++zoffset )
                data_row_begin = ( *jump_func )( data_row_begin, 0, last_index );

            if( delta.y < 0 )
                zoffset = -zoffset;

            for( i32 xoffset = -radius, data_col = 0; xoffset <= radius; ++xoffset, ++data_col )
            {
                u32 data_row = data_row_begin;
                for( i32 i = 0; i < delta_abs.y; ++i )
                {
                    u32 data_index = GetFlatIndex( Vector2UI( data_col, data_row ), _num_tiles_per_side );
                    SYS_ASSERT( data_index < _tiles_world_pos.size );
                    
                    Vector2I grid_pos = new_grid_observer_pos + Vector2I( xoffset, zoffset );
                    Vector3F world_pos = toVector3Fxz( grid_pos ) * _info.tile_side_length;
                    _tiles_world_pos[data_index] = world_pos;
                    _tiles_flags[data_index] |= ETileFlag::NEED_REBUILD;

                    data_row = ( *jump_func )( data_row, 0, last_index );
                }
            }
        }

        _grid_observer_pos = new_grid_observer_pos;
        _grid_center_pos = _grid_observer_pos;
    }

};

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