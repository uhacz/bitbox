#pragma once

#include <util/memory.h>
#include <util/containers.h>
#include <gdi/gdi_backend.h>

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

	static bxVoxel_Gfx* instance();
};

namespace bxVoxel
{
	void gfx_startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
	void gfx_shutdown( bxGdiDeviceBackend* dev );

    bxVoxel_GfxDisplayList* gfx_displayListNew( int capacity, bxAllocator* alloc = bxDefaultAllocator() );
    void gfx_displayListDelete( bxVoxel_GfxDisplayList** dlist );

    void gfx_displayListBuild( bxVoxel_GfxDisplayList* dlist, bxVoxel_Container* menago, const bxGfxCamera& camera );
    void gfx_displayListDraw( bxGdiContext* ctx, bxVoxel_GfxDisplayList* dlist, bxVoxel_Container* menago, const bxGfxCamera& camera );
}///
