#include "voxel.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/containers.h>
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
    OctreeNodeData octreeNodeDataMake( u32 data )
    {
        OctreeNodeData ond;
        ond.value = data;
        return ond;
    }
    struct OctreeChild
    {
        u16 children[8];
    };
    struct OctreeNode
    {
        u16 child_index = 0xFFFF;
        u16 unused = 0xFFFF;
    };
    struct Octree
    {
        //// 
        struct Data
        {
            i32 size = 0;
            i32 capacity = 0;
            void* memory_handle = nullptr;

            Vector4* pos_size;
            OctreeNode* nodes = nullptr;
            OctreeNodeData* nodes_data = nullptr;
            OctreeChild* children = nullptr;
        } _data;

        array_t<u16> _leaves;

        void dataAlloc( int newCapacity )
        {
            int mem_size = 0;
            mem_size += newCapacity * sizeof( *_data.pos_size );
            mem_size += newCapacity * sizeof( *_data.nodes );
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
            newData.nodes = chunker.add< OctreeNode >( newCapacity );
            newData.nodes_data = chunker.add< OctreeNodeData >( newCapacity );
            newData.children = chunker.add< OctreeChild >( newCapacity );

            chunker.check();

            if( size )
            {
                BX_CONTAINER_COPY_DATA( &newData, &_data, pos_size );
                BX_CONTAINER_COPY_DATA( &newData, &_data, nodes );
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

        int nodeAlloc( const Vector3 pos, float size, u32 data = 0 )
        {
            if( _data.size >= _data.capacity )
                return -1;

            int index = _data.size++;
            _data.pos_size[index] = Vector4( pos, size );
            _data.nodes[index] = OctreeNode();
            _data.nodes_data[index] = octreeNodeDataMake( data );
            memset( _data.children + index, 0xff, sizeof( OctreeChild ) );
            return index;
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

    int octreePointInsert( Octree* oct, const Vector3 pos )
    {
        
    }
    OctreeNodeData octreeDataGet( Octree* oct, int nodeIndex )
    {

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
