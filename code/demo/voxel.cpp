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

void octree_getShell( array_t<bxVoxelData>& vxData, const bxVoxelOctree* voct )
{
    bxGrid grid( 512, 512, 512 );
    for ( int inode = 0; inode < array::size( voct->nodes ); ++inode )
    {
        const bxVoxelOctree::Node& node = voct->nodes[inode];
        if ( node.size != 2 )
            continue;

        if ( node.children[0] == 0xFFFFFFFF )
            continue;

        u8 mask = 0;
        for( int ichild = 0; ichild < 8; ++ichild )
        {
            mask += (voct->nodes[node.children[ichild]].dataIndex != -1) ? 1 << ichild : 0;
        }

        if ( mask == 0xFF )
            continue;

        for ( int ichild = 0; ichild < 8; ++ichild )
        {
            if ( !(mask & (1 << ichild)) )
                continue;

            const bxVoxelOctree::Node& child = voct->nodes[node.children[ichild]];
            bxVoxelData vx;
            vx.gridIndex = grid.index( child.x, child.y, child.z );
            vx.colorRGBA = (u32)voct->data[child.dataIndex];
            array::push_back( vxData, vx );
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
