#pragma once

#include <gdi/gdi_backend.h>

namespace DepthFunc
{
	extern bxGdi::EDepthFunc fromString( const char* str );
}//

namespace BlendFactor
{
	extern bxGdi::EBlendFactor fromString( const char* str );
}//

namespace BlendEquation
{
    extern bxGdi::EBlendEquation fromString( const char* str );
}//

namespace Culling
{
	extern bxGdi::ECulling fromString( const char* str );
}//

namespace Fillmode
{
	extern bxGdi::EFillmode fromString( const char* str );
}// 

namespace ColorMask
{
	extern u8 fromString( const char* str );
}//
