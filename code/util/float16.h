#pragma once
// float->half variants.
// by Fabian "ryg" Giesen.
//
// I hereby place this code in the public domain.
//
// float_to_half_full: This is basically the ISPC stdlib code, except
// I preserve the sign of NaNs (any good reason not to?)
//
// float_to_half_fast: Almost the same, with some unnecessary cases cut.
//
// float_to_half_fast2: This is where it gets a bit weird. See lengthy
// commentary inside the function code. I'm a bit on the fence about two
// things:
// 1. This *will* behave differently based on whether flush-to-zero is
//    enabled or not. Is this acceptable for ISPC?
// 2. I'm a bit on the fence about NaNs. For half->float, I opted to extend
//    the mantissa (preserving both qNaN and sNaN contents) instead of always
//    returning a qNaN like the original ISPC stdlib code did. For float->half
//    the "natural" thing would be just taking the top mantissa bits, except
//    that doesn't work; if they're all zero, we might turn a sNaN into an
//    Infinity (seriously bad!). I could test for this case and do a sticky
//    bit-like mechanism, but that's pretty ugly. Instead I go with ISPC
//    std lib behavior in this case and just return a qNaN - not quite symmetric
//    but at least it's always safe. Any opinions?
//
// I'll just go ahead and give "fast2" the same treatment as my half->float code,
// but if there's concerns with the way it works I might revise it later, so watch
// this spot.
//
// float_to_half_fast3: Bitfields removed. Ready for SSE2-ification :)
//
// float_to_half_SSE2: Exactly what it says on the tin. Beware, this works slightly
// differently from float_to_half_fast3 - the clamp and bias steps in the "normal" path
// are interchanged, since I get "minps" on every SSE2 target, but "pminsd" only for
// SSE4.1 targets. This code does what it should and is remarkably short, but the way
// it ended up working is "nonobvious" to phrase it politely.
//
// approx_float_to_half: Simpler (but less accurate) version that matches the Fox
// toolkit float->half conversions: http://blog.fox-toolkit.org/?p=40 - note that this
// also (incorrectly) translates some sNaNs into infinity, so be careful!
//
// approx_float_to_half_SSE2: SSE2 version of above.
//
// ----
//
// Oh, and enumerating+testing all 32-bit floats takes some time, especially since
// we will snap a significant fraction of the overall FP32 range to denormals, not
// exactly a fast operation. There's a reason this one prints regular progress
// reports. You've been warned.

#include <stdio.h>
#include <stdlib.h>
#include <emmintrin.h>

typedef unsigned int uint;

union FP32
{
    uint u;
    float f;
    struct
    {
        uint Mantissa : 23;
        uint Exponent : 8;
        uint Sign : 1;
    };
};

union FP16
{
    unsigned short u;
    struct
    {
        uint Mantissa : 10;
        uint Exponent : 5;
        uint Sign : 1;
    };
};

inline FP32 fromU32( uint u ) { FP32 fp; fp.u = u; return fp; }
inline FP32 fromF32( float f ) { FP32 fp; fp.f = f; return fp; }
inline FP16 fromU16( unsigned short u ) { FP16 fp; fp.u = u; return fp; }

static FP16 float_to_half_full( FP32 f )
{
    FP16 o = { 0 };

    // Based on ISPC reference code (with minor modifications)
    if ( f.Exponent == 0 ) // Signed zero/denormal (which will underflow)
        o.Exponent = 0;
    else if ( f.Exponent == 255 ) // Inf or NaN (all exponent bits set)
    {
        o.Exponent = 31;
        o.Mantissa = f.Mantissa ? 0x200 : 0; // NaN->qNaN and Inf->Inf
    }
    else // Normalized number
    {
        // Exponent unbias the single, then bias the halfp
        int newexp = f.Exponent - 127 + 15;
        if ( newexp >= 31 ) // Overflow, return signed infinity
            o.Exponent = 31;
        else if ( newexp <= 0 ) // Underflow
        {
            if ( (14 - newexp) <= 24 ) // Mantissa might be non-zero
            {
                uint mant = f.Mantissa | 0x800000; // Hidden 1 bit
                o.Mantissa = mant >> (14 - newexp);
                if ( (mant >> (13 - newexp)) & 1 ) // Check for rounding
                    o.u++; // Round, might overflow into exp bit, but this is OK
            }
        }
        else
        {
            o.Exponent = newexp;
            o.Mantissa = f.Mantissa >> 13;
            if ( f.Mantissa & 0x1000 ) // Check for rounding
                o.u++; // Round, might overflow to inf, this is OK
        }
    }

    o.Sign = f.Sign;
    return o;
}

