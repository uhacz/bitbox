#include "voxel.h"
#include "voxel_octree.h"

#include <util/hashmap.h>
#include <util/array.h>
#include <util/debug.h>
#include <util/bbox.h>
#include "grid.h"

namespace bxVoxel
{
    namespace
    {
        u32 _Octree_allocateNode( bxVoxel_Octree* octree )
        {
            bxVoxel_Octree::Node node;
            memset( &node, 0x00, sizeof( bxVoxel_Octree::Node ) );
            u32 index = array::push_back( octree->nodes, node );
            return index;
        }

        i32 _Octree_allocateData( bxVoxel_Octree* octree )
        {
            return array::push_back( octree->data, (size_t)0 );
        }

        void _Octree_initNode( bxVoxel_Octree::Node* node, u32 size )
        {
            node->size = size;
            node->x = 0;
            node->y = 0;
            node->z = 0;
            node->dataIndex = -1;
            memset( node->children, 0xFF, sizeof( node->children ) );
        }

    }

    bxVoxel_Octree* octree_new( int size, bxAllocator* alloc )
    {
        SYS_ASSERT( size > 0 && size < 0x100 );
        SYS_ASSERT( (size % 2) == 0 );
        {
            int tmp = size;
            while ( tmp > 1 )
            {
                SYS_ASSERT( (tmp % 2) == 0 && "size must be power of 2 number" );
                tmp /= 2;
            }
        }

        bxVoxel_Octree* octree = BX_NEW( alloc, bxVoxel_Octree );

        u32 rootIndex = _Octree_allocateNode( octree );
        bxVoxel_Octree::Node& root = octree->nodes[rootIndex];
        _Octree_initNode( &root, size );

        return octree;

        //const int maxNodes = size * size * size;

        //int memSize = 0;
        //memSize += sizeof( bxVoxelOctree );
        //memSize += maxNodes * sizeof( bxVoxelOctree::Node );

        //void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 4 );
        //memset( mem, 0x00, memSize );

        //bxBufferChunker chunker( mem, memSize );
        //bxVoxelOctree* octree = chunker.add< bxVoxelOctree >();
        //octree->nodes = chunker.add< bxVoxelOctree::Node >( maxNodes );
        //octree->size = size;
        //octree->sizeNode = 0;

        //chunker.check();

        //return octree;

    }
    void octree_delete( bxVoxel_Octree** voct, bxAllocator* alloc )
    {
        BX_DELETE0( alloc, voct[0] );
    }

    namespace
    {
        bxAABB _Octree_nodeAABB( const bxVoxel_Octree::Node& node )
        {
            const Vector3 nodeBegin( (float)node.x, (float)node.y, (float)node.z );
            const Vector3 nodeSize( (float)node.size );
            return bxAABB( nodeBegin, nodeBegin + nodeSize );
        }
        static inline bool testAABB( const vec_float4 bboxMin, const vec_float4 bboxMax, const vec_float4 point )
        {
            const vec_float4 a = vec_cmple( bboxMin, point );
            const vec_float4 b = vec_cmpgt( bboxMax, point );
            const vec_float4 a_n_b = vec_and( a, b );
            const int mask = _mm_movemask_ps( vec_and( vec_splat( a_n_b, 0 ), vec_and( vec_splat( a_n_b, 1 ), vec_splat( a_n_b, 2 ) ) ) );
            return mask == 0xF;
        }

        inline size_t _Octree_createMapKey( u32 x, u32 y, u32 z )
        {
            return ( 255 << 24 ) | (x << 16) | (y << 8) | z;
        }

