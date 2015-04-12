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

#include <sstream>
#include <util/debug.h>
#include <util/hash.h>
#include <util/hashmap.h>
#include <util/string_util.h>
namespace
{
    inline size_t hashNameGet( const char* name )
    {
        const u32 seed = 0xDE817F0D;
        const u32 hash = murmur3_hash32( name, string::length( name ), seed );
        return (size_t)seed << 32 | (size_t)hash;
    }

    bxScene_ScriptCallback* scriptCallbackFind( bxScene_Script* script, size_t cmdHash )
    {
        hashmap_t::cell_t* cell = hashmap::lookup( script->_map_callback, cmdHash );
        return (cell) ? (bxScene_ScriptCallback*)cell->value : 0;
    }
    
}///

int bxScene_ScriptAttribData::addNumber( f64 n )
{
    const int size = numBytes / sizeof( f64 );
    if ( size >= eMAX_NUMBER_LEN )
    {
        bxLogError( "to many attribute values" );
        return -1;
    }

    number[size] = n;
    numBytes += sizeof( f64 );
    return 0;
}

int bxScene_ScriptAttribData::setString( const char* str, int len )
{
    if ( len >= eMAX_STRING_LEN )
    {
        bxLogError( "attribute string to long '%s'", str );
        return -1;
    }
    memcpy( string, str, len );
    string[len] = 0;
    numBytes = len;
    return 0;
}

namespace bxScene
{
    int script_run( bxScene_Script* script, const char* scriptTxt )
    {
        const int MAX_LINE_SIZE = 256;
        const int MAX_TOKEN_SIZE = 64;
        char line[MAX_LINE_SIZE + 1] = { 0 };
        char token[MAX_TOKEN_SIZE + 1 ] = { 0 };
        
        bxScene_ScriptAttribData attribData;
        memset( &attribData, 0x00, sizeof( bxScene_ScriptAttribData ) );

        bxScene_ScriptCallback* currentCallback = 0;
        char* scriptPtr = string::token( (char*)scriptTxt, line, MAX_LINE_SIZE, "\n" );
        while ( scriptPtr )
        {
            char* linePtr = string::token( line, token, MAX_TOKEN_SIZE, " " );
            if ( token[0] == '@' )
            {
                char objName[MAX_TOKEN_SIZE + 1] = { 0 };
                linePtr = string::token( linePtr, objName, MAX_TOKEN_SIZE, " " );

                char* typeName = token + 1;
                size_t typeNameHash = hashNameGet( typeName );
                bxScene_ScriptCallback* callback = scriptCallbackFind( script, typeNameHash );
                if( callback )
                {
                    callback->onCreate( typeName, objName );
                }

                currentCallback = callback;
            }
            else if ( token[0] == '$' && currentCallback )
            {
                memset( &attribData, 0x00, sizeof( bxScene_ScriptAttribData ) );
                char attribName[MAX_TOKEN_SIZE + 1] = { 0 };
                char attribDataToken[MAX_TOKEN_SIZE + 1] = { 0 };

                int ierr = 0;
                linePtr = string::token( line, attribName, MAX_TOKEN_SIZE, " " );
                while( linePtr && !ierr )
                {
                    linePtr = string::token( linePtr, attribDataToken, MAX_TOKEN_SIZE, " " );
                    if ( attribDataToken[0] == '\"' )
                    {
                        char* attribString = attribDataToken + 1;
                        const int tokenLen = string::length( attribString ) - 1; // minus '"' char at the end
                        ierr = attribData.setString( attribString, tokenLen );
                    }
                    else if ( isdigit( attribDataToken[0] ) )
                    {
                        ierr = attribData.addNumber( atof( attribDataToken ) );
                    }
                    else
                    {
                        bxLogError( "invalid character in script token '%s'", attribDataToken );
                        ierr = -1;
                    }
                }

                if( ierr == 0 )
                {
                    currentCallback->onAttribute( attribName, attribData );
                }
            }
            else if ( token[0] == ':' )
            {
                if( currentCallback )
                {
                    
                }
                else
                {
                    
                }
            }
            
            //SYS_ASSERT( linePtr == 0 );

            scriptPtr = string::token( scriptPtr, line, MAX_LINE_SIZE, "\n" );
        }
               
       
        return 0;
    }

    void script_addCallback( bxScene_Script* script, const char* name, bxScene_ScriptCallback* callback )
    {
        const size_t nameHash = hashNameGet( name );
        SYS_ASSERT( hashmap::lookup( script->_map_callback, nameHash ) == 0 );

        hashmap_t::cell_t* cell = hashmap::insert( script->_map_callback, nameHash );
        cell->value = size_t( callback );
    }
}///

#include "voxel.h"
////
////
void bxVoxel_SceneScriptCallback::onCreate(const char* typeName, const char* objectName)
{
    _currentId = bxVoxel::object_new( _container );
}

void bxVoxel_SceneScriptCallback::onAttribute(const char* attrName, const bxScene_ScriptAttribData& attribData)
{
    if( bxVoxel::object_valid( _container, _currentId ) )
    {
        bxVoxel::object_setAttribute( _container, _currentId, attrName, attribData.dataPointer(), attribData.dataSizeInBytes() );
    }
}

void bxVoxel_SceneScriptCallback::onCommand(const char* cmdName)
{
}

