#pragma once

#include <gdi/gdi_backend.h>

namespace DepthFunc
{
	extern bx::gdi::EDepthFunc fromString( const char* str );
}//

namespace BlendFactor
{
	extern bx::gdi::EBlendFactor fromString( const char* str );
}//

namespace BlendEquation
{
    extern bx::gdi::EBlendEquation fromString( const char* str );
}//

namespace Culling
{
	extern bx::gdi::ECulling fromString( const char* str );
}//

namespace Fillmode
{
	extern bx::gdi::EFillmode fromString( const char* str );
}// 

namespace ColorMask
{
	extern u8 fromString( const char* str );
}//
