#include "voxel.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/array.h>
#include <util/bbox.h>
#include <util/collision.h>
#include <gfx/gfx_debug_draw.h>

#include <string.h>

//#include <gdi/gdi_backend.h>
//#include <gdi/gdi_context.h>
//#include <gdi/gdi_render_source.h>
//#include <gdi/gdi_shader.h>
//
////#include <gfx/gfx_camera.h>
//#include "voxel_gfx.h"
//#include "voxel_container.h"
//
//namespace bxVoxel
//{
//    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
//    {
//		gfx_startup(dev, resourceManager);
//    }
//
//    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
//    {
//		gfx_shutdown(dev, resourceManager);
//    }
//
//  //  void gfx_draw( bxGdiContext* ctx, bxVoxel_Container* cnt, const bxGfxCamera& camera )
//  //  {
//  //      //bxVoxel_Container* menago = &vctx->menago;
//  //      const bxVoxel_Container::Data& data = cnt->_data;
//  //      const int numObjects = data.size;
//
//  //      if( !numObjects )
//  //          return;
//
//  //      const bxGdiBuffer* vxDataGpu = data.voxelDataGpu;
//  //      const bxVoxel_ObjectData* vxDataObj = data.voxelDataObj;
//  //      const Matrix4* worldMatrices = data.worldPose;
//
//		//bxVoxel_Gfx* vgfx = bxVoxel_Gfx::instance();
//
//  //      bxGdiShaderFx_Instance* fxI = vgfx->fxI;
//  //      bxGdiRenderSource* rsource = vgfx->rsource;
//
//  //      fxI->setUniform( "_viewProj", camera.matrix.viewProj );
//  //      
//  //      bxGdi::shaderFx_enable( ctx, fxI, 0 );
//  //      bxGdi::renderSource_enable( ctx, rsource );
//  //      const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
//  //      ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ), 0, bxGdi::eSTAGE_MASK_PIXEL );
//
//  //      for( int iobj = 0; iobj < numObjects; ++iobj )
//  //      {
//  //          const bxVoxel_ObjectData& objData = vxDataObj[iobj];
//  //          const int nInstancesToDraw = objData.numShellVoxels;
//  //          if( nInstancesToDraw <= 0 )
//  //              continue;
//
//  //          fxI->setUniform( "_world", worldMatrices[iobj] );
//  //          fxI->uploadCBuffers( ctx->backend() );
//
//  //          ctx->setBufferRO( vxDataGpu[iobj], 0, bxGdi::eSTAGE_MASK_VERTEX );
//		//	ctx->setTexture( vgfx->colorPalette_texture[objData.colorPalette], 0, bxGdi::eSTAGE_MASK_PIXEL);
//
//  //          bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, nInstancesToDraw );
//  //      }
//  //  }
//}

namespace bx
{
    OctreeNodeData octreeNodeDataMake( uptr data )
    {
        OctreeNodeData ond;
        ond.value = data;
        return ond;
    }
    union OctreeChild
    {
        u16 index_flat[8];
        u16 index[2][2][2];
    };
    struct Octree
    {
        static const int ROOT_INDEX = 0;
        //// 
        struct Data
        {
            i32 size = 0;
            i32 capacity = 0;
            void* memory_handle = nullptr;

            Vector4* pos_size = nullptr;
            OctreeNodeData* nodes_data = nullptr;
            OctreeChild* children = nullptr;
        } _data;

        array_t<u16> _leaves;

