#pragma once

class bxResourceManager;
struct bxVoxel_Map;

namespace bxVoxel
{
    int octree_loadMagicaVox( bxResourceManager* resourceManager, bxVoxel_Map* map, const char* filename );
    int octree_loadHeightmapRaw8( bxResourceManager* resourceManager, bxVoxel_Map* map, const char* filename );
}//