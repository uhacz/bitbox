#pragma once

#include <util/containers.h>

//struct bxDemoEngine;
//struct bxVoxel_Level;
////
//
//namespace bxVoxel
//{
//    bxVoxel_Level* level_new( const char* name );
//    void level_delete( bxVoxel_Level** level );
//
//    void level_loadResources  ( bxVoxel_Level* level, bxDemoEngine* engine );
//    void level_unloadResources( bxVoxel_Level* level, bxDemoEngine* engine );
//}///

////
//
/*
@ - create
; - attribute
: - command
*/
struct bxScene_ScriptCallback
{
    virtual void onCreate( const char* typeName, const char* objectName, void* userData ) = 0;
    virtual void onAttribute( const char* attrName, void* data, unsigned dataSize, void* userData ) = 0;
    virtual void onCommand( const char* cmdName, void* userData ) = 0;
};

struct bxScene_Script
{
    hashmap_t _map_callback;
};
namespace bxScene
{
    int  script_run( bxScene_Script* script, const char* scriptTxt, void* userData );
    void registerCallback( bxScene_Script* script, const char* name, bxScene_ScriptCallback* callback );
}///
