#include "voxel.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/containers.h>
#include <util/bbox.h>
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

            Vector4* pos_size;
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

        int nodeAlloc( const Vector3 pos, float size, uptr data = 0 )
        {
            if( _data.size >= _data.capacity )
                return -1;

            int index = _data.size++;
            _data.pos_size[index] = Vector4( pos, size );
            _data.nodes_data[index] = octreeNodeDataMake( data );
            memset( _data.children + index, 0xff, sizeof( OctreeChild ) );
            return index;
        }

        bxAABB nodeAABB( int index )
        {
            const Vector4& pos_size = _data.pos_size[index];
            const Vector3 pos = pos_size.getXYZ();
            Vector3 ext( _data.pos_size[index].getW() * halfVec );
            return bxAABB( pos - ext, pos + ext );
        }

    };

    void octreeCreate( Octree** octPtr, float size )
    {
        Octree* oct = BX_NEW( bxDefaultAllocator(), Octree );
        oct->dataAlloc( 64 + 1 );
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
            const Vector3 childCenter = nodeCenter + offsetFromCenter * childSize;

            const int index = oct->nodeAlloc( childCenter, childSize.getAsFloat() );
            oct->_data.children[nodeIndex].index_flat[ichild] = index;
        }

        void octreePointInsertR( int* outNodeIndex, Octree* oct, int currentNodeIndex, const Vector3& point, uptr data, float nodeSizeThreshold )
        {
            if( *outNodeIndex != -1 )
                return;

            const bxAABB nodeAABB = oct->nodeAABB( currentNodeIndex );
            const bool collision = bxAABB::isPointInside( nodeAABB, point );
            if( !collision )
                return;

            const Vector4& pos_size = oct->_data.pos_size[currentNodeIndex];
            const float nodeSize = pos_size.getW().getAsFloat();
           
            if( collision && ( nodeSize <= nodeSizeThreshold ) )
            {
                outNodeIndex[0] = currentNodeIndex;
                oct->_data.nodes_data[currentNodeIndex].value = data;
                return;
            }
            
            OctreeChild& children = oct->_data.children[currentNodeIndex];
            if( children.index_flat[0] == UINT16_MAX )
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
    }
}///

namespace bx
{
    


    struct VoxelContainer
    {
        
    };
}////