        void dataAlloc( int newCapacity )
        {
            int mem_size = 0;
            mem_size += newCapacity * sizeof( *_data.pos_size );
            mem_size += newCapacity * sizeof( *_data.nodes_data );
            mem_size += newCapacity * sizeof( *_data.children );

            int size = _data.size;
            Data newData;

            void* mem_handle = BX_MALLOC( bxDefaultAllocator(), mem_size, 16 );
            newData.memory_handle = mem_handle;
            newData.size = size;
            newData.capacity = newCapacity;

            bxBufferChunker chunker( mem_handle, mem_size );
            newData.pos_size = chunker.add< Vector4 >( newCapacity );
            newData.nodes_data = chunker.add< OctreeNodeData >( newCapacity );
            newData.children = chunker.add< OctreeChild >( newCapacity );

            chunker.check();

            if( size )
            {
                BX_CONTAINER_COPY_DATA( &newData, &_data, pos_size );
                BX_CONTAINER_COPY_DATA( &newData, &_data, nodes_data );
                BX_CONTAINER_COPY_DATA( &newData, &_data, children );
            }

            BX_FREE0( bxDefaultAllocator(), _data.memory_handle );
            _data = newData;
        }
        void dataFree()
        {
            BX_FREE( bxDefaultAllocator(), _data.memory_handle );
            _data = Data();
        }

        int nodeAlloc( const Vector3 pos, float size, uptr data = UINT64_MAX )
        {
            if( _data.size >= _data.capacity )
            {
                dataAlloc( _data.capacity * 2 );
            }
            int index = _data.size++;
            _data.pos_size[index] = Vector4( pos, size );
            _data.nodes_data[index] = octreeNodeDataMake( data );
            memset( &_data.children[index], 0xff, sizeof( OctreeChild ) );
            return index;
        }

        static bxAABB nodeAABB( const Data& data, int index )
        {
            const Vector4& pos_size = data.pos_size[index];
            const Vector3 pos = pos_size.getXYZ();
            Vector3 ext( data.pos_size[index].getW() * halfVec );
            return bxAABB( pos - ext, pos + ext );
        }

    };

    void octreeCreate( Octree** octPtr, float size )
    {
        Octree* oct = BX_NEW( bxDefaultAllocator(), Octree );
        oct->dataAlloc( 255 + 1 );
        oct->nodeAlloc( Vector3( 0.f ), size );
        octPtr[0] = oct;
    }
    void octreeDestroy( Octree** octPtr )
    {
        octPtr[0]->dataFree();
        BX_DELETE0( bxDefaultAllocator(), octPtr[0] );
    }

    namespace
    {
        void octreeCreateChildNode( Octree* oct, int nodeIndex, int ichild, const Vector3 offsetFromCenter )
        {
            const Vector3 nodeCenter = oct->_data.pos_size[nodeIndex].getXYZ();
            const floatInVec childSize = oct->_data.pos_size[nodeIndex].getW() * halfVec;
            const Vector3 childCenter = nodeCenter + offsetFromCenter * childSize * halfVec;

            const int index = oct->nodeAlloc( childCenter, childSize.getAsFloat() );
            SYS_ASSERT( index >= 0 );
            oct->_data.children[nodeIndex].index_flat[ichild] = index;
        }

        static inline int pointInAABBf4( const vec_float4 bboxMin, const vec_float4 bboxMax, const vec_float4 point )
        {
            const vec_float4 a = vec_cmple( bboxMin, point );
            const vec_float4 b = vec_cmpge( bboxMax, point );
            const vec_float4 a_n_b = vec_and( a, b );
            const vec_float4 r = vec_and( vec_splat( a_n_b, 0 ), vec_and( vec_splat( a_n_b, 1 ), vec_splat( a_n_b, 2 ) ) );
            return _mm_movemask_ps( r );
        }

