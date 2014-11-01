#include "string_util.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "memory.h"
#include "debug.h"

char* string::token( char* str, char* tok, size_t toklen, char* delim )
{
    if( !str )
        return 0;
    str += strspn( str, delim );
    size_t n = strcspn( str, delim );
    n = ( toklen - 1 < n ) ? toklen - 1 : n;
    memcpy( tok, str, n );
    tok[n] = 0;
    str += n;
    return ( *str == 0 ) ? 0 : str;
}

char* string::duplicate( char* old_string, const char* new_string )
{
    const size_t new_len = strlen( new_string );
    if( old_string )
    {
        const size_t old_len = strlen( old_string );
        if( old_len != new_len )
        {
            BX_FREE0( bxDefaultAllocator(), old_string );
            old_string = ( char* )BX_MALLOC( bxDefaultAllocator(), new_len + 1, 1 );
        }
    }
    else
    {
        old_string = (char*)BX_MALLOC( bxDefaultAllocator(), new_len + 1, 1 );
    }

    strcpy( old_string, new_string );
    return old_string;
}

void string::free( char* str )
{
    BX_FREE( bxDefaultAllocator(), str );
}

char* string::create( const char* str, size_t len )
{
    SYS_ASSERT( strlen( str ) >= len );
    char* out = (char*)BX_MALLOC( bxDefaultAllocator(), (u32)len + 1, 1 ); //memory_alloc( len + 1 );
    out[len] = 0;
    strncpy( out, str, len );

    return out;
}

unsigned string::count( const char* str, const char c )
{
    unsigned count = 0;
    while( *str )
    {
        const char strc = *str;
        count += strc == c;

        ++str;
    }

    return count;
}
unsigned string::length( const char* str )
{
    return (unsigned)strlen( str );
}
bool string::equal( const char* str0, const char* str1 )
{
    return strcmp( str0, str1 ) == 0;
}

int string::append( char* buffer, int size_buffer, const char* format, ... )
{
    char str_format[1024] = {0};
    
    if( format )
    {
        va_list arglist;
        va_start( arglist, format );
        vsprintf( str_format, format, arglist );
        va_end( arglist );
    }

    const int len_buff = (int)strlen( buffer );

    if( strlen( str_format ) == 0 )
    {
        return len_buff;
    }

    int len_left = size_buffer - len_buff;
    if( len_left > 0 )
    {
        sprintf_s( buffer + len_buff, len_left, str_format );
        len_left = size_buffer - (int)strlen( buffer );
    }
    return len_left;
}

char* string::find( const char* str, const char* to_find, char** next /*= 0 */ )
{
    char* result = (char*)strstr( str, to_find );
    if( result && next )
    {
        next[0] = (char*)( str + strlen( to_find ) );
    }

    return result;
}
