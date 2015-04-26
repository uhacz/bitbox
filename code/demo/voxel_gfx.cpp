#include "voxel_gfx.h"
#include "voxel_type.h"
#include <util/array.h>
#include <util/string_util.h>
#include <util/buffer_utils.h>
#include <util/range_splitter.h>
#include <util/common.h>
#include <util/memory.h>

#include <resource_manager/resource_manager.h>
#include <gdi/gdi_shader.h>
#include <gdi/gdi_render_source.h>

#include "voxel_container.h"

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

static bxVoxel_Gfx* __gfx = 0;
bxVoxel_Gfx* bxVoxel_Gfx::instance() { return __gfx; }

struct BIT_ALIGNMENT_16 bxVoxel_GfxDisplayList
{
	Matrix4* matrices;
	u16* objectDataIndex;
	
	i32 capacity;
	bxAllocator* allocator;
};

struct bxVoxel_GfxDisplayListChunk
{
	bxVoxel_GfxDisplayList* dlist;
	i32 begin;
	i32 end;
	i32 current;
};

namespace bxVoxel
{
    template< typename Titem >
    void gfx_sortListCreateChunks( typename bxSortList<Titem>::Chunk* chunks, int nChunks, bxSortList<Titem>* slist, int nItems )
    {
        const int N_TASKS = nChunks;
        bxRangeSplitter splitter = bxRangeSplitter::splitByTask( nItems, N_TASKS );
        int ilist = 0;
        while ( splitter.elementsLeft() )
        {
            const int begin = splitter.grabbedElements;
            const int grab = splitter.nextGrab();
            const int end = begin + grab;

            SYS_ASSERT( ilist < N_TASKS );
            bxSortList<Titem>::Chunk& c = chunks[ilist++];
            c.slist = slist;
            c.begin = begin;
            c.end = end;
        }

        SYS_ASSERT( ilist == N_TASKS );
    }

	bxVoxel_GfxDisplayList* gfx_displayListNew(int capacity, bxAllocator* alloc )
	{
		int memSize = 0;
		memSize += sizeof( bxVoxel_GfxDisplayList );
		memSize += capacity * sizeof( Matrix4 );
		memSize += capacity * sizeof( u16 );

		void* memHandle = BX_MALLOC( alloc, memSize, 16 );
		memset( memHandle, 0x00, memSize );
		bxBufferChunker chunker( memHandle, memSize );

		bxVoxel_GfxDisplayList* dlist = chunker.add< bxVoxel_GfxDisplayList >();
		dlist->matrices = chunker.add< Matrix4 >( capacity, 16 );
		dlist->objectDataIndex = chunker.add< u16 >( capacity );
		
		chunker.check();

		dlist->capacity = capacity;
		dlist->allocator = alloc;

		return dlist;

	}
	void gfx_displayListDelete( bxVoxel_GfxDisplayList** dlist )
	{
		bxAllocator* alloc = dlist[0]->allocator;
		BX_FREE0( alloc, dlist[0] );
	}

    void gfx_displayListCreateChunks( bxVoxel_GfxDisplayListChunk* chunks, int nChunks, bxVoxel_GfxDisplayList* dlist )
	{
        const int capacity = dlist->capacity;

        bxRangeSplitter split = bxRangeSplitter::splitByTask( capacity, nChunks );

        int ichunk = 0;
        while( split.elementsLeft() )
        {
            const int begin = split.grabbedElements;
            const int grab = split.nextGrab();
            const int end = begin + grab;

            SYS_ASSERT( ichunk < nChunks );

            bxVoxel_GfxDisplayListChunk& c = chunks[ichunk];
            c.dlist = dlist;
            c.begin = begin;
            c.end = end;
            c.current = begin;
            
            ++ichunk;
        }

        SYS_ASSERT( ichunk == nChunks );
	}

    int gfx_displayListChunkAdd( bxVoxel_GfxDisplayListChunk* chunk, u16 objectIndex, const Matrix4* matrices, int nMatrices )
    {
        const int spaceLeft = chunk->end - chunk->current;
        nMatrices = minOfPair( spaceLeft, nMatrices );

        bxVoxel_GfxDisplayList* dlist = chunk->dlist;
        for ( int i = 0; i < nMatrices; ++i )
        {
            dlist->objectDataIndex[chunk->current] = objectIndex;
            dlist->matrices[chunk->current] = matrices[i];

            ++chunk->current;
        }

        SYS_ASSERT( chunk->current <= chunk->end );
        return nMatrices;
    }

