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
#include "gfx/gfx_camera.h"

struct bxVoxel_ObjectData
{
    i32 numShellVoxels;
    u32 gridSize;
};

struct bxVoxel_Manager
{
    struct Data
    {
        Matrix4*            worldPose;
        bxVoxel_Octree**    octree;
        bxVoxel_ObjectData* voxelDataObj;
        bxGdiBuffer*        voxelDataGpu;
        bxVoxel_ObjectId*   indices;

        i32 capacity;
        i32 size;
        i32 _freeList;
        void* _memoryHandle;
    };

    Data _data;
    bxAllocator* _alloc_main;
    bxAllocator* _alloc_octree;

    bxVoxel_Manager()
        : _alloc_main(0)
        , _alloc_octree(0)
    {
        memset( &_data, 0x00, sizeof(bxVoxel_Manager::Data) );
    }
};


struct bxVoxel_Gfx
{
    bxGdiShaderFx_Instance* fxI;
    bxGdiRenderSource* rsource;

    bxVoxel_Gfx()
        : fxI(0)
        , rsource(0)
    {}
};

struct bxVoxel_Context
{
    bxVoxel_Manager menago;
    bxVoxel_Gfx gfx;

    bxVoxel_Context() {}
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
        man->_data._freeList = -1;

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
        return vctx;
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
            memSize += newCapacity * sizeof( *data.voxelDataObj );
            memSize += newCapacity * sizeof( *data.voxelDataGpu );
            memSize += newCapacity * sizeof( *data.indices );

            void* mem = BX_MALLOC( menago->_alloc_main, memSize, 16 );
            memset( mem, 0x00, memSize );

            bxVoxel_Manager::Data newData;
            memset( &newData, 0x00, sizeof( bxVoxel_Manager::Data ) );

            bxBufferChunker chunker( mem, memSize );
            newData.worldPose    = chunker.add<Matrix4>( newCapacity );
            newData.octree       = chunker.add<bxVoxel_Octree*>( newCapacity );
            newData.voxelDataObj = chunker.add<bxVoxel_ObjectData>( newCapacity );
            newData.voxelDataGpu = chunker.add<bxGdiBuffer>( newCapacity );
            newData.indices      = chunker.add<bxVoxel_ObjectId>( newCapacity );
            chunker.check();
            newData._memoryHandle = mem;
            newData.size = data.size;
            newData.capacity = newCapacity;
            newData._freeList = data._freeList;

