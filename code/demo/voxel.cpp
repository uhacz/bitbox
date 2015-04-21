#include "voxel.h"

#include <util/debug.h>
#include <util/memory.h>
#include <util/pool_allocator.h>
#include <util/array.h>
#include <util/bbox.h>
#include <util/buffer_utils.h>
#include <util/string_util.h>
#include <gfx/gfx_debug_draw.h>
#include <gdi/gdi_backend.h>
#include <gdi/gdi_context.h>
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>

#include "voxel_octree.h"
#include "grid.h"
#include "gfx/gfx_camera.h"
#include "voxel_file.h"
#include <util/common.h>

struct bxVoxel_ObjectData
{
    i32 numShellVoxels;
	i32 colorPalette;
};

struct bxVoxel_ObjectAttribute
{
    float3_t pos;
    float3_t rot;
    char* file;

    bxVoxel_ObjectAttribute()
        : pos(0.f)
        , rot(0.f)
        , file(0)
    {}

    void release(){
        string::free_and_null( &file );
    }

};

struct bxVoxel_Container
{
    struct Data
    {
        Matrix4*            worldPose;
        bxAABB*             aabb;
        //bxVoxel_Octree**    octree;
        bxVoxel_Map*        map;
        bxVoxel_ObjectData* voxelDataObj;
        bxGdiBuffer*        voxelDataGpu;
        bxVoxel_ObjectAttribute* attribute;
        
        bxVoxel_ObjectId*   indices;


        i32 capacity;
        i32 size;
        i32 _freeList;
        void* _memoryHandle;
    };

    Data _data;
    bxAllocator* _alloc_main;
    bxAllocator* _alloc_octree;

    bxVoxel_Container()
        : _alloc_main(0)
        , _alloc_octree(0)
    {
        memset( &_data, 0x00, sizeof(bxVoxel_Container::Data) );
    }

    int objectIndex( bxVoxel_ObjectId id ){
        return _data.indices[id.index].index;
    }
};

struct bxVoxel_ColorPalette
{
    u32 rgba[256];
};
inline bool equal( const bxVoxel_ColorPalette& a, const bxVoxel_ColorPalette& b )
{
    return memcmp( a.rgba, b.rgba, sizeof( bxVoxel_ColorPalette ) ) == 0;
}
inline bool equal( const bxVoxel_ColorPalette& a, const u32* b )
{
    return memcmp( a.rgba, b, sizeof( bxVoxel_ColorPalette ) ) == 0;
}
static const u32 __default_palette[256] = {
    0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff, 0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
    0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff, 0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
    0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc, 0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
    0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc, 0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
    0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc, 0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
    0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999, 0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
    0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099, 0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
    0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66, 0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
    0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366, 0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
    0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33, 0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
    0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633, 0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
    0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00, 0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
    0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600, 0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
    0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000, 0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
    0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700, 0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
    0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd, 0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
};

struct bxVoxel_Gfx
{
    bxGdiShaderFx_Instance* fxI;
    bxGdiRenderSource* rsource;

    array_t<const char*> colorPalette_name;
    array_t<bxGdiTexture> colorPalette_texture;
    array_t<bxVoxel_ColorPalette> colorPalette_data;

    bxVoxel_Gfx()
        : fxI(0)
        , rsource(0)
    {}


};

//struct bxVoxel_Context
//{
//    bxVoxel_Container menago;
//    bxVoxel_Gfx gfx;
//
//    bxVoxel_Context() {}
//};

static bxVoxel_Gfx* __gfx = 0;
namespace bxVoxel
{
    //bxVoxel_Container* manager( bxVoxel_Context* ctx ) { return &ctx->menago; }
    //bxVoxel_Gfx*     gfx( bxVoxel_Context* ctx ) { return &ctx->gfx; }
    ////
    ////
    void _Container_startup( bxVoxel_Container* cnt )
    {
        memset( &cnt->_data, 0x00, sizeof( bxVoxel_Container::Data ) );
        cnt->_data._freeList = -1;

        cnt->_alloc_main = bxDefaultAllocator();
        bxDynamicPoolAllocator* pool = BX_NEW( bxDefaultAllocator(), bxDynamicPoolAllocator );
        pool->startup( sizeof( bxVoxel_Octree ), 64, bxDefaultAllocator() );
        cnt->_alloc_octree = pool;
    }
    void _Container_shutdown( bxVoxel_Container* cnt )
    {
        SYS_ASSERT( cnt->_data.size == 0 );

        BX_FREE0( cnt->_alloc_main, cnt->_data._memoryHandle );

        bxDynamicPoolAllocator* pool = (bxDynamicPoolAllocator*)cnt->_alloc_octree;
        pool->shutdown();
        cnt->_alloc_main = 0;
    }
    ////
    ////
    namespace
    {
        //int _Gfx_loadColorPalette( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Gfx* gfx, const char* name )
        //{
        //    bxFS::File file = resourceManager->readFileSync( name );
        //    if ( !file.ok() )
        //        return -1;

