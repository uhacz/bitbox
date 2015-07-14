#pragma once
#include "type.h"

#define BX_UTIL_TAG32( a,b,c,d ) u32( a << 0 | b << 8| c << 16 | d << 24 )

struct bxTag32
{
	bxTag32() : _tag(0) {}
	explicit bxTag32( const char tag[5] )	{ _tag = *((u32*)tag); }
		
	union
	{
		struct
		{
			u8 _byte1;
			u8 _byte2;
			u8 _byte3;
			u8 _byte4;
		};
		u32 _tag;
	};
	
	//operator u32		()		 { return _tag; }
	operator const u32  () const { return _tag; }
};

union Tag32ToString
{
    Tag32ToString( u32 tag )
    : as_u32( tag )
    {
        as_str[4] = 0;
    }
    u32 as_u32;
    char as_str[5];
};

union bxTag64
{
    bxTag64() : _lo(0), _hi(0) {}
    explicit bxTag64( const char tag[9] )	
        : _lo(0), _hi(0)
    {  
        _i8[0] = tag[0];
        _i8[1] = tag[1];
        _i8[2] = tag[2];
        _i8[3] = tag[3];
        _i8[4] = tag[4];
        _i8[5] = tag[5];
        _i8[6] = tag[6];
        _i8[7] = tag[7];
    }
    
    i8 _i8[8];
    u8 _8[8];
    u16 _16[4];
    u32 _32[2];
    u64 _64[1];
    struct
    {
        u32 _lo;
        u32 _hi;
    };

    operator       u64()       { return _64[0]; }
    operator const u64() const { return _64[0]; }
};

template< char c0, char c1 = 0, char c2 = 0, char c3 = 0, char c4 = 0, char c5 = 0, char c6 = 0, char c7 = 0 >
struct bxMakeTag64
{
    static const u64 tag = ( (u64)c0 << 0 ) | ( (u64)c1 << 8 ) | ( (u64)c2 << 16 ) | ( (u64)c3 << 24 ) | ( (u64)c4 << 32 ) | ( (u64)c5 << 40 ) | ( (u64)c6 << 48 ) | ( (u64)c7 << 56 );
};
inline const u64 to_Tag64( char c0, char c1 = 0, char c2 = 0, char c3 = 0, char c4 = 0, char c5 = 0, char c6 = 0, char c7  = 0 )
{
    return ((u64)c0 << 0) | ((u64)c1 << 8) | ((u64)c2 << 16) | ((u64)c3 << 24) | ((u64)c4 << 32) | ((u64)c5 << 40) | ((u64)c6 << 48) | ((u64)c7 << 56 );
}

inline u64 to_Tag64( const char str[] );

union bxTag128
{
	bxTag128() : _lo(0), _hi(0) {}
	explicit bxTag128( const char tag[17] )	
		 : _lo(0), _hi(0)
	{  
		u8* src = (u8*)tag;
		u8* dst = _8;
		while( *src )
		{
			*dst = *src;
			++src;
			++dst;
		}
	}
	
	i8 _i8[16];
	u8 _8[16];
	u16 _16[8];
	u32 _32[4];
	u64 _64[2];
	struct
	{
		u64 _lo;
		u64 _hi;
	};

	

};
inline bool operator == ( const bxTag128& a, const bxTag128& b )
{
	return a._lo == b._lo && a._hi == b._hi;
}
inline bool operator != ( const bxTag128& a, const bxTag128& b )
{
	return !( a == b );
}
