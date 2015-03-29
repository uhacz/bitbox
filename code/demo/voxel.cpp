#include "voxel.h"

#include <util/debug.h>
#include <util/memory.h>
#include <util/pool_allocator.h>
#include <util/array.h>
#include <util/bbox.h>
#include <util/buffer_utils.h>
#include <gfx/gfx_debug_draw.h>
#include <gdi/gdi_backend.h>
#include <gdi/gdi_context.h>
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>

#include "voxel_octree.h"
#include "grid.h"

struct bxVoxel_InternalObject
{
    bxVoxel_Object _obj;
    bxVoxel_ObjectId _id;
};

struct bxVoxel_Manager
{
    //typedef bxHandleManager<u32> Indices;
    struct Data
    {
        Matrix4*          worldPose;
        bxVoxel_Octree**  octree;
        bxGdiBuffer*      voxelData;
        bxVoxel_InternalObject*   object;

        i32 capacity;
        i32 size;

        void* _memoryHandle;
    };

    Data _data;
    bxAllocator* _alloc_main;
    bxAllocator* _alloc_octree;
};


struct bxVoxel_Gfx
{
    bxGdiShaderFx_Instance* fxI;
    bxGdiRenderSource* rsource;
};

struct bxVoxel_Context
{
    bxVoxel_Manager menago;
    bxVoxel_Gfx gfx;
};


namespace bxVoxel
{
    bxVoxel_Manager* manager( bxVoxel_Context* ctx ) { return &ctx->menago; }
    bxVoxel_Gfx*     gfx( bxVoxel_Context* ctx ) { return &ctx->gfx; }
    ////
    ////
    void _Manager_startup( bxVoxel_Manager* man )
    {
        memset( &man->_data, 0x00, sizeof( bxVoxel_Manager::Data ) );
        man->_alloc_main = bxDefaultAllocator();
        bxDynamicPoolAllocator* pool = BX_NEW( bxDefaultAllocator(), bxDynamicPoolAllocator );
        pool->startup( sizeof( bxVoxel_Octree ), 64, bxDefaultAllocator() );
        man->_alloc_octree = pool;
    }
    void _Manager_shutdown( bxVoxel_Manager* man )
    {
        SYS_ASSERT( man->_data.size == 0 );

        BX_FREE0( man->_alloc_main, man->_data._memoryHandle );

        bxDynamicPoolAllocator* pool = (bxDynamicPoolAllocator*)man->_alloc_octree;
        pool->shutdown();
        man->_alloc_main = 0;
    }
    ////
    ////
    void _Gfx_startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Gfx* gfx )
    {
        gfx->fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "voxel" );
        {
            bxPolyShape polyShape;
            bxPolyShape_createBox( &polyShape, 1 );
            gfx->rsource = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
            bxPolyShape_deallocateShape( &polyShape );
        }
    }
    void _Gfx_shutdown( bxGdiDeviceBackend* dev, bxVoxel_Gfx* gfx )
    {
        bxGdi::shaderFx_releaseWithInstance( dev, &gfx->fxI );
        bxGdi::renderSource_releaseAndFree( dev, &gfx->rsource );
    }
    ////
    ////
    bxVoxel_Context* _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        bxVoxel_Context* vctx = BX_NEW( bxDefaultAllocator(), bxVoxel_Context );
        _Manager_startup( &vctx->menago );
        _Gfx_startup( dev, resourceManager, &vctx->gfx );
    }
    void _Shutdown( bxGdiDeviceBackend* dev, bxVoxel_Context** vctx )
    {
        _Gfx_shutdown( dev, &vctx[0]->gfx );
        _Manager_shutdown( &vctx[0]->menago );

        BX_DELETE0( bxDefaultAllocator(), vctx[0] );
    }
    

    namespace
    {
        void _Manager_allocateData( bxVoxel_Manager* menago, int newCapacity )
        {
            if ( newCapacity <= menago->_data.capacity )
                return;

            bxVoxel_Manager::Data& data = menago->_data;

            int memSize = 0;
            memSize += newCapacity * sizeof( *data.worldPose );
            memSize += newCapacity * sizeof( *data.octree );
            memSize += newCapacity * sizeof( *data.voxelData );
            memSize += newCapacity * sizeof( *data.object );

            void* mem = BX_MALLOC( menago->_alloc_main, memSize, 16 );
            memset( mem, 0x00, memSize );

            bxVoxel_Manager::Data newData;
            memset( &newData, 0x00, sizeof( bxVoxel_Manager::Data ) );

            bxBufferChunker chunker( mem, memSize );
            newData.worldPose = chunker.add<Matrix4>( newCapacity );
            newData.octree = chunker.add<bxVoxel_Octree*>( newCapacity );
            newData.voxelData = chunker.add<bxGdiBuffer>( newCapacity );
            newData.object = chunker.add<bxVoxel_InternalObject>( newCapacity );
            chunker.check();
            newData._memoryHandle = mem;
            newData.size = data.size;
            newData.capacity = newCapacity;

            if ( data._memoryHandle )
            {
                memcpy( newData.worldPose, data.worldPose, data.size * sizeof( *data.worldPose ) );
                memcpy( newData.octree, data.octree, data.size * sizeof( *data.octree ) );
                memcpy( newData.voxelData, data.voxelData, data.size * sizeof( *data.voxelData ) );
                memcpy( newData.object, data.object, data.size * sizeof( *data.object ) );

                BX_FREE0( menago->_alloc_main, data._memoryHandle );
            }

            menago->_data = newData;
        }
    
        bxGdiBuffer _Gfx_createVoxelDataBuffer( bxGdiDeviceBackend* dev, int gridSize )
        {
            const int maxVoxels = gridSize*gridSize*gridSize;
            const bxGdiFormat format( bxGdi::eTYPE_UINT, 2 );
            bxGdiBuffer buff = dev->createBuffer( maxVoxels, format, bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
            return buff;
        }
    }///



    bxVoxel_Object* object_new( bxGdiDeviceBackend* dev, bxVoxel_Manager* menago, int gridSize )
    {
        if( menago->_data.size + 1 > menago->_data.capacity )
        {
            const int newCapacity = menago->_data.capacity + 64;
            _Manager_allocateData( menago, newCapacity );
        }
        bxVoxel_Manager::Data& data = menago->_data;
        const int index = data.size;

        data.worldPose[index] = Matrix4::identity();
        data.octree[index] = octree_new( gridSize, menago->_alloc_octree );
        data.voxelData[index] = _Gfx_createVoxelDataBuffer( dev, gridSize );

        bxVoxel_InternalObject* obj = &data.object[index]; alokacja interlan objects!!
        obj->_id.index = index;

        ++data.size;
    }

    void object_delete( bxVoxel_Manager* menago, bxVoxel_Object** vobj )
    {
        bxVoxel_Manager::Data& data = menago->_data;
        const int lastIndex = data.size - 1;
        SYS_ASSERT( lastIndex >= 0 );

        bxVoxel_InternalObject* iobj = (bxVoxel_InternalObject*)vobj[0];
        const int index = iobj->_id.index;
    }
}