        //    bxGdiTexture texture = dev->createTexture( file.bin, file.size );
        //    if ( !texture.id )
        //        return -1;

        //    const int idx0 = array::push_back( gfx->colorPalette_texture, texture );
        //    


        //}
        int _Gfx_findColorPaletteByData( bxVoxel_Gfx* gfx, const u32* data )
        {
            for ( int i = 0; i < array::size( gfx->colorPalette_data ); ++i )
            {
                if( equal( gfx->colorPalette_data[i], data ) )
                {
                    return i;
                }
            }
            return -1;
        }
        int _Gfx_acquireColorPalette( bxGdiDeviceBackend* dev, bxVoxel_Gfx* gfx, const char* name, const u32* data, int dataSize )
        {
            int found = _Gfx_findColorPaletteByData( gfx, data );
            if( found != -1 )
            {
                if( !string::equal( gfx->colorPalette_name[found], name ) )
                {
                    bxLogWarning( "Color palette '%s' exists under different name: '%s'", name, gfx->colorPalette_name[found] );
                }
                return found;
            }

            const char* nameCopy = string::duplicate( 0, name );
            
            found = array::push_back( gfx->colorPalette_name, nameCopy );
            int idx = array::push_back( gfx->colorPalette_data, bxVoxel_ColorPalette() );
            SYS_ASSERT( idx == found );
            idx = array::push_back( gfx->colorPalette_texture, bxGdiTexture() );
            SYS_ASSERT( idx == found );

            bxVoxel_ColorPalette& palette = gfx->colorPalette_data[idx];
            memset( &palette, 0x00, sizeof( bxVoxel_ColorPalette ) );
            memcpy( palette.rgba, data, minOfPair( dataSize, (int)sizeof( bxVoxel_ColorPalette) ) );

			gfx->colorPalette_texture[idx] = dev->createTexture1D(256, 1, bxGdiFormat(bxGdi::eTYPE_UBYTE, 4, 1 ), bxGdi::eBIND_SHADER_RESOURCE, 0, data);
			return idx;
        }
    }
    void _Gfx_startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Gfx* gfx )
    {
        gfx->fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "voxel" );
        {
            bxPolyShape polyShape;
            bxPolyShape_createBox( &polyShape, 1 );
            gfx->rsource = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
            bxPolyShape_deallocateShape( &polyShape );
        }

        _Gfx_acquireColorPalette( dev, gfx, "default", __default_palette, sizeof( __default_palette ) );
    }
    void _Gfx_shutdown( bxGdiDeviceBackend* dev, bxVoxel_Gfx* gfx )
    {
		for (int i = 0; i < array::size(gfx->colorPalette_name); ++i)
		{
			string::free_and_null((char**)&gfx->colorPalette_name[i]);
			dev->releaseTexture(&gfx->colorPalette_texture[i]);
		}
		array::clear(gfx->colorPalette_texture);
		array::clear(gfx->colorPalette_data);
		array::clear(gfx->colorPalette_name);
		
		bxGdi::shaderFx_releaseWithInstance( dev, &gfx->fxI );
        bxGdi::renderSource_releaseAndFree( dev, &gfx->rsource );
    }
    int gfx_acquireColorPalette( bxGdiDeviceBackend* dev, const u32* data )
    {
        return _Gfx_acquireColorPalette( dev, __gfx, "data", data, 256 * sizeof( *data ) );
    }
    ////
    ////
    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        SYS_ASSERT( __gfx == 0 );
        __gfx = BX_NEW( bxDefaultAllocator(), bxVoxel_Gfx );
        _Gfx_startup( dev, resourceManager, __gfx );
    }

    void _Shutdown( bxGdiDeviceBackend* dev )
    {
        _Gfx_shutdown( dev, __gfx );
        BX_DELETE0( bxDefaultAllocator(), __gfx );
    }
    
    bxVoxel_Container* container_new()
    {
        bxVoxel_Container* cnt = BX_NEW( bxDefaultAllocator(), bxVoxel_Container );
        _Container_startup( cnt );
        return cnt;
    }

    namespace
    {
        bxGdiBuffer _Gfx_createVoxelDataBuffer( bxGdiDeviceBackend* dev, int maxVoxels )
        {
            //const int maxVoxels = gridSize*gridSize*gridSize;
            const bxGdiFormat format( bxGdi::eTYPE_UINT, 1 );
            bxGdiBuffer buff = dev->createBuffer( maxVoxels, format, bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
            return buff;
        }
        void _Gpu_uploadShell( bxGdiDeviceBackend* dev, bxVoxel_Container* cnt, int dataIndex )
        {
            bxVoxel_Container::Data& data = cnt->_data;

            bxVoxel_Map* map = &data.map[dataIndex];
            const int vxDataCapacity = (int)map->size;
            if( vxDataCapacity == 0 )
            {
                bxLogWarning( "Voxel map is empty. There is nothing to upload to GPU!!" );
                return;
            }

            bxVoxel_GpuData* tmp = (bxVoxel_GpuData*)BX_MALLOC( bxDefaultAllocator(), vxDataCapacity * sizeof( bxVoxel_GpuData ), 8 );

            const int numShellVoxels = map_getShell( tmp, vxDataCapacity, map[0] );
            const int requiredMemSize = numShellVoxels * sizeof( bxVoxel_GpuData );

            bxGdiBuffer buff = cnt->_data.voxelDataGpu[dataIndex];
            if( buff.id == 0 || buff.sizeInBytes != requiredMemSize )
            {
                dev->releaseBuffer( &buff );
                cnt->_data.voxelDataGpu[dataIndex] = _Gfx_createVoxelDataBuffer( dev, numShellVoxels );
                buff = cnt->_data.voxelDataGpu[dataIndex];
            }

            bxGdiContextBackend* ctx = dev->ctx;

            bxVoxel_GpuData* vxData = (bxVoxel_GpuData*)bxGdi::buffer_map( ctx, buff, 0, numShellVoxels );
            memcpy( vxData, tmp, numShellVoxels * sizeof( bxVoxel_GpuData ) );
            ctx->unmap( buff.rs );

            BX_FREE0( bxDefaultAllocator(), tmp );

            bxVoxel_ObjectData& objData = cnt->_data.voxelDataObj[dataIndex];
            objData.numShellVoxels = numShellVoxels;
            //objData.gridSize = octree->nodes[0].size;
        }
    }

    void container_load(bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Container* cnt)
    {
        bxVoxel_Container::Data& data = cnt->_data;
        const int n = data.size;
        for ( int i = 0; i < n; ++i )
        {
            const bxVoxel_ObjectAttribute& attribs = data.attribute[i];

            const Vector3 rotation( xyz_to_m128( attribs.rot.xyz ) );
            const Vector3 position( xyz_to_m128( attribs.pos.xyz ) );
            
            const Matrix4 pose = Matrix4( Matrix3::rotationZYX( rotation ), position );
            data.worldPose[i] = pose;

            if (attribs.file)
            {
                if( string::find( attribs.file, ".vox" ) )
                {
                    map_loadMagicaVox( dev, resourceManager, &data.map[i], attribs.file );
                }
                else if( string::find( attribs.file, ".raw" ) )
                {
                    map_loadHeightmapRaw8( resourceManager, &data.map[i], attribs.file );
                }
                else
                {
                    bxLogError( "Unsupported voxel file '%s'", attribs.file );
                }
            }
            _Gpu_uploadShell( dev, cnt, i );
        }
    }

    namespace
    {
        void object_releaseData( bxGdiDeviceBackend* dev, bxVoxel_Container::Data& data, int dataIndex )
        {
            dev->releaseBuffer( &data.voxelDataGpu[dataIndex] );
            data.map[dataIndex].~bxVoxel_Map();
            data.attribute[dataIndex].release();
        }
    }///
    void container_unload(bxGdiDeviceBackend* dev, bxVoxel_Container* cnt)
    {
        bxVoxel_Container::Data& data = cnt->_data;
        const int n = data.size;
        for( int i = 0; i < n; ++i )
        {
            object_releaseData( dev, data, i );
            data.indices[i].generation += 1;
        }

        data.size = 0;
    }

    void container_delete( bxVoxel_Container** cnt )
    {
        _Container_shutdown( cnt[0] );
        BX_DELETE0( bxDefaultAllocator(), cnt[0] );
    }

    namespace
    {
        void _Container_allocateData( bxVoxel_Container* cnt, int newCapacity )
        {
            if ( newCapacity <= cnt->_data.capacity )
                return;

            bxVoxel_Container::Data& data = cnt->_data;

            int memSize = 0;
            memSize += newCapacity * sizeof( *data.worldPose );
            memSize += newCapacity * sizeof( *data.aabb );
            //memSize += newCapacity * sizeof( *data.octree );
            memSize += newCapacity * sizeof( *data.map );
            memSize += newCapacity * sizeof( *data.voxelDataObj );
            memSize += newCapacity * sizeof( *data.voxelDataGpu );
            memSize += newCapacity * sizeof( *data.attribute );
            memSize += newCapacity * sizeof( *data.indices );

            void* mem = BX_MALLOC( cnt->_alloc_main, memSize, 16 );
            memset( mem, 0x00, memSize );

            bxVoxel_Container::Data newData;
            memset( &newData, 0x00, sizeof( bxVoxel_Container::Data ) );

            bxBufferChunker chunker( mem, memSize );
            newData.worldPose    = chunker.add<Matrix4>( newCapacity );
            newData.aabb         = chunker.add<bxAABB>( newCapacity );
            //newData.octree       = chunker.add<bxVoxel_Octree*>( newCapacity );
            newData.map          = chunker.add<bxVoxel_Map>( newCapacity );
            newData.voxelDataObj = chunker.add<bxVoxel_ObjectData>( newCapacity );
            newData.voxelDataGpu = chunker.add<bxGdiBuffer>( newCapacity );
            newData.attribute    = chunker.add<bxVoxel_ObjectAttribute>( newCapacity );
            newData.indices      = chunker.add<bxVoxel_ObjectId>( newCapacity );
            chunker.check();
            newData._memoryHandle = mem;
            newData.size = data.size;
            newData.capacity = newCapacity;
            newData._freeList = data._freeList;

            if ( data._memoryHandle )
            {
                memcpy( newData.worldPose, data.worldPose, data.size * sizeof( *data.worldPose ) );
                memcpy( newData.aabb     , data.aabb     , data.size * sizeof( *data.aabb ) );
                //memcpy( newData.octree, data.octree, data.size * sizeof( *data.octree ) );
                memcpy( newData.map   , data.map   , data.size * sizeof( *data.map ) );
                memcpy( newData.voxelDataObj, data.voxelDataObj, data.size * sizeof( *data.voxelDataObj ) );
                memcpy( newData.voxelDataGpu, data.voxelDataGpu, data.size * sizeof( *data.voxelDataGpu ) );
                memcpy( newData.attribute, data.attribute, data.size * sizeof( *data.attribute ) );
                memcpy( newData.indices, data.indices, data.size * sizeof( *data.indices ) );

                BX_FREE0( cnt->_alloc_main, data._memoryHandle );
            }

            cnt->_data = newData;
        }
    }///



    bxVoxel_ObjectId object_new( bxVoxel_Container* cnt )
    {
        if( cnt->_data.size + 1 > cnt->_data.capacity )
        {
            const int newCapacity = cnt->_data.capacity + 64;
            _Container_allocateData( cnt, newCapacity );
        }
        bxVoxel_Container::Data& data = cnt->_data;
        const int dataIndex = data.size;

        data.worldPose[dataIndex] = Matrix4::identity();
        data.aabb[dataIndex] = bxAABB( Vector3(-1.f), Vector3(1.f) );
        //data.octree[dataIndex] = 0; // octree_new( gridSize, menago->_alloc_octree );
        data.voxelDataGpu[dataIndex] = bxGdiBuffer();// _Gfx_createVoxelDataBuffer( dev, gridSize );
        data.voxelDataObj[dataIndex].colorPalette = 0;
        data.voxelDataObj[dataIndex].numShellVoxels = 0;
        new(&data.map[dataIndex]) hashmap_t();

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

    void object_delete( bxGdiDeviceBackend* dev, bxVoxel_Container* cnt, bxVoxel_ObjectId* id )
    {
        bxVoxel_Container::Data& data = cnt->_data;
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

        //BX_DELETE0( menago->_alloc_octree, data.octree[dataIndex] );
        object_releaseData( dev, data, dataIndex );

        data.indices[lastHandleIndex].index = dataIndex;
        data.indices[handleIndex].index = data._freeList;
        data._freeList = handleIndex;
        data.indices[handleIndex].generation += 1;
        data.worldPose[dataIndex] = data.worldPose[lastDataIndex];
        data.aabb[dataIndex] = data.aabb[lastDataIndex];
        //data.octree[dataIndex] = data.octree[lastDataIndex];
        data.voxelDataGpu[dataIndex] = data.voxelDataGpu[lastDataIndex];
        data.voxelDataObj[dataIndex] = data.voxelDataObj[lastDataIndex];
        data.attribute[dataIndex] = data.attribute[lastDataIndex];

        --data.size;
        id->index = 0;
        id->generation = 0;
    }

    bool object_valid( bxVoxel_Container* cnt, bxVoxel_ObjectId id )
    {
        return ( id.index < (u32)cnt->_data.size ) && ( cnt->_data.indices[id.index].generation == id.generation );
    }
    int object_setAttribute( bxVoxel_Container* cnt, bxVoxel_ObjectId id, const char* attrName, const void* attrData, unsigned attrDataSize )
    {
        if( !object_valid( cnt, id ) )
            return -1;

        bxVoxel_ObjectAttribute& attribs = cnt->_data.attribute[cnt->_data.indices[id.index].index];
        if( string::equal( attrName, "pos" ) )
        {
            if( attrDataSize < 12 )
                goto object_setAttribute_sizeError;

            memcpy( attribs.pos.xyz, attrData, 12 );
        }
        else if( string::equal( attrName, "rot" ) )
        {
            if( attrDataSize < 12 )
                goto object_setAttribute_sizeError;

            memcpy( attribs.rot.xyz, attrData, 12 );
        }
        else if( string::equal( attrName, "file" ) )
        {
            if( attrDataSize == 0 )
                goto object_setAttribute_sizeError;

            attribs.file = string::duplicate( attribs.file, (const char*)attrData );
        }

        return 0;

object_setAttribute_sizeError:
        bxLogError( "Attribute '%s' has wrong data size (%u)", attrName, attrDataSize );
        return -1;
        
    }
    //bxVoxel_Octree* object_octree( bxVoxel_Manager* menago, bxVoxel_ObjectId id )
    //{
    //    return menago->_data.octree[ menago->_data.indices[id.index].index ];
    //}

    bxVoxel_Map* object_map( bxVoxel_Container* cnt, bxVoxel_ObjectId id )
    {
        return &cnt->_data.map[ cnt->objectIndex( id ) ];
    }

    const bxAABB& object_aabb( bxVoxel_Container* cnt, bxVoxel_ObjectId id )
    {
        return cnt->_data.aabb[ cnt->objectIndex( id ) ];
    }

    const Matrix4& object_pose( bxVoxel_Container* cnt, bxVoxel_ObjectId id )
    {
        return cnt->_data.worldPose[ cnt->objectIndex( id ) ];
    }

    void object_setPose( bxVoxel_Container* cnt, bxVoxel_ObjectId id, const Matrix4& pose )
    {
        cnt->_data.worldPose[ cnt->objectIndex( id ) ] = pose;
    }

    void object_setAABB( bxVoxel_Container* cnt, bxVoxel_ObjectId id, const bxAABB& aabb )
    {
        cnt->_data.aabb[ cnt->objectIndex( id ) ] = aabb;
    }

    void gpu_uploadShell( bxGdiDeviceBackend* dev, bxVoxel_Container* cnt, bxVoxel_ObjectId id )
    {
        SYS_ASSERT( object_valid( cnt, id ) );
        const int dataIndex = cnt->objectIndex( id );
        _Gpu_uploadShell( dev, cnt, dataIndex );
    }

    void gfx_draw( bxGdiContext* ctx, bxVoxel_Container* cnt, const bxGfxCamera& camera )
    {
        //bxVoxel_Container* menago = &vctx->menago;
        const bxVoxel_Container::Data& data = cnt->_data;
        const int numObjects = data.size;

        if( !numObjects )
            return;

        const bxGdiBuffer* vxDataGpu = data.voxelDataGpu;
        const bxVoxel_ObjectData* vxDataObj = data.voxelDataObj;
        const Matrix4* worldMatrices = data.worldPose;

        bxGdiShaderFx_Instance* fxI = __gfx->fxI;
        bxGdiRenderSource* rsource = __gfx->rsource;

        fxI->setUniform( "_viewProj", camera.matrix.viewProj );
        
        bxGdi::shaderFx_enable( ctx, fxI, 0 );
        bxGdi::renderSource_enable( ctx, rsource );
        const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
        ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ), 0, bxGdi::eSTAGE_MASK_PIXEL );

        for( int iobj = 0; iobj < numObjects; ++iobj )
        {
            const bxVoxel_ObjectData& objData = vxDataObj[iobj];
            const int nInstancesToDraw = objData.numShellVoxels;
            if( nInstancesToDraw <= 0 )
                continue;

            fxI->setUniform( "_world", worldMatrices[iobj] );
            fxI->uploadCBuffers( ctx->backend() );

            ctx->setBufferRO( vxDataGpu[iobj], 0, bxGdi::eSTAGE_MASK_VERTEX );
            ctx->setTexture( __gfx->colorPalette_texture[objData.colorPalette], 0, bxGdi::eSTAGE_MASK_PIXEL );

            bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, nInstancesToDraw );
        }
    }





}
