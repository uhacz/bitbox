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

#define bxLogInfo( ... ) printf_s( __VA_ARGS__ ); printf_s( " : at %s:%d - \n", __FILE__, __LINE__ )
#define bxLogWarning( ... ) printf_s( "WARNING: " ); printf_s( __VA_ARGS__ ); printf_s( " : at %s:%d - \n", __FILE__, __LINE__ )
#define bxLogError( ... ) printf_s( "ERROR: " ); printf_s( __VA_ARGS__ ); printf_s( " : at %s:%d - \n", __FILE__, __LINE__ )

#else

#define bxLogInfo( ... )
#define bxLogWarning( ... )
#define bxLogError( ... )

#endif

#ifdef ASSERTION_ENABLED

#define AT __FILE__ ":" MAKE_STR(__LINE__)
#define SYS_ASSERT( expression ) bxDebugAssert( expression, "ASSERTION FAILED " AT " " #expression "\n" )
#define SYS_ASSERT_TXT( expression, txt, ... ) bxDebugAssert( expression, "ASSERTION FAILED " AT " " #expression "\n" #txt, __VA_ARGS__ )

#define SYS_STATIC_ASSERT( expression ) static_assert( expression, "" )
#define SYS_NOT_IMPLEMENTED SYS_ASSERT( false && "not implemented" )

#else
#define SYS_ASSERT( expression )
#define SYS_ASSERT_TXT( expression, txt )
#define SYS_STATIC_ASSERT( expression )
#endif



