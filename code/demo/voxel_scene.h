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
$ - attribute
: - command
*/
struct bxScene_ScriptAttribData
{
    enum
    {
        eMAX_STRING_LEN = 255,
        eMAX_NUMBER_LEN = 64,
    };
    union
    {
        f32 number[eMAX_NUMBER_LEN];
        char string[eMAX_STRING_LEN + 1];
    };
    u32 numBytes;

    int addNumber( f32 n );
    int setString( const char* str, int len );

    void* dataPointer() const { return (void*)&number[0]; }
    unsigned dataSizeInBytes() const { return numBytes;  }
};

struct bxScene_ScriptCallback
{
    virtual void onCreate( const char* typeName, const char* objectName ) = 0;
    virtual void onAttribute( const char* attrName, const bxScene_ScriptAttribData& attribData ) = 0;
    virtual void onCommand( const char* cmdName ) = 0;
};

struct bxScene_Script
{
    hashmap_t _map_callback;
};
namespace bxScene
{
    int  script_run( bxScene_Script* script, const char* scriptTxt );
    void script_addCallback( bxScene_Script* script, const char* name, bxScene_ScriptCallback* callback );
}///

#include "voxel_type.h"
struct bxVoxel_SceneScriptCallback : public bxScene_ScriptCallback
{
    virtual void onCreate( const char* typeName, const char* objectName );
    virtual void onAttribute( const char* attrName, const bxScene_ScriptAttribData& attribData );
    virtual void onCommand( const char* cmdName );

    bxVoxel_Container* _container;
    bxVoxel_ObjectId _currentId;
};