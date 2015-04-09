#pragma once

#include <util/containers.h>

struct bxDemoEngine;
struct bxScene_Level;
////
//
namespace bxVoxel
{
    bxScene_Level* level_new( const char* name );
    void level_delete( bxScene_Level** level );

    void level_loadResources  ( bxScene_Level* level, bxDemoEngine* engine );
    void level_unloadResources( bxScene_Level* level, bxDemoEngine* engine );
}///

////
//
struct bxScene_Callback
{
    virtual void onCreate( const char* objectName, bxScene_Level* level ) = 0;
    virtual void onAttribute( const char* attrName, void* data, unsigned dataSize, bxScene_Level* level ) = 0;
    virtual void onCommand( const char* cmdName, bxScene_Level* level ) = 0;
};

struct bxVoxelScene_Script
{
    hashmap_t _map_callback;
};
namespace bxVoxel
{
    int  script_run( bxDemoEngine* engine, bxVoxelScene_Script* script, const char* scriptTxt );
    void registerCallback( bxVoxelScene_Script* script, const char* name, bxScene_Callback* callback );
}///
