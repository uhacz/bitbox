#include "ascii_script.h"

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

    bxAsciiScript_Callback* scriptCallbackFind( bxAsciiScript* script, size_t cmdHash )
    {
        hashmap_t::cell_t* cell = hashmap::lookup( script->_map_callback, cmdHash );
        return (cell) ? (bxAsciiScript_Callback*)cell->value : 0;
    }

}///


int bxAsciiScript_AttribData::addNumberf( f32 n )
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
int bxAsciiScript_AttribData::addNumberi( i32 n )
{
    const int size = numBytes / sizeof( i32 );
    if ( size >= eMAX_NUMBER_LEN )
    {
        bxLogError( "to many attribute values" );
        return -1;
    }

    //inumber[size] = n;
    fnumber[size] = (f32)n;
    numBytes += sizeof( f32 );
    return 0;
}

int bxAsciiScript_AttribData::addNumberu( u32 n )
{
    const int size = numBytes / sizeof( u32 );
    if ( size >= eMAX_NUMBER_LEN )
    {
        bxLogError( "to many attribute values" );
        return -1;
    }

    //unumber[size] = n;
    fnumber[size] = (f32)n;
    numBytes += sizeof( f32 );
    return 0;
}
int bxAsciiScript_AttribData::setString( const char* str, int len )
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

    int attrib_parseLine( bxAsciiScript_AttribData* attribData, const char* line )
    {
        char attribDataToken[MAX_TOKEN_SIZE + 1] = { 0 };
        char* linePtr = (char*)line;
        int ierr = 0;
        while ( linePtr && !ierr )
        {
            linePtr = string::token( linePtr, attribDataToken, MAX_TOKEN_SIZE, " " );
            if ( attribDataToken[0] == '\"' )
            {
                char* attribString = attribDataToken + 1;
                int tokenLen = string::length( attribString ) - 1; // minus '"' char at the end
                ierr = attribData->setString( attribString, tokenLen );
            }
            else if ( isdigit( attribDataToken[0] ) || attribDataToken[0] == '-' || attribDataToken[0] == '.' )
            {
                if ( string::find( attribDataToken, "." ) )
                {
                    ierr = attribData->addNumberf( (f32)atof( attribDataToken ) );
                }
                else if ( string::find( attribDataToken, "x" ) )
                {
                    ierr = attribData->addNumberu( strtoul( attribDataToken, NULL, 0 ) );
                }
                else
                {
                    ierr = attribData->addNumberi( strtol( attribDataToken, NULL, 0 ) );
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
    int script_run( bxAsciiScript* script, const char* scriptTxt )
    {

        char line[MAX_LINE_SIZE + 1] = { 0 };
        char token[MAX_TOKEN_SIZE + 1] = { 0 };

        bxAsciiScript_AttribData attribData;
        memset( &attribData, 0x00, sizeof( bxAsciiScript_AttribData ) );

        bxAsciiScript_Callback* currentCallback = 0;
        char* scriptPtr = (char*)scriptTxt;

        char* lineDelimeter = "\n";
        {
            char* endOfLine = string::find( scriptPtr, lineDelimeter );
            if ( endOfLine && endOfLine[-1] == '\r' )
            {
                lineDelimeter = "\r\n";
            }
        }

        while ( 1 )
        {
            scriptPtr = string::token( scriptPtr, line, MAX_LINE_SIZE, lineDelimeter );
            if ( string::equal( line, "\r" ) )
                continue;

            if ( !scriptPtr && string::length( line ) == 0 )
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
                bxAsciiScript_Callback* callback = scriptCallbackFind( script, typeNameHash );
                if ( callback )
                {
                    callback->onCreate( typeName, objName );
                }

                currentCallback = callback;
            }
            else if ( token[0] == '$' && currentCallback )
            {
                memset( &attribData, 0x00, sizeof( bxAsciiScript_AttribData ) );
                char* attribName = token + 1;
                char attribDataToken[MAX_TOKEN_SIZE + 1] = { 0 };

                int ierr = attrib_parseLine( &attribData, linePtr );
                if ( ierr == 0 )
                {
                    currentCallback->onAttribute( attribName, attribData );
                }
            }
            else if ( token[0] == ':' )
            {
                memset( &attribData, 0x00, sizeof( bxAsciiScript_AttribData ) );
                char* cmdName = token + 1;
                char attribDataToken[MAX_TOKEN_SIZE + 1] = { 0 };
                int ierr = attrib_parseLine( &attribData, linePtr );
                if ( ierr == 0 )
                {
                    if ( currentCallback )
                    {
                        currentCallback->onCommand( cmdName, attribData );
                    }
                    else
                    {
                        size_t cmdNameHash = hashNameGet( cmdName );
                        bxAsciiScript_Callback* callback = scriptCallbackFind( script, cmdNameHash );
                        if ( callback )
                        {
                            callback->onCommand( cmdName, attribData );
                        }
                    }
                }
            }
        }


        return 0;
    }

    void script_addCallback( bxAsciiScript* script, const char* name, bxAsciiScript_Callback* callback )
    {
        const size_t nameHash = hashNameGet( name );
        SYS_ASSERT( hashmap::lookup( script->_map_callback, nameHash ) == 0 );

        hashmap_t::cell_t* cell = hashmap::insert( script->_map_callback, nameHash );
        cell->value = size_t( callback );
    }
}///
