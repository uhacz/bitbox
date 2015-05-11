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


int bxScene_ScriptAttribData::addNumberf( f32 n )
{
    const int size = numBytes / sizeof( f32 );
    if ( size >= eMAX_NUMBER_LEN )
    {
        bxLogError( "to many attribute values" );
        return -1;
    }

    fnumber[size] = n;
    numBytes += sizeof( f32 );
    return 0;
}
int bxScene_ScriptAttribData::addNumberi( i32 n )
{
    const int size = numBytes / sizeof( i32 );
    if ( size >= eMAX_NUMBER_LEN )
    {
        bxLogError( "to many attribute values" );
        return -1;
    }

    inumber[size] = n;
    numBytes += sizeof( i32 );
    return 0;
}

int bxScene_ScriptAttribData::addNumberu( u32 n )
{
    const int size = numBytes / sizeof( u32 );
    if ( size >= eMAX_NUMBER_LEN )
    {
        bxLogError( "to many attribute values" );
        return -1;
    }

    unumber[size] = n;
    numBytes += sizeof( u32 );
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
    static const int MAX_LINE_SIZE = 256;
    static const int MAX_TOKEN_SIZE = 64;

    int attrib_parseLine( bxScene_ScriptAttribData* attribData, const char* line )
    {
        char attribDataToken[MAX_TOKEN_SIZE + 1] = { 0 };
        char* linePtr = (char*)line;
        int ierr = 0;
        while( linePtr && !ierr )
        {
            linePtr = string::token( linePtr, attribDataToken, MAX_TOKEN_SIZE, " " );
            if ( attribDataToken[0] == '\"' )
            {
                char* attribString = attribDataToken + 1;
                int tokenLen = string::length( attribString ) - 1; // minus '"' char at the end
                ierr = attribData->setString( attribString, tokenLen );
            }
            else if ( isdigit( attribDataToken[0] ) || attribDataToken[0]=='-' || attribDataToken[0]=='.' )
            {
                if( string::find( attribDataToken, "." ) )
                {
                    ierr = attribData->addNumberf( (f32)atof( attribDataToken ) );
                }
                else if( string::find( attribDataToken, "x" ) )
                {
                    ierr = attribData->addNumberu( strtoul( attribDataToken, NULL, 0 ) );
                }
                else
                {
                    ierr = attribData->addNumberu( strtol( attribDataToken, NULL, 0 ) );
                }
            }
            else
            {
                bxLogError( "invalid character in script token '%s'", attribDataToken );
                ierr = -1;
            }
        } 
        return ierr;
    }
}///


namespace bxScene
{
    int script_run( bxScene_Script* script, const char* scriptTxt )
    {
        
        char line[MAX_LINE_SIZE + 1] = { 0 };
        char token[MAX_TOKEN_SIZE + 1 ] = { 0 };
        
        bxScene_ScriptAttribData attribData;
        memset( &attribData, 0x00, sizeof( bxScene_ScriptAttribData ) );

        bxScene_ScriptCallback* currentCallback = 0;
        char* scriptPtr = (char*)scriptTxt;

        char* lineDelimeter = "\n";
        {
            char* endOfLine = string::find( scriptPtr, lineDelimeter );
            if( endOfLine && endOfLine[-1] == '\r' )
            {
                lineDelimeter = "\r\n";
            }
        }

        while ( 1 )
        {
            scriptPtr = string::token( scriptPtr, line, MAX_LINE_SIZE, lineDelimeter );
            if ( string::equal( line, "\r" ) )
                continue;

            if( !scriptPtr && string::length( line ) == 0 )
            {
                break;
            }

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
                char* attribName = token + 1;
                char attribDataToken[MAX_TOKEN_SIZE + 1] = { 0 };

                int ierr = attrib_parseLine( &attribData, linePtr );
                if( ierr == 0 )
                {
                    currentCallback->onAttribute( attribName, attribData );
                }
            }
            else if ( token[0] == ':' )
            {
                memset( &attribData, 0x00, sizeof( bxScene_ScriptAttribData ) );
                char* cmdName = token + 1;
                char attribDataToken[MAX_TOKEN_SIZE + 1] = { 0 };
                int ierr = attrib_parseLine( &attribData, linePtr );
                if( ierr == 0 )
                {
                    if( currentCallback )
                    {
                        currentCallback->onCommand( cmdName, attribData );
                    }
                    else
                    {
                        size_t cmdNameHash = hashNameGet( cmdName );
                        bxScene_ScriptCallback* callback = scriptCallbackFind( script, cmdNameHash );
                        if( callback )
                        {
                            callback->onCommand( cmdName, attribData );
                        }
                    }
                }
            }
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

void bxVoxel_SceneScriptCallback::onCommand(const char* cmdName, const bxScene_ScriptAttribData& args )
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
    if ( string::equal( cmdName, "addSphere" ) )
    {
        if ( _container && bxVoxel::object_valid( _container, _currentId ) )
        {
            const int r = args.inumber[0];
            const int cindex = args.inumber[1];
            bxVoxel::util_addSphere( bxVoxel::object_map( _container, _currentId ), r, cindex );
        }
    }
}

////
////
void bxGfxCamera_SceneScriptCallback::onCreate(const char* typeName, const char* objectName)
{
    _currentId = bxGfx::camera_new( _menago, objectName );
}

void bxGfxCamera_SceneScriptCallback::onAttribute(const char* attrName, const bxScene_ScriptAttribData& attribData)
{
    bxGfx::camera_setAttrubute( _menago, _currentId, attrName, attribData.dataPointer(), attribData.dataSizeInBytes() );
}

void bxGfxCamera_SceneScriptCallback::onCommand(const char* cmdName, const bxScene_ScriptAttribData& args)
{
    if( string::equal( "camera_push", cmdName ) )
    {
        const bxGfxCamera_Id id = bxGfx::camera_find( _menago, args.string );
        bxGfx::camera_push( _menago, id );
    }
}