#include "debug.h"
#include <windows.h>
#include <crtdbg.h>
#include <stdarg.h>
#include <stdio.h>
#include <float.h>

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

void checkFloat( float x )
{
#ifdef _MSC_VER
    const double d = (double)x;
    const int cls = _fpclass( d );
    if( cls == _FPCLASS_SNAN ||
        cls == _FPCLASS_QNAN ||
        cls == _FPCLASS_NINF ||
        cls == _FPCLASS_PINF )
    {
        bxDebugAssert( 0, "invalid float" );
    }
#else
    (void)x;
#endif
}
