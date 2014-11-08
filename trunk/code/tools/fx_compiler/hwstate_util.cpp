#include "hwstate_util.h"
#include <string.h>

namespace DepthFunc
{
	bxGdi::EDepthFunc fromString( const char* str )
    {
        bxGdi::EDepthFunc value = bxGdi::eDEPTH_FUNC_LEQUAL;

	    if	   ( !strcmp( str, "NEVER" 	  ) ) value = bxGdi::eDEPTH_FUNC_NEVER;
	    else if( !strcmp( str, "LESS" 	  ) ) value = bxGdi::eDEPTH_FUNC_LESS;
	    else if( !strcmp( str, "EQUAL" 	  ) ) value = bxGdi::eDEPTH_FUNC_EQUAL;
	    else if( !strcmp( str, "LEQUAL"   ) ) value = bxGdi::eDEPTH_FUNC_LEQUAL;
	    else if( !strcmp( str, "GREATER"  ) ) value = bxGdi::eDEPTH_FUNC_GREATER;
	    else if( !strcmp( str, "NOTEQUAL" ) ) value = bxGdi::eDEPTH_FUNC_NOTEQUAL;
	    else if( !strcmp( str, "GEQUAL"   ) ) value = bxGdi::eDEPTH_FUNC_GEQUAL;
	    else if( !strcmp( str, "ALWAYS"   ) ) value = bxGdi::eDEPTH_FUNC_ALWAYS;

	    return value;
    }
}//

namespace BlendFactor
{
	bxGdi::EBlendFactor fromString( const char* str )
    {
        bxGdi::EBlendFactor value = bxGdi::eBLEND_ZERO;
	
	    if	   ( !strcmp( str, "ZERO" 				 ) )	value = bxGdi::eBLEND_ZERO;
	    else if( !strcmp( str, "ONE" 				 ) )	value = bxGdi::eBLEND_ONE;
	    else if( !strcmp( str, "SRC_COLOR" 			 ) )	value = bxGdi::eBLEND_SRC_COLOR;
	    else if( !strcmp( str, "ONE_MINUS_SRC_COLOR" ) )	value = bxGdi::eBLEND_ONE_MINUS_SRC_COLOR;
	    else if( !strcmp( str, "DST_COLOR" 			 ) )	value = bxGdi::eBLEND_DST_COLOR;
	    else if( !strcmp( str, "ONE_MINUS_DST_COLOR" ) )	value = bxGdi::eBLEND_ONE_MINUS_DST_COLOR;
	    else if( !strcmp( str, "SRC_ALPHA" 			 ) )	value = bxGdi::eBLEND_SRC_ALPHA;
	    else if( !strcmp( str, "ONE_MINUS_SRC_ALPHA" ) )	value = bxGdi::eBLEND_ONE_MINUS_SRC_ALPHA;
	    else if( !strcmp( str, "DST_ALPHA" 			 ) )	value = bxGdi::eBLEND_DST_ALPHA;
	    else if( !strcmp( str, "ONE_MINUS_DST_ALPHA" ) )	value = bxGdi::eBLEND_ONE_MINUS_DST_ALPHA;
	    else if( !strcmp( str, "SRC_ALPHA_SATURATE"  ) )	value = bxGdi::eBLEND_SRC_ALPHA_SATURATE;
	    return value;
    }
}//

namespace BlendEquation
{
    bxGdi::EBlendEquation fromString( const char* str )
    {
        bxGdi::EBlendEquation value = bxGdi::eBLEND_ADD;
        if     ( !strcmp( str, "ADD"         ) ) value = bxGdi::eBLEND_ADD; 
        else if( !strcmp( str, "SUB"         ) ) value = bxGdi::eBLEND_SUB;
        else if( !strcmp( str, "REVERSE_SUB" ) ) value = bxGdi::eBLEND_REVERSE_SUB;
        else if( !strcmp( str, "MIN"         ) ) value = bxGdi::eBLEND_MIN;
        else if( !strcmp( str, "MAX"         ) ) value = bxGdi::eBLEND_MAX;

        return value;
    }
}//

namespace Culling
{
	bxGdi::ECulling fromString( const char* str )
    {
        bxGdi::ECulling value = bxGdi::eCULL_BACK;
	    if	   ( !strcmp( str, "NONE"  ) ) value = bxGdi::eCULL_NONE;
	    else if( !strcmp( str, "BACK"  ) ) value = bxGdi::eCULL_BACK;
	    else if( !strcmp( str, "FRONT" ) ) value = bxGdi::eCULL_FRONT;
	    return value;
    }
}//

namespace Fillmode
{
	bxGdi::EFillmode fromString( const char* str )
    {
        bxGdi::EFillmode value = bxGdi::eFILL_SOLID;
	    if( !strcmp( str, "SOLID" ) ) value = bxGdi::eFILL_SOLID;
	    if( !strcmp( str, "WIREFRAME" ) ) value = bxGdi::eFILL_WIREFRAME;
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

	    value |= (r) ? bxGdi::eCOLOR_MASK_RED : 0;
	    value |= (g) ? bxGdi::eCOLOR_MASK_GREEN : 0;
	    value |= (b) ? bxGdi::eCOLOR_MASK_BLUE : 0;
	    value |= (a) ? bxGdi::eCOLOR_MASK_ALPHA : 0;

	    return value;
    }
}//
