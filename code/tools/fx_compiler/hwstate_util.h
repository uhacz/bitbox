#pragma once

//#include <gdi/gdi_backend.h>
#include <rdi/rdi_backend.h>

namespace DepthFunc
{
	extern bx::rdi::EDepthFunc::Enum fromString( const char* str );
}//

namespace BlendFactor
{
	extern bx::rdi::EBlendFactor::Enum fromString( const char* str );
}//

namespace BlendEquation
{
    extern bx::rdi::EBlendEquation::Enum fromString( const char* str );
}//

namespace Culling
{
	extern bx::rdi::ECullMode::Enum fromString( const char* str );
}//

namespace Fillmode
{
	extern bx::rdi::EFillMode::Enum fromString( const char* str );
}// 

namespace ColorMask
{
	extern u8 fromString( const char* str );
}//
