#pragma once

#include <util/memory.h>
#include <util/containers.h>
#include <gdi/gdi_backend.h>
#include "sort_list.h"

struct bxGdiContext;
struct bxGfxCamera;
struct bxVoxel_Container;
struct bxVoxel_GfxDisplayList;
struct bxGdiShaderFx_Instance;
struct bxGdiRenderSource;
class bxResourceManager;

struct bxVoxel_ColorPalette
{
	u32 rgba[256];
};
inline bool equal(const bxVoxel_ColorPalette& a, const bxVoxel_ColorPalette& b)
{
	return memcmp(a.rgba, b.rgba, sizeof(bxVoxel_ColorPalette)) == 0;
}
inline bool equal(const bxVoxel_ColorPalette& a, const u32* b)
{
	return memcmp(a.rgba, b, sizeof(bxVoxel_ColorPalette)) == 0;
}


////
////
struct bxVoxel_GfxSortItemColor
{
	union Key{
		u32 hash;
		struct{
			u32 depth : 16;
			u32 palette : 16;
		};
	} key;
	u16 itemIndex;
	u16 __padding[1];
};
inline bool operator < (const bxVoxel_GfxSortItemColor& a, const bxVoxel_GfxSortItemColor& b ){
    return a.key.hash < b.key.hash;
}

struct bxVoxel_GfxSortItemDepth
{
	union Key{
		u16 hash;
		u16 depth;
	} key;
	u16 itemIndex;
};
inline bool operator < (const bxVoxel_GfxSortItemDepth& a, const bxVoxel_GfxSortItemDepth& b){
    return a.key.hash < b.key.hash;
}

typedef bxSortList< bxVoxel_GfxSortItemColor > bxVoxel_GfxSortListColor;
typedef bxSortList< bxVoxel_GfxSortItemDepth > bxVoxel_GfxSortListDepth;

struct bxVoxel_GfxDisplayList;

struct bxVoxel_Gfx
{
	bxGdiShaderFx_Instance* fxI;
	bxGdiRenderSource* rsource;

	array_t<const char*> colorPalette_name;
	array_t<bxGdiTexture> colorPalette_texture;
	array_t<bxVoxel_ColorPalette> colorPalette_data;

	bxVoxel_GfxSortListColor* _slist_color;
	bxVoxel_GfxSortListDepth* _slist_depth;
	bxVoxel_GfxDisplayList* _dlist;

    static const int N_TASKS = 1;
    struct
    {
        bxChunk container[N_TASKS];
        bxChunk displayList[N_TASKS];
        bxChunk sortListColor[N_TASKS];
        bxChunk sortListDepth[N_TASKS];
        
        bxChunk finalSortedColor;
        bxChunk finalSortedDepth;
    } _chunk;
    
    

    bxVoxel_Gfx();

	static bxVoxel_Gfx* instance();
};

namespace bxVoxel
{
	void gfx_startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
	void gfx_shutdown( bxGdiDeviceBackend* dev );

    //bxVoxel_GfxDisplayList* gfx_displayListNew( int capacity, bxAllocator* alloc = bxDefaultAllocator() );
    //void gfx_displayListDelete( bxVoxel_GfxDisplayList** dlist );

    void gfx_displayListBuild( bxVoxel_Container* menago, const bxGfxCamera& camera );
    void gfx_displayListDraw( bxGdiContext* ctx, bxVoxel_Container* menago, const bxGfxCamera& camera );
}///