            if ( data._memoryHandle )
            {
                memcpy( newData.worldPose, data.worldPose, data.size * sizeof( *data.worldPose ) );
                memcpy( newData.octree, data.octree, data.size * sizeof( *data.octree ) );
                memcpy( newData.voxelDataObj, data.voxelDataObj, data.size * sizeof( *data.voxelDataObj ) );
                memcpy( newData.voxelDataGpu, data.voxelDataGpu, data.size * sizeof( *data.voxelDataGpu ) );
                memcpy( newData.indices, data.indices, data.size * sizeof( *data.indices ) );

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



    bxVoxel_ObjectId object_new( bxGdiDeviceBackend* dev, bxVoxel_Manager* menago, int gridSize )
    {
        if( menago->_data.size + 1 > menago->_data.capacity )
        {
            const int newCapacity = menago->_data.capacity + 64;
            _Manager_allocateData( menago, newCapacity );
        }
        bxVoxel_Manager::Data& data = menago->_data;
        const int dataIndex = data.size;

        data.worldPose[dataIndex] = Matrix4::identity();
        data.octree[dataIndex] = octree_new( gridSize, menago->_alloc_octree );
        data.voxelDataGpu[dataIndex] = _Gfx_createVoxelDataBuffer( dev, gridSize );

        int iobjIndex = -1;
        if( data._freeList == -1 )
        {
            iobjIndex = dataIndex;
            data.indices[iobjIndex].generation = 1;
        }
        else
        {
            iobjIndex = data._freeList;
            data._freeList = data.indices[iobjIndex].index;
        }
        ++data.size;
        
        bxVoxel_ObjectId& objIndex = data.indices[iobjIndex];
        objIndex.index = dataIndex;

        bxVoxel_ObjectId id;
        id.index = iobjIndex;
        id.generation = objIndex.generation;
        
        return id;
    }

    void object_delete( bxVoxel_Manager* menago, bxVoxel_ObjectId* id )
    {
        bxVoxel_Manager::Data& data = menago->_data;
        if( data.indices[id->index].generation != id->generation )
        {
            return;
        }
        
        const int handleIndex = id->index;
        const int dataIndex = data.indices[handleIndex].index;
        
        const int lastDataIndex = data.size - 1;
        SYS_ASSERT( lastDataIndex >= 0 );

        int lastHandleIndex = -1;
        for( int i = 0; i < data.size; ++i )
        {
            if( data.indices[i].index == lastDataIndex )
            {
                lastHandleIndex = i;
                break;
            }
        }
        
        data.indices[lastHandleIndex].index = dataIndex;
        data.indices[handleIndex].index = data._freeList;
        data._freeList = handleIndex;
        data.indices[handleIndex].generation += 1;
        data.worldPose[dataIndex] = data.worldPose[lastDataIndex];
        data.octree[dataIndex] = data.octree[lastDataIndex];
        data.voxelDataGpu[dataIndex] = data.voxelDataGpu[lastDataIndex];

        --data.size;
        id->index = 0;
        id->generation = 0;
    }

    bool object_valid( bxVoxel_Manager* menago, bxVoxel_ObjectId id )
    {
        return ( id.index < (u32)menago->_data.size ) && ( menago->_data.indices[id.index].generation == id.generation );
    }

    bxVoxel_Octree* object_octree( bxVoxel_Manager* menago, bxVoxel_ObjectId id )
    {
        return menago->_data.octree[ menago->_data.indices[id.index].index ];
    }

    const Matrix4& object_pose( bxVoxel_Manager* menago, bxVoxel_ObjectId id )
    {
        return menago->_data.worldPose[ menago->_data.indices[id.index].index ];
    }

    void object_setPose( bxVoxel_Manager* menago, bxVoxel_ObjectId id, const Matrix4& pose )
    {
        menago->_data.worldPose[ menago->_data.indices[id.index].index ] = pose;
    }

    void object_upload( bxGdiContextBackend* ctx, bxVoxel_Manager* menago, bxVoxel_ObjectId id )
    {
        SYS_ASSERT( object_valid( menago, id ) );

        const bxVoxel_ObjectId* indices = menago->_data.indices;
        const int dataIndex = indices[ id.index ].index;

        bxVoxel_Octree* octree = object_octree( menago, id );
        bxGdiBuffer buff = menago->_data.voxelDataGpu[ dataIndex ];
        const int vxDataCapacity = (int)octree->map.size;
        bxVoxel_GpuData* vxData =  (bxVoxel_GpuData*)bxGdi::buffer_map( ctx, buff, 0, vxDataCapacity );

        const int numShellVoxels = octree_getShell( vxData, vxDataCapacity, octree );

        ctx->unmap( buff.rs );


        bxVoxel_ObjectData& objData = menago->_data.voxelDataObj[dataIndex];
        objData.numShellVoxels = numShellVoxels;
        objData.gridSize = octree->nodes[0].size;
    }

    void gfx_draw( bxGdiContext* ctx, bxVoxel_Context* vctx, const bxGfxCamera& camera )
    {
        bxVoxel_Manager* menago = &vctx->menago;
        const bxVoxel_Manager::Data& data = menago->_data;
        const int numObjects = data.size;

        if( !numObjects )
            return;

        const bxGdiBuffer* vxDataGpu = data.voxelDataGpu;
        const bxVoxel_ObjectData* vxDataObj = data.voxelDataObj;
        const Matrix4* worldMatrices = data.worldPose;

        bxGdiShaderFx_Instance* fxI = vctx->gfx.fxI;
        bxGdiRenderSource* rsource = vctx->gfx.rsource;

        fxI->setUniform( "_viewProj", camera.matrix.viewProj );
        
        bxGdi::shaderFx_enable( ctx, fxI, 0 );
        bxGdi::renderSource_enable( ctx, rsource );
        const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );

        for( int iobj = 0; iobj < numObjects; ++iobj )
        {
            const bxVoxel_ObjectData& objData = vxDataObj[iobj];
            const int nInstancesToDraw = objData.numShellVoxels;
            if( nInstancesToDraw <= 0 )
                continue;

            //const u32 gridSize = objData.gridSize;
            //fxI->setUniform( "_gridSize", gridSize );
            //fxI->setUniform( "_gridSizeSqr", gridSize * gridSize );
            //fxI->setUniform( "_gridSizeInv", 1.0f / (float)gridSize );
            fxI->setUniform( "_world", worldMatrices[iobj] );
            fxI->uploadCBuffers( ctx->backend() );

            ctx->setBufferRO( vxDataGpu[iobj], 0, bxGdi::eSTAGE_MASK_VERTEX );
            bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, nInstancesToDraw );
        }
    }

}
