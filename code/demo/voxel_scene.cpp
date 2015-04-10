#include "voxel_scene.h"
#include <gfx/gfx_camera.h>
#include <util/array.h>

#include "voxel.h"
#include <tools/lua/lua.hpp>
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

//#include <sstream>
//#include <util/hash.h>
//#include <util/hashmap.h>
//#include <util/string_util.h>
//namespace
//{
//    inline size_t hashNameGet( const char* name )
//    {
//        const u32 seed = 0xDE817F0D;
//        const u32 hash = murmur3_hash32( name, string::length( name ), seed );
//        return (size_t)seed << 32 | (size_t)hash;
//    }
//
//    bxScene_ScriptCallback* scriptCallbackFind( bxScene_Script* script, size_t cmdHash )
//    {
//        hashmap_t::cell_t* cell = hashmap::lookup( script->_map_callback, cmdHash );
//        return (cell) ? (bxScene_ScriptCallback*)cell->value : 0;
//    }
//
//}///
//
//namespace bxVoxel
//{
//    int script_run( bxScene_Script* script, const char* scriptTxt, void* userData )
//    {
//        std::stringstream s( scriptTxt );
//        std::string token;
//        while( s )
//        {
//            s >> token;
//            std::string cmd( token.begin() + 1, token.end() );
//            const size_t cmdHash = hashNameGet( cmd.c_str() );
//            
//            if( token[0] == '@' )
//            {
//                std::string objName;
//                s >> objName;
//            
//                
//
//            }
//            else if( token[0] == ';' )
//            {
//
//            }
//            else if( token[0] == ':' )
//            {
//                
//            }
//
//        }
//        return 0;
//    }
//
//    void registerCallback( bxScene_Script* script, const char* name, bxScene_ScriptCallback* callback )
//    {
//    }
//}///
