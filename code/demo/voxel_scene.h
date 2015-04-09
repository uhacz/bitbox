#pragma once

#include <util/array.h>
#include <gfx/gfx_camera.h>
#include "voxel.h"

struct bxDemoEngine;

////
//
struct bxScene_Level
{
    typedef array_t< bxVoxel_ObjectId > Container_VoxelObjectId;
    typedef array_t< bxGfxCamera >      Container_Camera;

    bxVoxel_Context* vxCtx;
    bxGfxCamera*     currentCamera;

    Container_Camera        camera;
    Container_VoxelObjectId vxObjects;

    bxScene_Level();

    void load( bxDemoEngine* engine );
    void unload( bxDemoEngine* engine );
};

////
//
struct bxScene_Callback
{
    virtual void onCreate( const char* objectName, bxScene_Level* level ) = 0;
    virtual void onAttribute( const char* attrName, void* data, unsigned dataSize, bxScene_Level* level ) = 0;
    virtual void onCommand( const char* cmdName, bxScene_Level* level  ) = 0;
};

////
//
struct bxScene_Script
{
    void parse( const char* script, bxDemoEngine* engine, bxScene_Level* level );
    void registerCallback( const char* name, bxScene_Callback* callback );

private:
    hashmap_t _map_callback;
};