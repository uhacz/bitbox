#pragma once

class bxResourceManager;
struct bxGdiDeviceBackend;
struct bxVoxel_Map;

namespace bxVoxel
{
    int map_loadMagicaVox( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Map* map, const char* filename );
    int map_loadHeightmapRaw8( bxResourceManager* resourceManager, bxVoxel_Map* map, const char* filename );
}//