        int _Octree_insertR( bxVoxel_Octree* voct, u32 nodeIndex, const Vector3& point, size_t data )
        {
            const bxAABB bbox = _Octree_nodeAABB( voct->nodes[nodeIndex] );
            const bool insideBBox = testAABB( bbox.min.get128(), bbox.max.get128(), point.get128() );

            if ( !insideBBox )
                return -1;

            int result = -1;

            const u32 nodeSize = voct->nodes[nodeIndex].size;
            const u8 nodeX = voct->nodes[nodeIndex].x;
            const u8 nodeY = voct->nodes[nodeIndex].y;
            const u8 nodeZ = voct->nodes[nodeIndex].z;

            if ( nodeSize == 1 )
            {
                u32 dataIndex = _Octree_allocateData( voct );
                voct->nodes[nodeIndex].dataIndex = dataIndex;
                voct->data[dataIndex] = data;
                {
                    size_t hash = _Octree_createMapKey( nodeX, nodeY, nodeZ );
                    hashmap_t::cell_t* cell = hashmap::insert( voct->map, hash );
                    cell->value = data;
                }

                result = 1;
            }
            else
            {
                if ( voct->nodes[nodeIndex].children[0] == 0xFFFFFFFF )
                {
                    bxGrid grid( 2, 2, 2 );

                    const u32 childSize = nodeSize / 2;

                    for ( int ichild = 0; ichild < 8; ++ichild )
                    {
                        u32 xyz[3];
                        grid.coords( xyz, ichild );

                        u32 index = _Octree_allocateNode( voct );
                        bxVoxel_Octree::Node& childNode = voct->nodes[index];
                        _Octree_initNode( &childNode, childSize );
                        childNode.x = xyz[0] * childSize + nodeX;
                        childNode.y = xyz[1] * childSize + nodeY;
                        childNode.z = xyz[2] * childSize + nodeZ;

                        voct->nodes[nodeIndex].children[ichild] = index;
                    }
                }

                for ( int ichild = 0; ichild < 8; ++ichild )
                {
                    result = _Octree_insertR( voct, voct->nodes[nodeIndex].children[ichild], point, data );
                    if ( result > 0 )
                        break;
                }
            }
            return result;
        }
    }


    void octree_insert( bxVoxel_Octree* voct, const Vector3& point, size_t data )
    {
        _Octree_insertR( voct, 0, point, data );
    }

    void octree_clear( bxVoxel_Octree* voct )
    {
        if( array::empty( voct->nodes ) )
            return;

        const u32 size = voct->nodes[0].size;

        array::clear( voct->nodes );
        array::clear( voct->data );
        hashmap::clear( voct->map );

        u32 rootIndex = _Octree_allocateNode( voct );
        bxVoxel_Octree::Node& root = voct->nodes[rootIndex];
        _Octree_initNode( &root, size );
    }

    namespace
    {
        int _Octree_checkCellR( const bxVoxel_Octree* voct, u32 nodeIndex, const Vector3& point )
        {
            const bxVoxel_Octree::Node& node = voct->nodes[nodeIndex];
            const bxAABB bbox = _Octree_nodeAABB( node );
            const bool insideBBox = testAABB( bbox.min.get128(), bbox.max.get128(), point.get128() );

            int result = -1;
            if ( !insideBBox )
                return result;

            if ( node.size == 1 )
            {
                result = (node.dataIndex > 0) ? 1 : -1;
            }
            else if ( node.children[0] != 0xFFFFFFFF )
            {
                for ( int ichild = 0; ichild < 8; ++ichild )
                {
                    result = _Octree_checkCellR( voct, node.children[ichild], point );
                    if ( result > 0 )
                        break;
                }
            }

            return result;
        }


        inline bxVoxel_GpuData _Octree_createVoxelData( const bxVoxel_Octree* voct, const bxVoxel_Octree::Node& node, const bxGrid& grid )
        {
            bxVoxel_GpuData vx;
            vx.gridIndex = grid.index( node.x, node.y, node.z );
            vx.colorRGBA = (u32)voct->data[node.dataIndex];
            return vx;
        }
    }///

    void octree_getShell( array_t<bxVoxel_GpuData>& vxData, const bxVoxel_Octree* voct )
    {
        array::reserve( vxData, (int)voct->map.size );
        vxData.size = octree_getShell( array::begin( vxData ), array::capacity( vxData ), voct );
    }

