#include "hwstate_util.h"
#include <string.h>

using namespace bx;

namespace DepthFunc
{
	gdi::EDepthFunc fromString( const char* str )
    {
        gdi::EDepthFunc value = gdi::eDEPTH_FUNC_LEQUAL;

	    if	   ( !strcmp( str, "NEVER" 	  ) ) value = gdi::eDEPTH_FUNC_NEVER;
	    else if( !strcmp( str, "LESS" 	  ) ) value = gdi::eDEPTH_FUNC_LESS;
	    else if( !strcmp( str, "EQUAL" 	  ) ) value = gdi::eDEPTH_FUNC_EQUAL;
	    else if( !strcmp( str, "LEQUAL"   ) ) value = gdi::eDEPTH_FUNC_LEQUAL;
	    else if( !strcmp( str, "GREATER"  ) ) value = gdi::eDEPTH_FUNC_GREATER;
	    else if( !strcmp( str, "NOTEQUAL" ) ) value = gdi::eDEPTH_FUNC_NOTEQUAL;
	    else if( !strcmp( str, "GEQUAL"   ) ) value = gdi::eDEPTH_FUNC_GEQUAL;
	    else if( !strcmp( str, "ALWAYS"   ) ) value = gdi::eDEPTH_FUNC_ALWAYS;

	    return value;
    }
}//

namespace BlendFactor
{
	gdi::EBlendFactor fromString( const char* str )
    {
        gdi::EBlendFactor value = gdi::eBLEND_ZERO;
	
	    if	   ( !strcmp( str, "ZERO" 				 ) )	value = gdi::eBLEND_ZERO;
	    else if( !strcmp( str, "ONE" 				 ) )	value = gdi::eBLEND_ONE;
	    else if( !strcmp( str, "SRC_COLOR" 			 ) )	value = gdi::eBLEND_SRC_COLOR;
	    else if( !strcmp( str, "ONE_MINUS_SRC_COLOR" ) )	value = gdi::eBLEND_ONE_MINUS_SRC_COLOR;
	    else if( !strcmp( str, "DST_COLOR" 			 ) )	value = gdi::eBLEND_DST_COLOR;
	    else if( !strcmp( str, "ONE_MINUS_DST_COLOR" ) )	value = gdi::eBLEND_ONE_MINUS_DST_COLOR;
	    else if( !strcmp( str, "SRC_ALPHA" 			 ) )	value = gdi::eBLEND_SRC_ALPHA;
	    else if( !strcmp( str, "ONE_MINUS_SRC_ALPHA" ) )	value = gdi::eBLEND_ONE_MINUS_SRC_ALPHA;
	    else if( !strcmp( str, "DST_ALPHA" 			 ) )	value = gdi::eBLEND_DST_ALPHA;
	    else if( !strcmp( str, "ONE_MINUS_DST_ALPHA" ) )	value = gdi::eBLEND_ONE_MINUS_DST_ALPHA;
	    else if( !strcmp( str, "SRC_ALPHA_SATURATE"  ) )	value = gdi::eBLEND_SRC_ALPHA_SATURATE;
	    return value;
    }
}//

namespace BlendEquation
{
    gdi::EBlendEquation fromString( const char* str )
    {
        gdi::EBlendEquation value = gdi::eBLEND_ADD;
        if     ( !strcmp( str, "ADD"         ) ) value = gdi::eBLEND_ADD; 
        else if( !strcmp( str, "SUB"         ) ) value = gdi::eBLEND_SUB;
        else if( !strcmp( str, "REVERSE_SUB" ) ) value = gdi::eBLEND_REVERSE_SUB;
        else if( !strcmp( str, "MIN"         ) ) value = gdi::eBLEND_MIN;
        else if( !strcmp( str, "MAX"         ) ) value = gdi::eBLEND_MAX;

        return value;
    }
}//

namespace Culling
{
	gdi::ECulling fromString( const char* str )
    {
        gdi::ECulling value = gdi::eCULL_BACK;
	    if	   ( !strcmp( str, "NONE"  ) ) value = gdi::eCULL_NONE;
	    else if( !strcmp( str, "BACK"  ) ) value = gdi::eCULL_BACK;
	    else if( !strcmp( str, "FRONT" ) ) value = gdi::eCULL_FRONT;
	    return value;
    }
}//

namespace Fillmode
{
	gdi::EFillmode fromString( const char* str )
    {
        gdi::EFillmode value = gdi::eFILL_SOLID;
	    if( !strcmp( str, "SOLID" ) ) value = gdi::eFILL_SOLID;
	    if( !strcmp( str, "WIREFRAME" ) ) value = gdi::eFILL_WIREFRAME;
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

	    value |= (r) ? gdi::eCOLOR_MASK_RED : 0;
	    value |= (g) ? gdi::eCOLOR_MASK_GREEN : 0;
	    value |= (b) ? gdi::eCOLOR_MASK_BLUE : 0;
	    value |= (a) ? gdi::eCOLOR_MASK_ALPHA : 0;

	    return value;
    }
}//
