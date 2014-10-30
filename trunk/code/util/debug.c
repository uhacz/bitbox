#include "debug.h"
#include <windows.h>
#include <crtdbg.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _MBCS
#pragma warning( disable: 4996 )
#endif

void bxDebugAssert( int expression, const char *format, ... )
{
	char str[1024];

	if( !expression )
	{
		va_list arglist;
		va_start( arglist, format );
		vsprintf( str, format, arglist );
		va_end( arglist );
		bxDebugHalt( str );
	}
}

void bxDebugHalt( char *str )
{
	MessageBox( 0, str, "error", MB_OK );
	//__asm { int 3 }
	__debugbreak();
}   
