#include "voxel.h"

#include <util/type.h>
#include <util/debug.h>
#include <util/buffer_utils.h>
#include <util/memory.h>
#include <util/array.h>

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
    root.size = size;
    root.x = 0;
    root.y = 0;
    root.z = 0;
    root.dataIndex = -1;
    memset( root.children, 0xFF, sizeof( root.children ) );
    
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



unsigned octree_insert( bxVoxelOctree* voct, const Vector3& point )
{
    return 0;

}
}///
