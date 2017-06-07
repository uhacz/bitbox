#include "spatial_hash_grid.h"
#include <util\array.h>
#include <algorithm>
#include "..\util\debug.h"

namespace bx
{
//namespace hash_grid
//{
    static __forceinline u32 MakeHash( int x, int y, int z, u32 hashmapSize )
    {
        static const u32 prime[] = 
        {
            73856093,19349663,83492791
        };

        const u32 a = x * prime[0];
        const u32 b = y * prime[1];
        const u32 c = z * prime[2];

        return ( a ^ b ^ c ) % hashmapSize;
    }

    static __forceinline i32x3 Discretize( const Vector3F& x, float cellSizeInv )
    {
        const Vector3F xi = x * cellSizeInv;
        return i32x3( (int)xi.x, (int)xi.y, (int)xi.z );
    }

    static __forceinline u32 ComputeIndex( const Vector3F& x, float cellSizeInv, int hashmapSize )
    {
        const i32x3 x_in_grid = Discretize( x, cellSizeInv );
        const u32 hash_index = MakeHash( x_in_grid.x, x_in_grid.y, x_in_grid.z, hashmapSize );
        return hash_index;
    }

    void Build( HashGridStatic* hg, u32* xGridIndices, const Vector3F* x, u32 count, u32 hashmapSize, float cellSize )
    {
        array::clear( hg->_lookup_array );
        array::clear( hg->_data );

        array::resize( hg->_lookup_array, hashmapSize );
        array::reserve( hg->_data, count );

        union Info
        {
            u64 key;
            struct
            {
                u32 point_index;
                u32 hash_index;
            }data;
        };
        array_t<u64>& scratch = hg->_scratch_buffer;
        array::clear( scratch );
        array::reserve( scratch, count );

        const float cellSizeInv = 1.f / cellSize;

        if( xGridIndices )
        {
            for( u32 i = 0; i < count; ++i )
            {
                const u32 hash_index = ComputeIndex( x[i], cellSizeInv, hashmapSize );

                Info info;
                info.data.hash_index = hash_index;
                info.data.point_index = i;

                array::push_back( scratch, info.key );
                xGridIndices[i] = hash_index;
            }
        }
        else
        {
            for( u32 i = 0; i < count; ++i )
            {
                const u32 hash_index = ComputeIndex( x[i], cellSizeInv, hashmapSize );

                Info info;
                info.data.hash_index = hash_index;
                info.data.point_index = i;

                array::push_back( scratch, info.key );
            }
        }

        std::sort( scratch.begin(), scratch.end(), std::less<u64>() );

        for( u32 i = 0; i < hashmapSize; ++i )
        {
            hg->_lookup_array[i].begin = 0;
            hg->_lookup_array[i].count = 0;
        }

        
        const Info* first_info = (Info*)scratch.begin();
        u32 current_grid_index = first_info->data.hash_index;
        HashGridStatic::Bucket current_bucket = { 0, 0 };
        for( const u64 infoKey : scratch )
        {
            const Info info = { infoKey };

            array::push_back( hg->_data, info.data.point_index );
            
            if( info.data.hash_index == current_grid_index )
            {
                current_bucket.count += 1;
            }
            else
            {
                hg->_lookup_array[current_grid_index] = current_bucket;
                current_bucket.begin += current_bucket.count;
                current_bucket.count = 1;
                current_grid_index = info.data.hash_index;
            }
        }

        if( current_bucket.count )
        {
            hg->_lookup_array[current_grid_index] = current_bucket;
        }
        
        hg->_cell_size = cellSize;
        hg->_cell_size_inv = cellSizeInv;
    }
//}

const HashGridStatic::Indices HashGridStatic::Lookup( const Vector3F& x ) const
{
    const u32 index = ComputeIndex( x, _cell_size_inv, _lookup_array.size );
    SYS_ASSERT( index < _lookup_array.size );
    return Get( index );
}

const HashGridStatic::Indices HashGridStatic::Get( u32 index ) const
{
    SYS_ASSERT( index < _lookup_array.size );

    const Bucket b = _lookup_array[index];
    SYS_ASSERT( ( b.begin + b.count ) <= _data.size );

    Indices indices;
    indices.data = _data.begin() + b.begin;
    indices.count = b.count;
    return indices;
}

}//