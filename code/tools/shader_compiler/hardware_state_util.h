#pragma once

#include <rdi/rdi_backend.h>

namespace bx{ namespace tool{

namespace DepthFunc
{
	extern rdi::EDepthFunc::Enum FromString( const char* str );
}//

namespace BlendFactor
{
	extern rdi::EBlendFactor::Enum FromString( const char* str );
}//

namespace BlendEquation
{
    extern rdi::EBlendEquation::Enum FromString( const char* str );
}//

namespace Culling
{
	extern rdi::ECullMode::Enum FromString( const char* str );
}//

namespace Fillmode
{
	extern rdi::EFillMode::Enum FromString( const char* str );
}// 

namespace ColorMask
{
	extern u8 FromString( const char* str );
}//

}}///