static FP16 float_to_half_fast( FP32 f )
{
    FP16 o = { 0 };

    // Based on ISPC reference code (with minor modifications)
    if ( f.Exponent == 255 ) // Inf or NaN (all exponent bits set)
    {
        o.Exponent = 31;
        o.Mantissa = f.Mantissa ? 0x200 : 0; // NaN->qNaN and Inf->Inf
    }
    else // Normalized number
    {
        // Exponent unbias the single, then bias the halfp
        int newexp = f.Exponent - 127 + 15;
        if ( newexp >= 31 ) // Overflow, return signed infinity
            o.Exponent = 31;
        else if ( newexp <= 0 ) // Underflow
        {
            if ( (14 - newexp) <= 24 ) // Mantissa might be non-zero
            {
                uint mant = f.Mantissa | 0x800000; // Hidden 1 bit
                o.Mantissa = mant >> (14 - newexp);
                if ( (mant >> (13 - newexp)) & 1 ) // Check for rounding
                    o.u++; // Round, might overflow into exp bit, but this is OK
            }
        }
        else
        {
            o.Exponent = newexp;
            o.Mantissa = f.Mantissa >> 13;
            if ( f.Mantissa & 0x1000 ) // Check for rounding
                o.u++; // Round, might overflow to inf, this is OK
        }
    }

    o.Sign = f.Sign;
    return o;
}

static FP16 float_to_half_fast2( FP32 f )
{
    FP32 infty = { 31 << 23 };
    FP32 magic = { 15 << 23 };
    FP16 o = { 0 };

    uint sign = f.Sign;
    f.Sign = 0;

    // Based on ISPC reference code (with minor modifications)
    if ( f.Exponent == 255 ) // Inf or NaN (all exponent bits set)
    {
        o.Exponent = 31;
        o.Mantissa = f.Mantissa ? 0x200 : 0; // NaN->qNaN and Inf->Inf
    }
    else // (De)normalized number or zero
    {
        f.u &= ~0xfff; // Make sure we don't get sticky bits
        // Not necessarily the best move in terms of accuracy, but matches behavior
        // of other versions.

        // Shift exponent down, denormalize if necessary.
        // NOTE This represents half-float denormals using single precision denormals.
        // The main reason to do this is that there's no shift with per-lane variable
        // shifts in SSE*, which we'd otherwise need. It has some funky side effects
        // though:
        // - This conversion will actually respect the FTZ (Flush To Zero) flag in
        //   MXCSR - if it's set, no half-float denormals will be generated. I'm
        //   honestly not sure whether this is good or bad. It's definitely interesting.
        // - If the underlying HW doesn't support denormals (not an issue with Intel
        //   CPUs, but might be a problem on GPUs or PS3 SPUs), you will always get
        //   flush-to-zero behavior. This is bad, unless you're on a CPU where you don't
        //   care.
        // - Denormals tend to be slow. FP32 denormals are rare in practice outside of things
        //   like recursive filters in DSP - not a typical half-float application. Whether
        //   FP16 denormals are rare in practice, I don't know. Whatever slow path your HW
        //   may or may not have for denormals, this may well hit it.
        f.f *= magic.f;

        f.u += 0x1000; // Rounding bias
        if ( f.u > infty.u ) f.u = infty.u; // Clamp to signed infinity if overflowed

        o.u = f.u >> 13; // Take the bits!
    }

    o.Sign = sign;
    return o;
}

static FP16 float_to_half_fast3( FP32 f )
{
    FP32 f32infty = { 255 << 23 };
    FP32 f16infty = { 31 << 23 };
    FP32 magic = { 15 << 23 };
    uint sign_mask = 0x80000000u;
    uint round_mask = ~0xfffu;
    FP16 o = { 0 };

    uint sign = f.u & sign_mask;
    f.u ^= sign;

    // NOTE all the integer compares in this function can be safely
    // compiled into signed compares since all operands are below
    // 0x80000000. Important if you want fast straight SSE2 code
    // (since there's no unsigned PCMPGTD).

    if ( f.u >= f32infty.u ) // Inf or NaN (all exponent bits set)
        o.u = (f.u > f32infty.u) ? 0x7e00 : 0x7c00; // NaN->qNaN and Inf->Inf
    else // (De)normalized number or zero
    {
        f.u &= round_mask;
        f.f *= magic.f;
        f.u -= round_mask;
        if ( f.u > f16infty.u ) f.u = f16infty.u; // Clamp to signed infinity if overflowed

        o.u = f.u >> 13; // Take the bits!
    }

    o.u |= sign >> 16;
    return o;
}

