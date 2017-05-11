#pragma once

#include <util/containers.h>
#include <util/vectormath/vectormath.h>
#include "terrain_create_info.h"

#include <rdi/rdi_backend.h>
#include "util/debug.h"

namespace bx { namespace terrain {

    typedef array_t<Vector3F> Vector3FArray;
    typedef array_t<Vector2I> Vector2IArray;
    typedef array_t<u32>      U32Array;
    typedef array_t<u8 >      U8Array;
    typedef array_t<f32>      F32Array;
    
    typedef array_t<rdi::VertexBuffer> VertexBufferArray;
    typedef array_t<rdi::IndexBuffer>  IndexBufferArray;

    inline u32       GetFlatIndex( const Vector2UI& v, u32 width ) { return v.y * width + v.x; }
    inline Vector2UI GetCoords   ( u32 index, u32 w )              { return Vector2UI( index % w, index / w ); }

namespace ETileFlag
{
    enum Enum : u8
    {
        NEED_REBUILD = 0x1,
    };
}//

struct Instance
{
    CreateInfo _info = {};
    f32 _tile_side_length_inv = 0.f;
    u32 _num_tiles_per_side = 0;
    u32 _num_tiles_in_radius = 0;
    u32 _num_tile_samples = 0;
    u32 _num_indices_per_lod[Const::ELod::COUNT] = {};
    u32 _indices_offset     [Const::ELod::COUNT] = {};

    Vector3FArray       _tiles_world_pos;
    F32Array            _tiles_samples;
    VertexBufferArray   _tiles_vbuff;
    U8Array             _tiles_flags;
    U32Array            _tiles_indices;
    rdi::IndexBuffer    _tiles_gpu_indices;

    Vector3F _observer_pos{ 0.f };
    Vector3F _up_axis{ 0.f, 1.f, 0.f };

    Vector2I _grid_center_pos{ 0 };
    Vector2I _grid_observer_pos{ 0 };
    Vector2UI _data_center{ 0 };

    Vector2I   _ComputePosOnGrid( const Vector3F& posWS )       { return toVector2xz<i32>( projectVectorOnPlane( posWS, _up_axis ) * _tile_side_length_inv ); }

    void _Init();
    void _ComputeTiles();

    void Tick();
    void DebugDraw( u32 color );
    void Gui();
};

}}
