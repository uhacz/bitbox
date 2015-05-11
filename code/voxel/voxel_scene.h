#pragma once

#include <util/ascii_script.h>
#include "voxel_type.h"

struct bxVoxel_SceneScriptCallback : public bxAsciiScript_Callback
{
    virtual void onCreate( const char* typeName, const char* objectName );
    virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData );
    virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args );

    bxVoxel_Container* _container;
    bxVoxel_ObjectId _currentId;
};