    int octree_getShell( bxVoxel_GpuData* vxData, int xvDataCapacity, const bxVoxel_Octree* voct )
    {
        bxGrid grid( GLOBAL_GRID_SIZE, GLOBAL_GRID_SIZE, GLOBAL_GRID_SIZE );


        int vxDataSize = 0;
        for ( int inode = 0; inode < array::size( voct->nodes ); ++inode )
        {
            const bxVoxel_Octree::Node& node = voct->nodes[inode];
            if ( node.size > 1 )
                continue;

            if ( node.dataIndex < 0 )
                continue;

            if( vxDataSize >= xvDataCapacity )
                break;

            const u32 centerX = node.x;
            const u32 centerY = node.y;
            const u32 centerZ = node.z;
            const Vector3 center( (float)centerX, (float)centerY, (float)centerZ );

            int emptyFound = 0;
            for ( int iz = -1; iz <= 1 && !emptyFound; ++iz )
            {
                for ( int iy = -1; iy <= 1 && !emptyFound; ++iy )
                {
                    for ( int ix = -1; ix <= 1 && !emptyFound; ++ix )
                    {
                        if ( ix == 0 && iy == 0 && iz == 0 )
                            continue;
                        const size_t key = _Octree_createMapKey( centerX + ix, centerY + iy, centerZ +iz );
                        emptyFound = hashmap::lookup( voct->map, key ) == 0;
                    }
                }
            }

            if( emptyFound )
            {
                vxData[vxDataSize++] = _Octree_createVoxelData( voct, node, grid );
            }

            ////u32 mask = 0; // 0x3F for all sides
            //u32 hitX = 0;
            //u32 hitY = 0;
            //u32 hitZ = 0;
            //for ( int ix = -1; ix <= 1; ix += 2 )
            //{
            //    const size_t key = _Octree_createMapKey( centerX + ix, centerY, centerZ );
            //    if ( !hashmap::lookup( voct->map, key ) )
            //        break;

            //    ++hitX;
            //}

            //if ( hitX < 2 )
            //{
            //    vxData[vxDataSize++] = _Octree_createVoxelData( voct, node, grid );
            //    continue;
            //}

            //if( vxDataSize >= xvDataCapacity )
            //    break;

            //for ( int iy = -1; iy <= 1; iy += 2 )
            //{
            //    const size_t key = _Octree_createMapKey( centerX, centerY + iy, centerZ );
            //    if ( !hashmap::lookup( voct->map, key ) )
            //        break;

            //    ++hitY;
            //}

            //if ( hitY < 2 )
            //{
            //    vxData[vxDataSize++] = _Octree_createVoxelData( voct, node, grid );
            //    continue;
            //}

            //if( vxDataSize >= xvDataCapacity )
            //    break;

            //for ( int iz = -1; iz <= 1; iz += 2 )
            //{
            //    const size_t key = _Octree_createMapKey( centerX, centerY, centerZ + iz );
            //    if ( !hashmap::lookup( voct->map, key ) )
            //        break;

            //    ++hitZ;
            //}

            //if ( hitZ < 2 )
            //{
            //    vxData[vxDataSize++] = _Octree_createVoxelData( voct, node, grid );
            //    continue;
            //}
        }
        return vxDataSize;
    }

}///

#include <gfx/gfx_debug_draw.h>
namespace bxVoxel
{
    void octree_debugDraw( bxVoxel_Octree* voct )
    {
        for ( int inode = 0; inode < array::size( voct->nodes ); ++inode )
        {
            const bxVoxel_Octree::Node& node = voct->nodes[inode];
            const bxAABB bbox = _Octree_nodeAABB( node );

            u32 color = 0x333333FF;
            if ( node.dataIndex > 0 )
                color = (u32)voct->data[node.dataIndex];

            bxGfxDebugDraw::addBox( Matrix4::translation( bxAABB::center( bbox ) ), bxAABB::size( bbox ) * 0.5f, color, 1 );
        }
    }


}///
