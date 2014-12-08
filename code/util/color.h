#pragma once

#include "type.h"
#include "common.h"

namespace bxColor
{

inline u32 toRGBA( u8 r, u8 g, u8 b, u8 a )
{
	return r << 24 | g << 16 | b << 8 | a;
}
		
inline u32 float4ToU32( const f32* f4 )
{
	u32 r = static_cast<u32>( f4[0] * 255.f );
	u32 g = static_cast<u32>( f4[1] * 255.f );
	u32 b = static_cast<u32>( f4[2] * 255.f );
	u32 a = static_cast<u32>( f4[3] * 255.f );
	r = clamp( r, 0U, 255U );
	g = clamp( g, 0U, 255U );
	b = clamp( b, 0U, 255U );
	a = clamp( a, 0U, 255U );
	return toRGBA( r, g, b, a );
}
		
inline void float4ToU32( const f32* f4, u32* ui32 )
{
	*ui32 = float4ToU32( f4 );
}

inline void u32ToFloat4( const u32 ui32, f32* f4 )
{
	const f32 r = (f32)((ui32 & 0xFF000000)	>> 24 ) / 255.f;
	const f32 g = (f32)((ui32 & 0x00FF0000)	>> 16 ) / 255.f;
	const f32 b = (f32)((ui32 & 0x0000FF00)	>> 8  ) / 255.f;
	const f32 a = (f32)((ui32 & 0x000000FF)	>> 0  ) / 255.f;
	f4[0] = static_cast<f32>( r );
	f4[1] = static_cast<f32>( g );
	f4[2] = static_cast<f32>( b );
	f4[3] = static_cast<f32>( a );
}

}///