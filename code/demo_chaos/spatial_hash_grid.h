#pragma once

#include <util/type.h>
#include <util/containers.h>
#include <util/vectormath/vectormath.h>

namespace bx
{

struct HashGridStatic
{
    struct Bucket
    {
        u32 begin;
        u32 count;
    };

    struct Indices
    {
        const u32* data;
        u32 count;

        const u32* begin() const { return data; }
        const u32* end()   const { return data + count; }
    };

    array_t<Bucket> _lookup_array; // here is stored index in data array
    array_t<u32>    _data;   
    array_t<u64>    _scratch_buffer;
    f32 _cell_size = 0.f;
    f32 _cell_size_inv = 0.f;
    
    const Indices Lookup( const Vector3F& x ) const;
    const Indices Lookup( const i32x3& xGrid ) const;
    const Indices Get( u32 index ) const;
    const i32x3 ComputeGridPos( const Vector3F& x ) const;
};
void Build( HashGridStatic* hg, u32* xGridIndices, const Vector3F* x, u32 count, u32 hashmapSize, float cellSize );

}//