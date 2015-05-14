#include "voxel_scene.h"
#include <gfx/gfx_camera.h>
#include <util/string_util.h>
#include "voxel.h"

////
////
void bxVoxel_SceneScriptCallback::onCreate(const char* typeName, const char* objectName)
{
    _currentId = bxVoxel::object_new( _container, objectName );
}

void bxVoxel_SceneScriptCallback::onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData )
{
    if( bxVoxel::object_valid( _container, _currentId ) )
    {
        bxVoxel::object_setAttribute( _container, _currentId, attrName, attribData.dataPointer(), attribData.dataSizeInBytes() );
    }
}

void bxVoxel_SceneScriptCallback::onCommand( const char* cmdName, const bxAsciiScript_AttribData& args )
{
    if( string::equal( cmdName, "addPlane" ) )
    {
        if( _container && bxVoxel::object_valid( _container, _currentId ) )
        {
            const int w = args.inumber[0];
            const int h = args.inumber[1];
            const unsigned color = args.unumber[2];
            bxVoxel::util_addPlane( bxVoxel::object_map( _container, _currentId ), w, h, color );
        }
    }
    else if ( string::equal( cmdName, "addSphere" ) )
    {
        if ( _container && bxVoxel::object_valid( _container, _currentId ) )
        {
            const int r = args.inumber[0];
            const int cindex = args.inumber[1];
            bxVoxel::util_addSphere( bxVoxel::object_map( _container, _currentId ), r, cindex );
        }
    }
    else if ( string::equal( cmdName, "addBox" ) )
    {
        if ( _container && bxVoxel::object_valid( _container, _currentId ) )
        {
            const int w = args.inumber[0];
            const int h = args.inumber[1];
            const int d = args.inumber[2];
            const int cindex = args.inumber[3];
            bxVoxel::util_addBox( bxVoxel::object_map( _container, _currentId ), w, h, d, cindex );
        }
    }
}

