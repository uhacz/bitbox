#pragma once

#include <stdint.h>
#include <float.h>

#ifdef _MSC_VER
#pragma warning( disable: 4512 )
#endif

#ifdef _MSC_VER
#pragma warning( disable: 4351 )
#endif

typedef int8_t i8;
typedef uint8_t u8;

typedef int16_t i16;
typedef uint16_t u16;

typedef int32_t i32;
typedef uint32_t u32;

typedef int64_t i64;
typedef uint64_t u64;

typedef uintptr_t uptr;
typedef intptr_t  iptr;

typedef volatile long		atomic32;
typedef volatile __int64	atomic64;

typedef float f32;
typedef double f64;

union TypeReinterpert
{
    u32 u;
    i32 i;
    f32 f;

    explicit TypeReinterpert( u32 v ) : u( v ) {}
    explicit TypeReinterpert( i32 v ) : i( v ) {}
    explicit TypeReinterpert( f32 v ) : f( v ) {}
};

union float2_t
{
    f32 xy[2];
    struct { f32 x, y; };

    float2_t() {}
    float2_t( f32 vx, f32 vy ) 
        : x(vx), y(vy) {}
};

union float3_t
{
    f32 xyz[3];
    struct { f32 x, y, z; };

    float3_t() {}
    float3_t( f32 vxyz ) : x( vxyz ), y( vxyz ), z( vxyz ) {}
    float3_t( f32 vx, f32 vy, f32 vz ) : x( vx ), y( vy ), z(vz) {}
};
inline float3_t operator * ( const float3_t& a, float b ) { return float3_t( a.x * b, a.y * b, a.y * b ); }

union float4_t
{
    f32 xyzw[4];
    struct { f32 x, y, z, w; };
    
    float4_t() {}
    float4_t( f32 vx, f32 vy, f32 vz, f32 vw ) : x( vx ), y( vy ), z( vz ), w( vw ) {}
    float4_t( const float3_t xyz, f32 vw ): x( xyz.x ), y( xyz.y ), z( xyz.z ), w( vw ) {}
};

union i32x2
{
    struct
    {
        i32 x, y;
    };
    i32 xyz[2];

    i32x2() : x( 0 ), y( 0 ) {}
    i32x2( i32 all ) : x( all ), y( all ) {}
    i32x2( i32 a, i32 b ) : x( a ), y( b ) {}
};
union i32x3
{
    struct
    {
        i32 x, y, z;
    };
    i32 xyz[3];

    i32x3() : x( 0 ), y( 0 ), z( 0 ) {}
    i32x3( i32 all ) : x( all ), y( all ), z( all ) {}
    i32x3( i32 a, i32 b, i32 c ) : x( a ), y( b ), z( c ) {}
};

#ifdef x86
typedef atomic32 atomic;
#elif x64
typedef atomic64 atomic;
#else
#error not implemented
#endif

#define BIT_OFFSET(n) (1<<n)
#define PTR_TO_U32( p )   ((u32)(uptr) (u32*) (p))
#define U32_TO_PTR( ui )  ((void *)(u32*)((u32)ui))

#define TYPE_OFFSET_GET_POINTER(type,offset) ( (offset)? (type*)((iptr)(&offset)+(iptr)(offset)) : (type*)0 )
#define TYPE_POINTER_GET_OFFSET(base, address) ( (base) ? (PTR_TO_U32(address) - PTR_TO_U32(base)) : 0 )

#ifdef _MSC_VER
#define BIT_ALIGNMENT( alignment )	__declspec(align(alignment))	
#else
#define BIT_ALIGNMENT( alignment ) __attribute__ ((aligned(alignment)))
#endif


#define BIT_ALIGNMENT_16 BIT_ALIGNMENT(16)
#define BIT_ALIGNMENT_64 BIT_ALIGNMENT(64)
#define BIT_ALIGNMENT_128 BIT_ALIGNMENT(128)

// "alignment" must be a power of 2
#define TYPE_IS_ALIGNED(value, alignment)                       \
	(                                                           \
		!(((iptr)(value)) & (((iptr)(alignment)) - 1U))         \
	) 


#define TYPE_ALIGN(value, alignment)                                \
	(                                                               \
		(((iptr) (value)) + (((iptr) (alignment)) - 1U))			\
		& ~(((iptr) (alignment)) - 1U)								\
	)



#ifndef alignof
#define ALIGNOF(x) __alignof(x)
#endif

#define MAKE_STR(x) MAKE_STR_(x)
#define MAKE_STR_(x) #x

template <typename T, size_t N>
size_t sizeof_array( const T( &)[N] )
{
    return N;
}

inline bool is_pow2( int n )
{
    return ( n&( n - 1 ) ) == 0;
}