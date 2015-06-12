#pragma once

#include <stdio.h>

#define LOGGER_ENABLED 1
#define ASSERTION_ENABLED 1

#ifdef __cplusplus
extern "C" {
#endif
//////////////////////////////////////////////////////////////////////////
extern void bxDebugAssert( int expression, const char *format, ... );
extern void bxDebugHalt( char *str );
extern void checkFloat( float x ); 
#ifdef __cplusplus
}
#endif

#ifdef LOGGER_ENABLED

#define bxLogInfo( ... ) printf_s( __VA_ARGS__ ); printf_s( " : at %s:%d - ", __FILE__, __LINE__ ); printf( "\n" )
#define bxLogWarning( ... ) printf_s( "WARNING: " ); printf_s( __VA_ARGS__ ); printf_s( " : at %s:%d - ", __FILE__, __LINE__ ); printf( "\n" )
#define bxLogError( ... ) printf_s( "ERROR: " ); printf_s( __VA_ARGS__ ); printf_s( " : at %s:%d - ", __FILE__, __LINE__ ); printf( "\n" )

#else

#define bxLogInfo( ... )
#define bxLogWarning( ... )
#define bxLogError( ... )

#endif

#ifdef ASSERTION_ENABLED

#define AT __FILE__ ":" MAKE_STR(__LINE__)
#define SYS_ASSERT( expression ) bxDebugAssert( expression, "ASSERTION FAILED " AT " " #expression "\n" )
#define SYS_STATIC_ASSERT( expression ) typedef char(&foobar)[ (expression) ? 1 : 0 ]
#define SYS_NOT_IMPLEMENTED SYS_ASSERT( false && "not implemented" )

#else
SYS_ASSERT( expression )
SYS_STATIC_ASSERT( expression )
#endif



