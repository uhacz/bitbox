#pragma once

class bxResourceManager;
struct bxVoxel_Octree;

namespace bxVoxel
{
    int octree_loadMagicaVox( bxResourceManager* resourceManager, bxVoxel_Octree* voct, const char* filename );
}//