// Approximate solution. This is faster but converts some sNaNs to
// infinity and doesn't round correctly. Handle with care.
static FP16 approx_float_to_half( FP32 f )
{
    FP32 f32infty = { 255 << 23 };
    FP32 f16max = { (127 + 16) << 23 };
    FP32 magic = { 15 << 23 };
    FP32 expinf = { (255 ^ 31) << 23 };
    uint sign_mask = 0x80000000u;
    FP16 o = { 0 };

    uint sign = f.u & sign_mask;
    f.u ^= sign;

    if ( !(f.f < f32infty.u) ) // Inf or NaN
        o.u = f.u ^ expinf.u;
    else
    {
        if ( f.f > f16max.f ) f.f = f16max.f;
        f.f *= magic.f;
    }

    o.u = f.u >> 13; // Take the mantissa bits
    o.u |= sign >> 16;
    return o;
}

static __m128i float_to_half_SSE2( __m128 f )
{
#define FTH_SSE_CONST4(name, val) static const __declspec(align(16)) uint name[4] = { (val), (val), (val), (val) }
#define FTH_CONST(name) *(const __m128i *)&name
#define FTH_CONSTF(name) *(const __m128 *)&name

    FTH_SSE_CONST4( mask_sign, 0x80000000u );
    FTH_SSE_CONST4( mask_round, ~0xfffu );
    FTH_SSE_CONST4( c_f32infty, 255 << 23 );
    FTH_SSE_CONST4( c_magic, 15 << 23 );
    FTH_SSE_CONST4( c_nanbit, 0x200 );
    FTH_SSE_CONST4( c_infty_as_fp16, 0x7c00 );
    FTH_SSE_CONST4( c_clamp, (31 << 23) - 0x1000 );

    __m128  msign = FTH_CONSTF( mask_sign );
    __m128  justsign = _mm_and_ps( msign, f );
    __m128i f32infty = FTH_CONST( c_f32infty );
    __m128  absf = _mm_xor_ps( f, justsign );
    __m128  mround = FTH_CONSTF( mask_round );
    __m128i absf_int = _mm_castps_si128( absf ); // pseudo-op, but val needs to be copied once so count as mov
    __m128i b_isnan = _mm_cmpgt_epi32( absf_int, f32infty );
    __m128i b_isnormal = _mm_cmpgt_epi32( f32infty, _mm_castps_si128( absf ) );
    __m128i nanbit = _mm_and_si128( b_isnan, FTH_CONST( c_nanbit ) );
    __m128i inf_or_nan = _mm_or_si128( nanbit, FTH_CONST( c_infty_as_fp16 ) );

    __m128  fnosticky = _mm_and_ps( absf, mround );
    __m128  scaled = _mm_mul_ps( fnosticky, FTH_CONSTF( c_magic ) );
    __m128  clamped = _mm_min_ps( scaled, FTH_CONSTF( c_clamp ) ); // logically, we want PMINSD on "biased", but this should gen better code
    __m128i biased = _mm_sub_epi32( _mm_castps_si128( clamped ), _mm_castps_si128( mround ) );
    __m128i shifted = _mm_srli_epi32( biased, 13 );
    __m128i normal = _mm_and_si128( shifted, b_isnormal );
    __m128i not_normal = _mm_andnot_si128( b_isnormal, inf_or_nan );
    __m128i joined = _mm_or_si128( normal, not_normal );

    __m128i sign_shift = _mm_srli_epi32( _mm_castps_si128( justsign ), 16 );
    __m128i final = _mm_or_si128( joined, sign_shift );

    // ~20 SSE2 ops
    return final;

#undef FTH_SSE_CONST4
#undef FTH_CONST
#undef FTH_CONSTF
}