    int gfx_displayListChunkAdd( bxVoxel_GfxDisplayListChunk* chunk, bxVoxel_Container* menago, bxVoxel_ObjectId id, const Matrix4* matrices, int nMatrices )
	{
        const u16 objIndex = menago->objectIndex( id );
        return gfx_displayListChunkAdd( chunk, objIndex, matrices, nMatrices );
	}

    int gfx_displayListChunkFill( bxVoxel_GfxDisplayListChunk* chunk, bxVoxel_Container* menago )
	{
        const bxVoxel_Container::Data& data = menago->_data;
        int nObjects = data.size;

        int iobj = 0;
        for( ; iobj < nObjects; ++iobj )
        {
            const u16 objectIndex = (u16)iobj;
            const Matrix4& matrix = data.worldPose[objectIndex];

            int ires = gfx_displayListChunkAdd( chunk, objectIndex, &matrix, 1 );
            if ( !ires )
                break;
        }
        return iobj;
	}

    void gfx_displayListBuild( bxVoxel_Container* menago, const bxGfxCamera& camera )
	{
		bxVoxel_GfxDisplayList* dlist = __gfx->_dlist;
		
		const int N_TASKS = 1;
		bxVoxel_GfxDisplayListChunk chunks[N_TASKS];
        memset( chunks, 0x00, sizeof( chunks ) );
		const int nChunks = N_TASKS;

        gfx_displayListCreateChunks( chunks, nChunks, dlist );
        gfx_displayListChunkFill( &chunks[0], menago );

		int nItems = 0;
		for( int ichunk = 0; ichunk < nChunks; ++ichunk )
		{
			nItems += chunks[ichunk].current - chunks[ichunk].begin;
		}

		bxVoxel_GfxSortListColor::Chunk slistColorChunks[N_TASKS];
        gfx_sortListCreateChunks( slistColorChunks, N_TASKS, __gfx->_slist_color, nItems );



	}
    void gfx_displayListDraw( bxGdiContext* ctx, bxVoxel_Container* menago, const bxGfxCamera& camera )
	{
	    
	}

}


namespace
{

	int _Gfx_findColorPaletteByData( bxVoxel_Gfx* gfx, const u32* data )
	{
		for( int i = 0; i < array::size( gfx->colorPalette_data ); ++i )
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
		memcpy( palette.rgba, data, minOfPair( dataSize, ( int )sizeof( bxVoxel_ColorPalette ) ) );

		gfx->colorPalette_texture[idx] = dev->createTexture1D( 256, 1, bxGdiFormat( bxGdi::eTYPE_UBYTE, 4, 1 ), bxGdi::eBIND_SHADER_RESOURCE, 0, data );
		return idx;
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

		const int MAX_DISPLAY_ITEMS = 1024 * 8;
		bx::sortListNew( &gfx->_slist_color, MAX_DISPLAY_ITEMS, bxDefaultAllocator() );
		bx::sortListNew( &gfx->_slist_depth, MAX_DISPLAY_ITEMS, bxDefaultAllocator() );
		gfx->_dlist = bxVoxel::gfx_displayListNew( MAX_DISPLAY_ITEMS, bxDefaultAllocator() );


	}
	void _Gfx_shutdown( bxGdiDeviceBackend* dev, bxVoxel_Gfx* gfx )
	{
		bxVoxel::gfx_displayListDelete( &gfx->_dlist );
		bx::sortListDelete( &gfx->_slist_depth );
		bx::sortListDelete( &gfx->_slist_color );


		for( int i = 0; i < array::size( gfx->colorPalette_name ); ++i )
		{
			string::free_and_null( (char**)&gfx->colorPalette_name[i] );
			dev->releaseTexture( &gfx->colorPalette_texture[i] );
		}
		array::clear( gfx->colorPalette_texture );
		array::clear( gfx->colorPalette_data );
		array::clear( gfx->colorPalette_name );

		bxGdi::shaderFx_releaseWithInstance( dev, &gfx->fxI );
		bxGdi::renderSource_releaseAndFree( dev, &gfx->rsource );
	}
}

namespace bxVoxel
{
	void gfx_startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
	{
		SYS_ASSERT( __gfx == 0 );
		__gfx = BX_NEW( bxDefaultAllocator(), bxVoxel_Gfx );
		_Gfx_startup( dev, resourceManager, __gfx );
	}
	void gfx_shutdown( bxGdiDeviceBackend* dev )
	{
		_Gfx_shutdown( dev, __gfx );
		BX_DELETE0( bxDefaultAllocator(), __gfx );
	}
}