        void octreePointInsertR( int* outNodeIndex, Octree* oct, int currentNodeIndex, const Vector3 point, uptr data, float nodeSizeThreshold )
        {
            //if( *outNodeIndex != -1 )
            //    return;

            const bxAABB nodeAABB = Octree::nodeAABB( oct->_data, currentNodeIndex );
            const int collision = pointInAABBf4( nodeAABB.min.get128(), nodeAABB.max.get128(), point.get128() );
            if( !collision )
                return;

            const Vector4& pos_size = oct->_data.pos_size[currentNodeIndex];
            const float nodeSize = pos_size.getW().getAsFloat();
           
            if( nodeSize <= nodeSizeThreshold )
            {
                outNodeIndex[0] = currentNodeIndex;
                oct->_data.nodes_data[currentNodeIndex].value = data;
                return;
            }
            
            if( oct->_data.children[currentNodeIndex].index_flat[0] == UINT16_MAX )
            {
                octreeCreateChildNode( oct, currentNodeIndex, 0, Vector3( 1.f,-1.f,-1.f ) );
                octreeCreateChildNode( oct, currentNodeIndex, 1, Vector3(-1.f,-1.f,-1.f ) );
                octreeCreateChildNode( oct, currentNodeIndex, 2, Vector3( 1.f, 1.f,-1.f ) );
                octreeCreateChildNode( oct, currentNodeIndex, 3, Vector3(-1.f, 1.f,-1.f ) );
                octreeCreateChildNode( oct, currentNodeIndex, 4, Vector3( 1.f,-1.f, 1.f ) );
                octreeCreateChildNode( oct, currentNodeIndex, 5, Vector3(-1.f,-1.f, 1.f ) );
                octreeCreateChildNode( oct, currentNodeIndex, 6, Vector3( 1.f, 1.f, 1.f ) );
                octreeCreateChildNode( oct, currentNodeIndex, 7, Vector3(-1.f, 1.f, 1.f ) );
            }

            OctreeChild& children = oct->_data.children[currentNodeIndex];
            for( int i = 0; i < 8; ++i )
            {
                octreePointInsertR( outNodeIndex, oct, children.index_flat[i], point, data, nodeSizeThreshold );
            }
        }
    }


    int octreePointInsert( Octree* oct, const Vector3 point, uptr data )
    {
        const float NODE_SIZE_THRESHOLD = 1.f + FLT_EPSILON;
        int nodeIndex = -1;
        octreePointInsertR( &nodeIndex, oct, Octree::ROOT_INDEX, point, data, NODE_SIZE_THRESHOLD );
        return nodeIndex;
    }
    OctreeNodeData octreeDataGet( Octree* oct, int nodeIndex )
    {
        if( nodeIndex < 0 || nodeIndex >= oct->_data.size )
        {
            return octreeNodeDataMake( UINT64_MAX );
        }

        return oct->_data.nodes_data[nodeIndex];
    }
    OctreeNodeData octreeDataLookup( Octree* oct, const Vector3 pos )
    {
        return octreeNodeDataMake( 0 );
    }

    Vector4 octreeNodePosSize( Octree* oct, int nodeIndex )
    {
        if( nodeIndex < 0 || nodeIndex >= oct->_data.size )
            return Vector4( -1.f );

        return oct->_data.pos_size[nodeIndex];
    }

    namespace
    {
        void octreeRaycastR( int* result, const Octree::Data& data, int currentNodeIndex, const Vector3 ro, const Vector3 rdInv, const floatInVec rayLength )
        {
            if( *result != -1 )
                return;
            
            bxAABB aabb = Octree::nodeAABB( data, currentNodeIndex );
            OctreeNodeData currentNodeData = data.nodes_data[currentNodeIndex];

            floatInVec t;
            int collision = intersectRayAABB( &t, ro, rdInv, rayLength, aabb.min, aabb.max );
            if( !collision )
                return;

            if( !currentNodeData.empty() )
            {
                result[0] = currentNodeIndex;
                return;
            }

            if( data.children[currentNodeIndex].index_flat[0] == UINT16_MAX )
                return;

            const OctreeChild& children = data.children[currentNodeIndex];
            for( int i = 0; i < 8; ++i )
            {
                octreeRaycastR( result, data, children.index_flat[i], ro, rdInv, rayLength );
            }
        }

        template< int N >
        struct OctreeRaycastStack
        {
            i32 _nodeIndex[N];
            i32 _size = 0;

