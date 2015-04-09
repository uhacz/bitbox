#include "voxel_scene.h"
#include <gfx/gfx_camera.h>
#include <util/array.h>

#include "voxel.h"

struct bxScene_Level
{
    typedef array_t< bxVoxel_ObjectId > Container_VoxelObjectId;
    typedef array_t< bxGfxCamera >      Container_Camera;

    bxVoxel_Context* vxCtx;
    bxGfxCamera*     currentCamera;

    Container_Camera        camera;
    Container_VoxelObjectId vxObjects;

    bxScene_Level()
        : vxCtx(0)
        , currentCamera(0)
    {}
};

namespace bxVoxel
{

}///


namespace bxVoxel
{
    int script_run( bxDemoEngine* engine, bxVoxelScene_Script* script, const char* scriptTxt )
    {

    }

    void registerCallback( bxVoxelScene_Script* script, const char* name, bxScene_Callback* callback )
    {
    }
}///