static __m128i approx_float_to_half_SSE2( __m128 f )
{
#define FTH_SSE_CONST4(name, val) static const __declspec(align(16)) uint name[4] = { (val), (val), (val), (val) }
#define FTH_CONSTF(name) *(const __m128 *)&name

    FTH_SSE_CONST4( mask_fabs, 0x7fffffffu );
    FTH_SSE_CONST4( c_f32infty, (255 << 23) );
    FTH_SSE_CONST4( c_expinf, (255 ^ 31) << 23 );
    FTH_SSE_CONST4( c_f16max, (127 + 16) << 23 );
    FTH_SSE_CONST4( c_magic, 15 << 23 );

    __m128  mabs = FTH_CONSTF( mask_fabs );
    __m128  fabs = _mm_and_ps( mabs, f );
    __m128  justsign = _mm_xor_ps( f, fabs );

    __m128  f16max = FTH_CONSTF( c_f16max );
    __m128  expinf = FTH_CONSTF( c_expinf );
    __m128  infnancase = _mm_xor_ps( expinf, fabs );
    __m128  clamped = _mm_min_ps( f16max, fabs );
    __m128  b_notnormal = _mm_cmpnlt_ps( fabs, FTH_CONSTF( c_f32infty ) );
    __m128  scaled = _mm_mul_ps( clamped, FTH_CONSTF( c_magic ) );

    __m128  merge1 = _mm_and_ps( infnancase, b_notnormal );
    __m128  merge2 = _mm_andnot_ps( b_notnormal, scaled );
    __m128  merged = _mm_or_ps( merge1, merge2 );

    __m128i shifted = _mm_srli_epi32( _mm_castps_si128( merged ), 13 );
    __m128i signshifted = _mm_srli_epi32( _mm_castps_si128( justsign ), 16 );
    __m128i final = _mm_or_si128( shifted, signshifted );

    // ~15 SSE2 ops
    return final;

#undef FTH_SSE_CONST4
#undef FTH_CONSTF
}

// from fox toolkit float->half code (which "approx" variants match)
static uint basetable[512];
static unsigned char shifttable[512];

static void generatetables(){
    unsigned int i;
    int e;
    for ( i = 0; i<256; ++i ){
        e = i - 127;
        if ( e<-24 ){                  // Very small numbers map to zero
            basetable[i | 0x000] = 0x0000;
            basetable[i | 0x100] = 0x8000;
            shifttable[i | 0x000] = 24;
            shifttable[i | 0x100] = 24;
        }
        else if ( e<-14 ){             // Small numbers map to denorms
            basetable[i | 0x000] = (0x0400 >> (-e - 14));
            basetable[i | 0x100] = (0x0400 >> (-e - 14)) | 0x8000;
            shifttable[i | 0x000] = -e - 1;
            shifttable[i | 0x100] = -e - 1;
        }
        else if ( e <= 15 ){             // Normal numbers just lose precision
            basetable[i | 0x000] = ((e + 15) << 10);
            basetable[i | 0x100] = ((e + 15) << 10) | 0x8000;
            shifttable[i | 0x000] = 13;
            shifttable[i | 0x100] = 13;
        }
        else if ( e<128 ){             // Large numbers map to Infinity
            basetable[i | 0x000] = 0x7C00;
            basetable[i | 0x100] = 0xFC00;
            shifttable[i | 0x000] = 24;
            shifttable[i | 0x100] = 24;
        }
        else{                       // Infinity and NaN's stay Infinity and NaN's
            basetable[i | 0x000] = 0x7C00;
            basetable[i | 0x100] = 0xFC00;
            shifttable[i | 0x000] = 13;
            shifttable[i | 0x100] = 13;
        }
    }
}

// also from fox toolkit
static uint float_to_half_foxtk( uint f )
{
    return basetable[(f >> 23) & 0x1ff] + ((f & 0x007fffff) >> shifttable[(f >> 23) & 0x1ff]);
}

// from half->float code - just for verification.
static FP32 half_to_float( FP16 h )
{
    static const FP32 magic = { 113 << 23 };
    static const uint shifted_exp = 0x7c00 << 13; // exponent mask after shift
    FP32 o;

    o.u = (h.u & 0x7fff) << 13;     // exponent/mantissa bits
    uint exp = shifted_exp & o.u;   // just the exponent
    o.u += (127 - 15) << 23;        // exponent adjust

    // handle exponent special cases
    if ( exp == shifted_exp ) // Inf/NaN?
        o.u += (128 - 16) << 23;    // extra exp adjust
    else if ( exp == 0 ) // Zero/Denormal?
    {
        o.u += 1 << 23;             // extra exp adjust
        o.f -= magic.f;             // renormalize
    }

    o.u |= (h.u & 0x8000) << 16;    // sign bit
    return o;
}
