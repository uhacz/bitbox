#include "hwstate_util.h"
#include <string.h>

using namespace bx;

namespace DepthFunc
{
	rdi::EDepthFunc::Enum fromString( const char* str )
    {
        rdi::EDepthFunc::Enum value = rdi::EDepthFunc::LEQUAL;

	    if	   ( !strcmp( str, "NEVER" 	  ) ) value = rdi::EDepthFunc::NEVER;
	    else if( !strcmp( str, "LESS" 	  ) ) value = rdi::EDepthFunc::LESS;
	    else if( !strcmp( str, "EQUAL" 	  ) ) value = rdi::EDepthFunc::EQUAL;
	    else if( !strcmp( str, "LEQUAL"   ) ) value = rdi::EDepthFunc::LEQUAL;
	    else if( !strcmp( str, "GREATER"  ) ) value = rdi::EDepthFunc::GREATER;
	    else if( !strcmp( str, "NOTEQUAL" ) ) value = rdi::EDepthFunc::NOTEQUAL;
	    else if( !strcmp( str, "GEQUAL"   ) ) value = rdi::EDepthFunc::GEQUAL;
	    else if( !strcmp( str, "ALWAYS"   ) ) value = rdi::EDepthFunc::ALWAYS;

	    return value;
    }
}//

namespace BlendFactor
{
	rdi::EBlendFactor::Enum fromString( const char* str )
    {
        rdi::EBlendFactor::Enum value = rdi::EBlendFactor::ZERO;
	
	    if	   ( !strcmp( str, "ZERO" 				 ) )	value = rdi::EBlendFactor::ZERO;
	    else if( !strcmp( str, "ONE" 				 ) )	value = rdi::EBlendFactor::ONE;
	    else if( !strcmp( str, "SRC_COLOR" 			 ) )	value = rdi::EBlendFactor::SRC_COLOR;
	    else if( !strcmp( str, "ONE_MINUS_SRC_COLOR" ) )	value = rdi::EBlendFactor::ONE_MINUS_SRC_COLOR;
	    else if( !strcmp( str, "DST_COLOR" 			 ) )	value = rdi::EBlendFactor::DST_COLOR;
	    else if( !strcmp( str, "ONE_MINUS_DST_COLOR" ) )	value = rdi::EBlendFactor::ONE_MINUS_DST_COLOR;
	    else if( !strcmp( str, "SRC_ALPHA" 			 ) )	value = rdi::EBlendFactor::SRC_ALPHA;
	    else if( !strcmp( str, "ONE_MINUS_SRC_ALPHA" ) )	value = rdi::EBlendFactor::ONE_MINUS_SRC_ALPHA;
	    else if( !strcmp( str, "DST_ALPHA" 			 ) )	value = rdi::EBlendFactor::DST_ALPHA;
	    else if( !strcmp( str, "ONE_MINUS_DST_ALPHA" ) )	value = rdi::EBlendFactor::ONE_MINUS_DST_ALPHA;
	    else if( !strcmp( str, "SRC_ALPHA_SATURATE"  ) )	value = rdi::EBlendFactor::SRC_ALPHA_SATURATE;
	    return value;
    }
}//

namespace BlendEquation
{
    rdi::EBlendEquation::Enum fromString( const char* str )
    {
        rdi::EBlendEquation::Enum value = rdi::EBlendEquation::ADD;
        if     ( !strcmp( str, "ADD"         ) ) value = rdi::EBlendEquation::ADD; 
        else if( !strcmp( str, "SUB"         ) ) value = rdi::EBlendEquation::SUB;
        else if( !strcmp( str, "REVERSE_SUB" ) ) value = rdi::EBlendEquation::REVERSE_SUB;
        else if( !strcmp( str, "MIN"         ) ) value = rdi::EBlendEquation::MIN;
        else if( !strcmp( str, "MAX"         ) ) value = rdi::EBlendEquation::MAX;

        return value;
    }
}//

namespace Culling
{
	rdi::ECullMode::Enum fromString( const char* str )
    {
        rdi::ECullMode::Enum value = rdi::ECullMode::BACK;
	    if	   ( !strcmp( str, "NONE"  ) ) value = rdi::ECullMode::NONE;
	    else if( !strcmp( str, "BACK"  ) ) value = rdi::ECullMode::BACK;
	    else if( !strcmp( str, "FRONT" ) ) value = rdi::ECullMode::FRONT;
	    return value;
    }
}//

namespace Fillmode
{
	rdi::EFillMode::Enum fromString( const char* str )
    {
        rdi::EFillMode::Enum value = rdi::EFillMode::SOLID;
	    if( !strcmp( str, "SOLID"     ) ) value = rdi::EFillMode::SOLID;
	    if( !strcmp( str, "WIREFRAME" ) ) value = rdi::EFillMode::WIREFRAME;
	    return value;
    }
}// 

namespace ColorMask
{
	u8 fromString( const char* str )
    {
        u8 value = 0;
	
        const char* r = strchr( str, 'R' );
        const char* g = strchr( str, 'G' );
        const char* b = strchr( str, 'B' );
        const char* a = strchr( str, 'A' );
        
        value |= ( r ) ? rdi::EColorMask::RED : 0;
        value |= ( g ) ? rdi::EColorMask::GREEN : 0;
        value |= ( b ) ? rdi::EColorMask::BLUE : 0;
        value |= ( a ) ? rdi::EColorMask::ALPHA : 0;

	    return value;
    }
}//