            void push( int i ) { SYS_ASSERT(_size < N );  _nodeIndex[_size++] = i; }
            void pop() { SYS_ASSERT(_size > 0 ); --_size; }
            int top() const { SYS_ASSERT( !empty() ); return _nodeIndex[_size - 1]; }
            bool empty() const { return _size == 0; }
            int size() const { return _size; }
        };
        int octreeRaycastI( const Octree::Data& data, int rootNodeIndex, const Vector3 ro, const Vector3 rdInv, const floatInVec rayLength )
        {
            {
                const bxAABB rootAABB = Octree::nodeAABB( data, rootNodeIndex );
                floatInVec t;
                int collision = intersectRayAABB( &t, ro, rdInv, rayLength, rootAABB.min, rootAABB.max );
                if( !collision )
                    return -1;
            }
            int result = -1;

            OctreeRaycastStack<16> stack;
            stack.push( rootNodeIndex );
            do 
            {
                
                int currentNodeIndex = stack.top();
                const OctreeChild& children = data.children[currentNodeIndex];
                if( children.index_flat[0] == UINT16_MAX )
                    break;

                floatInVec t[8];
                int collision[8];
                for( int i = 0; i < 8; ++i )
                {
                    bxAABB aabb = Octree::nodeAABB( data, children.index_flat[i] );
                    collision[i] = intersectRayAABB( &t[i], ro, rdInv, rayLength, aabb.min, aabb.max );
                    if( collision[i] )
                    {
                        bxGfxDebugDraw::addBox( Matrix4::translation( bxAABB::center( aabb ) ), bxAABB::size( aabb ) * halfVec, 0xFF0000FF, 1 );
                    }
                }

                int bestIndex = -1;
                float bestT = FLT_MAX;
                for( int i = 0; i < 8; ++i )
                {
                    if( !collision[i] )
                        continue;

                    float ti = t[i].getAsFloat();
                    if( bestT > ti )
                    {
                        bestT = ti;
                        bestIndex = i;
                    }
                }

                if( bestIndex == -1 )
                {
                    break;
                }
                else
                {
                    int bestNodeIndex = children.index_flat[bestIndex];
                    const OctreeNodeData ndata = data.nodes_data[bestNodeIndex];
                    if( ndata.empty() )
                    {

                        stack.push( bestNodeIndex );
                    }
                    else
                    {
                        result = bestNodeIndex;
                        break;
                    }
                }
            } while (1);

            return result;
        }
    }///

    int octreeRaycast( const Octree* oct, const Vector3 ro, const Vector3 rd, const floatInVec rayLength )
    {
        const Vector3 rdInv = divPerElem( Vector3( 1.f ), rd );
        //int nodeIndex = -1;
        //octreeRaycastR( &nodeIndex, oct->_data, Octree::ROOT_INDEX, ro, rdInv, rayLength );
        int nodeIndex = octreeRaycastI( oct->_data, Octree::ROOT_INDEX, ro, rdInv, rayLength );

        return nodeIndex;
    }

    void octreeDebugDraw( Octree* oct, u32 color0 /*= 0x00FF00FF*/, u32 color1 /*= 0xFF0000FF */ )
    {
        int n = oct->_data.size;
        for( int i = 0; i < n; ++i )
        {
            //bxAABB aabb = oct->nodeAABB( i );
            const Vector4& posAndSize = oct->_data.pos_size[i];
            Vector3 aabbCenter = posAndSize.getXYZ();
            Vector3 aabbSize = Vector3( posAndSize.getW() );

            bxGfxDebugDraw::addBox( Matrix4::translation( aabbCenter ), aabbSize * 0.5f, color0, true );
            if( oct->_data.nodes_data[i].value != UINT64_MAX )
            {
                bxGfxDebugDraw::addSphere( Vector4( aabbCenter, posAndSize.getW() * halfVec ), color1, true );
            }
        }
    }

}///

namespace bx
{
    


    struct VoxelContainer
    {
        
    };



}////
