#include "voxel.h"

#include <util/type.h>
#include <util/debug.h>
#include <util/buffer_utils.h>
#include <util/memory.h>
#include <util/array.h>
#include <util/bbox.h>
#include "grid.h"
#include <gfx/gfx_debug_draw.h>

struct bxVoxelOctree
{
    struct Node
    {
        u8 x, y, z;
        u8 size;
        u32 children[8];
        i32 dataIndex;
    };

    array_t<Node> nodes;
    array_t<size_t> data;
    
};

namespace bxVoxel
{
    namespace
    {
        u32 _Octree_allocateNode( bxVoxelOctree* octree )
        {
            bxVoxelOctree::Node node;
            memset( &node, 0x00, sizeof( bxVoxelOctree::Node ) );
            u32 index = array::push_back( octree->nodes, node );
            return index;
        }

        i32 _Octree_allocateData( bxVoxelOctree* octree )
        {
            return array::push_back( octree->data, (size_t)0 );
        }

        void _Octree_initNode( bxVoxelOctree::Node* node, u32 size )
        {
            node->size = size;
            node->x = 0;
            node->y = 0;
            node->z = 0;
            node->dataIndex = -1;
            memset( node->children, 0xFF, sizeof( node->children ) );    
        }

    }

bxVoxelOctree* octree_new( int size )
{
    SYS_ASSERT( size > 0 && size < 0x100 );
    SYS_ASSERT( ( size % 2 ) == 0 );
    {
        int tmp = size;
        while( tmp > 1 )
        {
            SYS_ASSERT( (tmp % 2) == 0 && "size must be power of 2 number" );
            tmp /= 2;
        }
    }

    bxVoxelOctree* octree = BX_NEW( bxDefaultAllocator(), bxVoxelOctree );
    
    u32 rootIndex = _Octree_allocateNode( octree );
    bxVoxelOctree::Node& root = octree->nodes[rootIndex];
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
void octree_delete( bxVoxelOctree** voct )
{
    BX_DELETE0( bxDefaultAllocator(), voct[0] );
}

namespace
{
    bxAABB _Octree_nodeAABB( const bxVoxelOctree::Node& node )
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

    int _Octree_insertR( bxVoxelOctree* voct, u32 nodeIndex, const Vector3& point, size_t data )
    {
        const bxAABB bbox = _Octree_nodeAABB( voct->nodes[nodeIndex] );
        const bool insideBBox = testAABB( bbox.min.get128(), bbox.max.get128(), point.get128() );
        
        if( !insideBBox )
            return -1;

        int result = -1;

        const u32 nodeSize = voct->nodes[nodeIndex].size;
        const u8 nodeX = voct->nodes[nodeIndex].x;
        const u8 nodeY = voct->nodes[nodeIndex].y;
        const u8 nodeZ = voct->nodes[nodeIndex].z;

        if( nodeSize == 1 )
        {
            u32 dataIndex = _Octree_allocateData( voct );
            voct->nodes[nodeIndex].dataIndex = dataIndex;
            voct->data[dataIndex] = data;
            result = 1;
        }
        else
        {
            if( voct->nodes[nodeIndex].children[0] == 0xFFFFFFFF )
            {
                bxGrid grid( 2, 2, 2 );
                
                const u32 childSize = nodeSize/2;
                
                for( int ichild = 0; ichild < 8; ++ichild )
                {
                    u32 xyz[3];
                    grid.coords( xyz, ichild );

                    u32 index = _Octree_allocateNode( voct );
                    bxVoxelOctree::Node& childNode = voct->nodes[index];
                    _Octree_initNode( &childNode, childSize );
                    childNode.x = xyz[0] * childSize + nodeX;
                    childNode.y = xyz[1] * childSize + nodeY;
                    childNode.z = xyz[2] * childSize + nodeZ;

                    voct->nodes[nodeIndex].children[ichild] = index;
                }
            }
            
            for( int ichild = 0; ichild < 8; ++ichild )
            {
                result = _Octree_insertR( voct, voct->nodes[nodeIndex].children[ichild], point, data );
                if( result > 0 )
                    break;
            }
        }
        return result;
    }
}


void octree_insert( bxVoxelOctree* voct, const Vector3& point, size_t data )
{
    _Octree_insertR( voct, 0, point, data );
}

void octree_clear( bxVoxelOctree* voct )
{
    array::clear( voct->nodes );
    array::clear( voct->data );
}

namespace
{
    int _Octree_checkCellR( const bxVoxelOctree* voct, u32 nodeIndex, const Vector3& point )
    {
        const bxVoxelOctree::Node& node = voct->nodes[nodeIndex];
        const bxAABB bbox = _Octree_nodeAABB( node );
        const bool insideBBox = testAABB( bbox.min.get128(), bbox.max.get128(), point.get128() );

        int result = -1;
        if ( !insideBBox )
            return result;

        if( node.size == 1 )
        {
            result = (node.dataIndex > 0) ? 1 : -1;
        }
        else if( node.children[0] != 0xFFFFFFFF )
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

    inline size_t _Octree_createMapKey( u32 x, u32 y, u32 z )
    {
        return (x << 16) | (y << 8) | z;
    }
    inline bxVoxelData _Octree_createVoxelData( const bxVoxelOctree* voct, const bxVoxelOctree::Node& node, const bxGrid& grid )
    {
        bxVoxelData vx;
        vx.gridIndex = grid.index( node.x, node.y, node.z );
        vx.colorRGBA = (u32)voct->data[node.dataIndex];
        return vx;
    }
}///

void octree_getShell( array_t<bxVoxelData>& vxData, const bxVoxelOctree* voct )
{
    bxGrid grid( 512, 512, 512 );

    hashmap_t cellMap( array::size( voct->data ) );
    for ( int inode = 0; inode < array::size( voct->nodes ); ++inode )
    {
        const bxVoxelOctree::Node& node = voct->nodes[inode];
        if ( node.size > 1 )
            continue;

        if ( node.dataIndex < 0 )
            continue;

        const u32 nodeX = node.x;
        const u32 nodeY = node.y;
        const u32 nodeZ = node.z;

        size_t hash = _Octree_createMapKey( nodeX, nodeY, nodeZ );
        SYS_ASSERT( hashmap::lookup( cellMap, hash ) == 0 );

        hashmap_t::cell_t* cell = hashmap::insert( cellMap, hash );
        cell->value = voct->data[node.dataIndex];

        //array::push_back( vxData, _Octree_createVoxelData( voct, node, grid ) );
    }

    //return;

    for ( int inode = 0; inode < array::size( voct->nodes ); ++inode )
    {
        const bxVoxelOctree::Node& node = voct->nodes[inode];
        if ( node.size > 1 )
            continue;

        if ( node.dataIndex < 0 )
            continue;

        const u32 centerX = node.x;
        const u32 centerY = node.y;
        const u32 centerZ = node.z;
        const Vector3 center( centerX, centerY, centerZ );
        
        //u32 mask = 0; // 0x3F for all sides
        u32 hitX = 0;
        u32 hitY = 0;
        u32 hitZ = 0;
        for( int ix = -1; ix <= 1; ix += 2 )
        {
            const size_t key = _Octree_createMapKey( centerX + ix, centerY, centerZ );
            if ( !hashmap::lookup( cellMap, key ) )
                break;

            ++hitX;
        }
        
        if( hitX < 2 )
        {
            const bxVoxelData vx = _Octree_createVoxelData( voct, node, grid );
            array::push_back( vxData, vx );
            continue;
        }

        for ( int iy = -1; iy <= 1; iy += 2 )
        {
            const size_t key = _Octree_createMapKey( centerX, centerY + iy, centerZ );
            if ( !hashmap::lookup( cellMap, key ) )
                break;
            
            ++hitY;
        }
        
        if ( hitY < 2 )
        {
            const bxVoxelData vx = _Octree_createVoxelData( voct, node, grid );
            array::push_back( vxData, vx );
            continue;
        }

        for ( int iz = -1; iz <= 1; iz += 2 )
        {
            const size_t key = _Octree_createMapKey( centerX, centerY, centerZ + iz );
            if ( !hashmap::lookup( cellMap, key ) )
                break;

            ++hitZ;
        }

        if ( hitZ < 2 )
        {
            const bxVoxelData vx = _Octree_createVoxelData( voct, node, grid );
            array::push_back( vxData, vx );
            continue;
        }
    }
}

void octree_debugDraw( bxVoxelOctree* voct )
{
    for( int inode = 0; inode < array::size( voct->nodes ); ++inode )
    {
        const bxVoxelOctree::Node& node = voct->nodes[inode];
        const bxAABB bbox = _Octree_nodeAABB( node );
        
        u32 color = 0x333333FF;
        if (node.dataIndex > 0) 
            color = (u32)voct->data[node.dataIndex];

        bxGfxDebugDraw::addBox( Matrix4::translation( bxAABB::center( bbox ) ), bxAABB::size( bbox ) * 0.5f, color, 1 );
    }
}

}///
