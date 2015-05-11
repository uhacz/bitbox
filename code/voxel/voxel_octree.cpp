#include "voxel.h"
#include "voxel_octree.h"

#include <util/hashmap.h>
#include <util/array.h>
#include <util/debug.h>
#include <util/bbox.h>
#include <util/grid.h>

#include <smmintrin.h>

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

        void _Octree_initNode( bxVoxel_Octree::Node* node, const Vector3& size )
        {
            node->size = Vector4( size, size.getX() * size.getY() * size.getZ() );
            node->center = Vector3(0.f);
            node->dataIndex = -1;
            memset( node->children, 0xFF, sizeof( node->children ) );
        }

    }

    bxVoxel_Octree* octree_new( bxAllocator* alloc )
    {
        //SYS_ASSERT( size > 0 && size < 0x100 );
        //SYS_ASSERT( (size % 2) == 0 );
        //{
        //    int tmp = size;
        //    while ( tmp > 1 )
        //    {
        //        SYS_ASSERT( (tmp % 2) == 0 && "size must be power of 2 number" );
        //        tmp /= 2;
        //    }
        //}

        bxVoxel_Octree* octree = BX_NEW( alloc, bxVoxel_Octree );

        //u32 rootIndex = _Octree_allocateNode( octree );
        //bxVoxel_Octree::Node& root = octree->nodes[rootIndex];
        //_Octree_initNode( &root, size );

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
            const Vector3& nodeBegin = node.center; //( xyz_to_m128( node.center.xyz ) );
            const Vector3 nodeSize = node.size.getXYZ(); //( xyz_to_m128( node.size.xyz ) );
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


        void _Octree_createChildNode( bxVoxel_Octree* voct, u32 nodeIndex, u32 ichild, const Vector3& offsetFromCenter )
        {
            //const Vector3 offsetFromCenter( -1.f, -1.f, -1.f );
            const Vector3& nodeCenter = voct->nodes[nodeIndex].center;
            const Vector3 childSize = voct->nodes[nodeIndex].size.getXYZ() * halfVec;
            const u32 index = _Octree_allocateNode( voct );
            bxVoxel_Octree::Node& childNode = voct->nodes[index];
            _Octree_initNode( &childNode, childSize );
            childNode.center = nodeCenter + mulPerElem( offsetFromCenter, childSize );
            voct->nodes[nodeIndex].children[ichild] = index;
        }

        int _Octree_insertR( bxVoxel_Octree* voct, u32 nodeIndex, const Vector3& point, size_t data )
        {
            const bxAABB bbox = _Octree_nodeAABB( voct->nodes[nodeIndex] );
            const bool insideBBox = testAABB( bbox.min.get128(), bbox.max.get128(), point.get128() );

            if ( !insideBBox )
                return -1;

            int result = -1;

            const Vector3 nodeSize = voct->nodes[nodeIndex].size.getXYZ();
            const float nodeVolume = voct->nodes[nodeIndex].size.getW().getAsFloat();
            const Vector3 nodeXYZ = voct->nodes[nodeIndex].center;

            if ( nodeVolume <= ( 1.f + FLT_EPSILON ) )
            {
                u32 dataIndex = _Octree_allocateData( voct );
                voct->nodes[nodeIndex].dataIndex = dataIndex;
                voct->data[dataIndex] = data;
                result = 1;
            }
            else
            {
                if ( voct->nodes[nodeIndex].children[0] == 0xFFFFFFFF )
                {
                    //bxGrid grid( 2, 2, 2 );

                    //const float3_t childSize = nodeSize * 0.5f;

                    {
                        _Octree_createChildNode( voct, nodeIndex, 0, Vector3(-1.f, -1.f, -1.f) );
                        _Octree_createChildNode( voct, nodeIndex, 1, Vector3( 1.f, -1.f, -1.f) );
                        _Octree_createChildNode( voct, nodeIndex, 2, Vector3( 1.f,  1.f, -1.f) );
                        _Octree_createChildNode( voct, nodeIndex, 3, Vector3(-1.f,  1.f, -1.f) );
                        _Octree_createChildNode( voct, nodeIndex, 4, Vector3(-1.f, -1.f,  1.f) );
                        _Octree_createChildNode( voct, nodeIndex, 5, Vector3( 1.f, -1.f,  1.f) );
                        _Octree_createChildNode( voct, nodeIndex, 6, Vector3( 1.f,  1.f,  1.f) );
                        _Octree_createChildNode( voct, nodeIndex, 7, Vector3(-1.f,  1.f,  1.f) );
                        //const Vector3 offsetFromCenter( -1.f, -1.f, -1.f );
                        //
                        //u32 index = _Octree_allocateNode( voct );
                        //bxVoxel_Octree::Node& childNode = voct->nodes[index];
                        //_Octree_initNode( &childNode, childSize );
                        //childNode.center = nodeXYZ + mulPerElem( offsetFromCenter, childSize );
                        //voct->nodes[nodeIndex].children[ichild] = index;
                    }

                    //for ( int ichild = 0; ichild < 8; ++ichild )
                    //{
                    //    u32 xyz[3];
                    //    grid.coords( xyz, ichild );

                    //    
                    //    childNode.x = xyz[0] * childSize + nodeX;
                    //    childNode.y = xyz[1] * childSize + nodeY;
                    //    childNode.z = xyz[2] * childSize + nodeZ;

                    //    voct->nodes[nodeIndex].children[ichild] = index;
                    //}
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

        //const u32 size = voct->nodes[0].size;

        array::clear( voct->nodes );
        array::clear( voct->data );

        //u32 rootIndex = _Octree_allocateNode( voct );
        //bxVoxel_Octree::Node& root = voct->nodes[rootIndex];
        //_Octree_initNode( &root, size );
    }

    void octree_init( bxVoxel_Octree* voct, const Vector3& center, const Vector3& size )
    {
        octree_clear( voct );
        u32 rootIndex = _Octree_allocateNode( voct );
        bxVoxel_Octree::Node& root = voct->nodes[rootIndex];
        _Octree_initNode( &root, size );
        root.center = center;
    }

    //void octree_build( bxVoxel_Octree* voct, const bxVoxel_Map* m )
    //{
    //    bxAABB bbox = bxAABB::prepare();

    //    hashmap::iterator mapIt( *m );
    //    hashmap_t::cell_t* cell = mapIt.next();
    //    while( cell )
    //    {
    //        bbox
    //        cell = mapIt.next();
    //    }
    //}

    namespace
    {
        //int _Octree_checkCellR( const bxVoxel_Octree* voct, u32 nodeIndex, const Vector3& point )
        //{
        //    const bxVoxel_Octree::Node& node = voct->nodes[nodeIndex];
        //    const bxAABB bbox = _Octree_nodeAABB( node );
        //    const bool insideBBox = testAABB( bbox.min.get128(), bbox.max.get128(), point.get128() );

        //    int result = -1;
        //    if ( !insideBBox )
        //        return result;

        //    if ( node.size == 1 )
        //    {
        //        result = (node.dataIndex > 0) ? 1 : -1;
        //    }
        //    else if ( node.children[0] != 0xFFFFFFFF )
        //    {
        //        for ( int ichild = 0; ichild < 8; ++ichild )
        //        {
        //            result = _Octree_checkCellR( voct, node.children[ichild], point );
        //            if ( result > 0 )
        //                break;
        //        }
        //    }

        //    return result;
        //}

        union MapKey
        {
            size_t hash64;
            u32 hash32[2];
            struct
            {
                i8 x, y, z, w;
                u32 _padd;
            };
        };

        inline size_t _Map_createKey( i32 x, i32 y, i32 z )
        {
            MapKey mk;
            mk.x = x;
            mk.y = y;
            mk.z = z;
            mk.w = -1;
            mk._padd = 0;
            return mk.hash64;
            //return ( 255 << 24 ) | ((i8)x << 16) | ((i8)y << 8) | z;
        }
        inline void _Map_keyToXYZ( i32 xyz[3], size_t key )
        {
            //MapKey mk = { key };
            MapKey mk = { key };
            xyz[0] = mk.x;
            xyz[1] = mk.y;
            xyz[2] = mk.z;
        }
        inline bxVoxel_GpuData _Map_createVoxelData( i32 xyz[3], u8 colorIndex )
        {
            bxVoxel_GpuData gpuData;
            gpuData.x = (i8)xyz[0];
            gpuData.y = (i8)xyz[1];
            gpuData.z = (i8)xyz[2];
            gpuData.colorIndex = colorIndex;

            return gpuData;
        }
        //inline bxVoxel_GpuData _Map_createVoxelData( i32 xyz[3], u32 colorRGBA )
        //{
        //    MapKey key;
        //    key.x = xyz[0];
        //    key.y = xyz[1];
        //    key.z = xyz[2];
        //    key.w = 0;
        //    
        //    bxVoxel_GpuData vx;
        //    vx.gridIndex = key.hash32[0];
        //    vx.colorRGBA = colorRGBA;
        //    return vx;
        //}
    }///
    void map_insert( bxVoxel_Map* m, const Vector3& point, size_t data )
    {
        const __m128i rounded = _mm_cvtps_epi32( _mm_round_ps( point.get128(), _MM_FROUND_NINT ) );
        const SSEScalar tmp( rounded );
        const size_t key = _Map_createKey( tmp.ix, tmp.iy, tmp.iz );
        hashmap_t::cell_t* cell = hashmap::insert( *m, key );
        cell->value = data;
    }
    void map_clear( bxVoxel_Map* m )
    {
        hashmap::clear( *m );
    }



    void map_getShell( array_t<bxVoxel_GpuData>& vxData, const bxVoxel_Map& map )
    {
        array::reserve( vxData, (int)map.size );
        vxData.size = map_getShell( array::begin( vxData ), array::capacity( vxData ), map, NULL );
    }

    int map_getShell( bxVoxel_GpuData* vxData, int xvDataCapacity, const bxVoxel_Map& map, bxAABB* bbox )
    {
        //const u32 gridSize = voct->nodes[0].size;
        //bxGrid grid( gridSize, gridSize, gridSize );

        bxAABB aabb = bxAABB::prepare();

        int vxDataSize = 0;

        hashmap::iterator cellIt( (hashmap_t&)map );
        hashmap_t::cell_t* cell = cellIt.next();
        while( cell )
        {
            int centerXYZ[3];
            _Map_keyToXYZ( centerXYZ, cell->key );

            int emptyFound = 0;
            for ( int iz = -1; iz <= 1 && !emptyFound; iz += 1 )
            {
                for ( int iy = -1; iy <= 1 && !emptyFound; iy += 1 )
                {
                    for ( int ix = -1; ix <= 1 && !emptyFound; ix += 1 )
                    {
                        const size_t key = _Map_createKey( centerXYZ[0] + ix, centerXYZ[1] + iy, centerXYZ[2] + iz );
                        emptyFound = hashmap::lookup( map, key ) == 0;
                    }
                }
            }

            if ( emptyFound )
            {
                vxData[vxDataSize++] = _Map_createVoxelData( centerXYZ, (u32)cell->value );

                const Vector3 pos( (float)centerXYZ[0], (float)centerXYZ[1], (float)centerXYZ[2] );
                aabb = bxAABB::extend( aabb, pos );
            }

            cell = cellIt.next();
        }

        if( bbox )
        {
            bbox[0] = aabb;
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
