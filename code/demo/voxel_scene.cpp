#include "voxel_scene.h"
#include <gfx/gfx_camera.h>
#include <util/array.h>

#include "voxel.h"

//struct bxVoxel_Level
//{
//    typedef array_t< bxVoxel_ObjectId > Container_VoxelObjectId;
//    typedef array_t< bxGfxCamera >      Container_Camera;
//
//    bxVoxel_Container* container;
//    bxGfxCamera*       currentCamera;
//
//    Container_Camera        camera;
//    Container_VoxelObjectId objects;
//
//    bxVoxel_Level()
//        : container(0)
//        , currentCamera(0)
//    {}
//};
//
//namespace bxVoxel
//{
//
//}///

#include <sstream>

namespace
{
    
}///

namespace bxVoxel
{
    int script_run( bxDemoEngine* engine, bxScene_Script* script, const char* scriptTxt, void* userData )
    {
        std::stringstream s( scriptTxt );
        std::string token;
        while( s )
        {
            s >> token;

            if( token[0] == '@' )
            {
                std::string typeName( token.begin() + 1, token.end() );
                std::string objName;
                s >> objName;



            }
            else if( token[0] == ';' )
            {

            }
            else if( token[0] == ':' )
            {
                
            }

        }
        return 0;
    }

    void registerCallback( bxScene_Script* script, const char* name, bxScene_Callback* callback )
    {
    }
}///
