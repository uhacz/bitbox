passes:
{
    ssao =
    {
        vertex = "vs_screenquad";
        pixel = "ps_ssao";
        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            color_mask = "RG";
        };
    };

    blurX = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_blurX";

        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            color_mask = "RG";
        };
    };

    blurY = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_blurY";

        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            color_mask = "R";
        };
    };

    ambientTransfer =
    {
        vertex = "vs_screenquad";
        pixel = "ps_ambientTransfer";
        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            //color_mask = "R";
        };
    };

}; #~header

#include <sys/frame_data.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/util.hlsl>

//Texture2D<float>  InputTextureLinearDepth       : register(t0);
//Texture2D<float2> InputTextureSSAO              : register(t1);
//Texture2D<float2> InputTextureMotion            : register(t2);
//Texture2D<float4> tex_normalsVS;
Texture2D<float> texHwDepth;

shared cbuffer MaterialData: register(b3)
{
    float _radius;
    float _radius2;
    float _bias;
    float _intensity;
    float _projScale;
    float _randomRot;
    float2 _ssaoTexSize;
};



float loadLinearDepth( int2 ssP )
{
    float hwDepth = texHwDepth.Load( int3(ssP, 0) ).r;
    float linDepth = resolveLinearDepth( hwDepth );
    return linDepth;
}



// Total number of direct samples to take at each pixel
#define NUM_SAMPLES (11)

static const int ROTATIONS[] = { 1, 1, 2, 3, 2, 5, 2, 3, 2,
3, 3, 5, 5, 3, 4, 7, 5, 5, 7,
9, 8, 5, 5, 7, 7, 7, 8, 5, 8,
11, 12, 7, 10, 13, 8, 11, 8, 7, 14,
11, 11, 13, 12, 13, 19, 17, 13, 11, 18,
19, 11, 11, 14, 17, 21, 15, 16, 17, 18,
13, 17, 11, 17, 19, 18, 25, 18, 19, 19,
29, 21, 19, 27, 31, 29, 21, 18, 17, 29,
31, 31, 23, 18, 25, 26, 25, 23, 19, 34,
19, 27, 21, 25, 39, 29, 17, 21, 27 };

// If using depth mip levels, the log of the maximum pixel offset before we need to switch to a lower 
// miplevel to maintain reasonable spatial locality in the cache
// If this number is too small (< 3), too many taps will land in the same pixel, and we'll get bad variance that manifests as flashing.
// If it is too high (> 5), we'll get bad performance because we're not using the MIP levels effectively
#define LOG_MAX_OFFSET 3

// This must be less than or equal to the MAX_MIP_LEVEL defined in SSAO.cpp
#define MAX_MIP_LEVEL 4

/** Used for preventing AO computation on the sky (at infinite depth) and defining the CS Z to bilateral depth key scaling. 
    This need not match the real far plane*/
#define FAR_PLANE_Z (90.0)

// This is the number of turns around the circle that the spiral pattern makes.  This should be prime to prevent
// taps from lining up.  This particular choice was tuned for NUM_SAMPLES == 9
// BW: Morgan pre-computed those values in later paper to maximize variance
static const int NUM_SPIRAL_TURNS = ROTATIONS[NUM_SAMPLES - 1];

/** World-space AO radius in scene units (r).  e.g., 1.0m */
//static const float radius = 1.2f;
/** radius*radius*/
//static const float radius2 = (1.2f*1.2f);

/** Bias to avoid AO in smooth corners, e.g., 0.01m */
//static const float bias = 0.025f;

/** Darkending factor, e.g., 1.0 */
//static const float intensity = 0.4f;

/** The height in pixels of a 1m object if viewed from 1m away.  
    You can compute it from your projection matrix.  The actual value is just
    a scale factor on radius; you can simply hardcode this to a constant (~500)
    and make your radius value unitless (...but resolution dependent.)  */
// BW: todo precalc this on cpu
//static const float projScale = 700.0f;

/** Reconstruct camera-space P.xyz from screen-space S = (x, y) in
    pixels and camera-space z < 0.  Assumes that the upper-left pixel center
    is at (0.5, 0.5) [but that need not be the location at which the sample tap 
    was placed!]
  */
// BW: I precomputed those values on the CPU - 2x2 less ALU ops :) 
float3 reconstructCSPosition(float2 S, float z) 
{
    return float3((S * _reprojectInfoFromInt.xy + _reprojectInfoFromInt.zw)*z,z);
}

/** Reconstructs screen-space unit normal from screen-space position */
float3 reconstructCSFaceNormal(float3 C) 
{
    return normalize(cross(ddy_fine(C), ddx_fine(C)));
}

/** Returns a unit vector and a screen-space radius for the tap on a unit disk (the caller should scale by the actual disk radius) */
float2 tapLocation(int sampleNumber, float spinAngle, out float ssR)
{
    // Radius relative to ssR
    float alpha = float(sampleNumber + 0.5) * (1.0 / NUM_SAMPLES);
    float angle = alpha * (NUM_SPIRAL_TURNS * 6.28) + spinAngle;

    ssR = alpha;
    float sin_v, cos_v;
    sincos(angle,sin_v,cos_v);
    return float2(cos_v, sin_v);
}


/** Used for packing Z into the GB channels */
float CSZToKey(float z) 
{
    return clamp(z * (1.0 / FAR_PLANE_Z), 0.0, 1.0);
}

/** Used for packing Z into the GB channels */
//void packKey(float key, out float2 p) 
//{
//    // Round to the nearest 1/256.0
//    float temp = floor(key * 256.0);
//    // Integer part
//    p.x = temp * (1.0 / 256.0);
//    // Fractional part
//    p.y = key * 256.0 - temp;
//}
void packKey1( float key, out float p )
{
    p = key;
    //// Round to the nearest 1/256.0
    //float temp = floor( key * 256.0 );
    //
    //// Integer part
    //float i = temp * ( 1.0 / 256.0 );
    //// Fractional part
    //float f = key * 256.0 - temp;
    //
    //uint ihalf = f32tof16( i );
    //uint fhalf = f32tof16( f );
    //p.x = asfloat( ihalf << 16 | fhalf );
}
/** Read the camera-space position of the point at screen-space pixel ssP */
float3 getPosition(int2 ssP) 
{
    float3 P;

    P.z = loadLinearDepth( ssP );//CS_Z_buffer.Load(int3(ssP, 0)).r;

    // Offset to pixel center
    P = reconstructCSPosition(float2(ssP), P.z);
    return P;
}

/** Read the camera-space position of the point at screen-space pixel ssP + unitOffset * ssR.  Assumes length(unitOffset) == 1 */
float3 getOffsetPosition(int2 ssC, float2 unitOffset, float ssR) 
{
    // Derivation:
    //  mipLevel = floor(log(ssR / MAX_OFFSET));
    int mipLevel = clamp((int)floor(log2(ssR)) - LOG_MAX_OFFSET, 0, MAX_MIP_LEVEL);

    int2 ssP = int2(ssR*unitOffset) + ssC;

    float3 P;

    // Divide coordinate by 2^mipLevel
    P.z = loadLinearDepth( ssP ); //CS_Z_buffer.Load(int3(ssP >> mipLevel, mipLevel)).r;

    // Offset to pixel center
    P = reconstructCSPosition(float2(ssP), P.z);

    return P;
}

// Derived from batch testing
#define IEEE_INT_RCP_CONST_NR0 0x7EF311C2  
float rcpIEEEIntApproximation(float inX, const int inRcpConst)
{
    int x = asint(inX);
    x = inRcpConst - x;
    return asfloat(x);
}
float fastRcpNR0(float inX)
{
    float  xRcp = rcpIEEEIntApproximation(inX, IEEE_INT_RCP_CONST_NR0);
    return xRcp;
}

/** Compute the occlusion due to sample with index \a i about the pixel at \a ssC that corresponds
    to camera-space point \a C with unit normal \a n_C, using maximum screen-space sampling radius \a ssDiskRadius */
float sampleAO(in int2 ssC, in float3 C, in float3 n_C, in float ssDiskRadius, in int tapIndex, in float randomPatternRotationAngle) 
{
    // Offset on the unit disk, spun for this pixel
    float ssR;
    float2 unitOffset = tapLocation(tapIndex, randomPatternRotationAngle, ssR);
    ssR *= ssDiskRadius;

    // The occluding point in camera space
    float3 Q = getOffsetPosition(ssC, unitOffset, ssR);

    float3 v = Q - C;

    float vv = dot(v, v);
    float vn = dot(v, n_C);

    const float epsilon = 0.02f;
    float f = max(_radius2 - vv, 0.0); 
    return f * f * max((vn - _bias) * fastRcpNR0(epsilon + vv), 0.0);
}

//float unpackKey(float2 p)
//{
//    return p.x * (256.0 / 257.0) + p.y * (1.0 / 257.0);
//}
float unpackKey1( float p )
{
    return p;
}

#define visibility      output.r
#define bilateralKey    output.g
//#define bilateralKey    output.gb


float4 ps_ssao( out_VS_screenquad In ) : SV_Target
{
    float2 vPos = In.uv * _renderTarget_size;

    // Pixel being shaded 
    int2 ssC = vPos;

    float4 output = float4(1,1,1,1);

    // World space point being shaded
    float3 C = getPosition(ssC);

    //float shadows = tex_shadows.Load( int3( ssC, 0 ) ).r;

    // bw: early out on borders of screen to avoid over-darkening, load defaults to 0
    bool earlyOut = C.z > FAR_PLANE_Z || C.z < 0.4f || any(ssC < 8) || any(ssC >= (_renderTarget_size-8));// || shadows >= 1.f ;
    [branch]
    if(earlyOut)
    {
        return output;
    }

    // bw: unpack motion vector
    //float2 diffVector = (motion.Load(int3(ssC, 0)) - 0.4980f)*float2(-0.125f,0.125f);
    //float4 prevFrame = reproject.SampleLevel(linearSamp, In.vTexCoord.xy+diffVector, 0);
    //float keyPrevFrame = unpackKey(prevFrame.gb);
    //float aoPrevFrame = prevFrame.r;

    // Hash function used in the HPG12 AlchemyAO paper + random rotation from CPU
    float randomPatternRotationAngle = (3 * ssC.x ^ ssC.y + ssC.x * ssC.y) * 10; // + _randomRot;

    // Reconstruct normals from positions. These will lead to 1-pixel black lines
    // at depth discontinuities, however the blur will wipe those out so they are not visible
    // in the final image.
    float3 n_C = reconstructCSFaceNormal(C);

    // BW: per Guillaume request, we actually do "regular" normals from normal maps - more detail
    //float3 n_C = normalize(mul(float4(2.0f*(tex_normalsVS.Load(int3(ssC, 0)).rgb-0.5f),0),_camera_view).xyz);
    //float3 n_C = normalize( mul( (float3x3)_camera_proj, tex_normalsVS.Load( int3(ssC, 0) ).rgb ) ) * float3(-1.f, 1.f, 1.f );


    // Choose the screen-space sample radius
    // proportional to the projected area of the sphere, default params were tweaked for 900p so scale projScale accordingly - 1 ALU mul operation as projScale/900.0f gets optimized by compiler
    float ssDiskRadius = (_projScale / 1080.0 * _renderTarget_size.y) * _radius * fastRcpNR0(max(C.z, 0.1f));

    float sum = 0.0;

    //[unroll]
    for (int i = 0; i < NUM_SAMPLES; ++i) 
    {
         sum += sampleAO(ssC, C, n_C, ssDiskRadius, i, randomPatternRotationAngle);
    }

    const float temp = _radius2 * _radius;
    sum /= temp * temp;

    float A = max(0.0f, 1.0f - sum * _intensity * (5.0f / NUM_SAMPLES));

    // bilat filter 1-pixel wide for "free"
	if (abs(ddx(C.z)) < 0.1) 
    {
		A -= ddx_fine(A) * ((ssC.x & 1) - 0.5);
	}
	if (abs(ddy(C.z)) < 0.1) 
    {
		A -= ddy_fine(A) * ((ssC.y & 1) - 0.5);
	}

    //float difference = saturate(1.0f - 200*abs(zKey-keyPrevFrame));
    //float temporalExponent = 0.8f;
    //visibility = lerp(visibility, aoPrevFrame, temporalExponent*difference);
    //visibility = pow( A / _radius2, 2.0 );
    
    float zKey = CSZToKey(C.z);
    packKey1(zKey, bilateralKey);
    visibility = A;
    //if ( In.uv.x < 0.5f )
    //    return float4(n_C1, 0.f);
    //else
    //    return float4(n_C, 0.f);
    return output; // float4(n_C1, 1.0);
}


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

/** Increase to make edges crisper. Decrease to reduce temporal flicker. */
#define EDGE_SHARPNESS     (0.3)

/** Step in 2-pixel intervals since we already blurred against neighbors in the
    first AO pass.  This constant can be increased while R decreases to improve
    performance at the expense of some dithering artifacts. 
    
    Morgan found that a scale of 3 left a 1-pixel checkerboard grid that was
    unobjectionable after shading was applied but eliminated most temporal incoherence
    from using small numbers of sample taps.
    */
#define SCALE               (2)

/** Filter radius in pixels. This will be multiplied by SCALE. */
#define R                   (5)



//////////////////////////////////////////////////////////////////////////////////////////////

/** Type of data to read from source.  This macro allows
    the same blur shader to be used on different kinds of input data. */
#define VALUE_TYPE        float

/** Swizzle to use to extract the channels of source. This macro allows
    the same blur shader to be used on different kinds of input data. */
#define VALUE_COMPONENTS   r

#define VALUE_IS_KEY       0

/** Channel encoding the bilateral key value (which must not be the same as VALUE_COMPONENTS) */
//#define KEY_COMPONENTS     gb
#define KEY_COMPONENTS     g
// Gaussian coefficients
static const float gaussian[] = 
//	{ 0.356642, 0.239400, 0.072410, 0.009869 };
//	{ 0.398943, 0.241971, 0.053991, 0.004432, 0.000134 };  // stddev = 1.0
  //  { 0.153170, 0.144893, 0.122649, 0.092902, 0.062970 };  // stddev = 2.0
  { 0.111220, 0.107798, 0.098151, 0.083953, 0.067458, 0.050920, 0.036108 }; // stddev = 3.0
  //{ 0.106595, 0.140367, 0.165569, 0.174938, 0.165569, 0.140367, 0.106595 };

Texture2D<float4> tex_source        : register(t0);        // Param: InputTexture

#define  result         output.VALUE_COMPONENTS
#define  keyPassThrough output.KEY_COMPONENTS

float4 doBlur( int2 ssC, float2 axis )
{
    float4 output = 1.0f;

    float4 temp = tex_source.Load( int3(ssC, 0) );

    keyPassThrough = temp.KEY_COMPONENTS;
    float key = unpackKey1( keyPassThrough );

    float sum = temp.VALUE_COMPONENTS;

    [branch]
    if ( key == 1.0 )
    {
        // Sky pixel (if you aren't using depth keying, disable this test)
        result = sum;
        return output;
    }

    // Base weight for depth falloff.  Increase this for more blurriness,
    // decrease it for better edge discrimination
    float BASE = gaussian[0];
    float totalWeight = BASE;
    sum *= totalWeight;

    [unroll]
    for ( int r = -R; r <= R; ++r )
    {
        // We already handled the zero case above.  This loop should be unrolled and the branch discarded
        if ( r != 0 )
        {
            temp = tex_source.Load( int3(ssC + axis * (r * SCALE), 0) );
            float tapKey = unpackKey1( temp.KEY_COMPONENTS );
            float value = temp.VALUE_COMPONENTS;

            // spatial domain: offset gaussian tap
            float weight = 0.3 + gaussian[abs( r )];

            // range domain (the "bilateral" weight). As depth difference increases, decrease weight.
            weight *= max( 0.0, 1.0 - (2000.0 * EDGE_SHARPNESS) * abs( tapKey - key ) );

            sum += value * weight;
            totalWeight += weight;
        }
    }

    const float epsilon = 0.0001;
    result = sum / (totalWeight + epsilon);
    
#ifdef blurY
    float afterGamma = result;
    afterGamma = pow( afterGamma, 2.2 );
    output = afterGamma.xxxx;
#endif
    
    return output;
}

float4 ps_blurX( out_VS_screenquad In ) : SV_Target
{
    float2 vPos = In.uv * _ssaoTexSize;
    return doBlur( (int2)vPos, float2(1, 0) );
}
float4 ps_blurY( out_VS_screenquad In ) : SV_Target
{
    float2 vPos = In.uv * _ssaoTexSize;
    return doBlur( (int2)vPos, float2(0, 1) );
}

#ifdef ambientTransfer

Texture2D texAlbedo;
Texture2D texNoise;
SamplerState samplerNoise;
SamplerState samplerAlbedo;

float4 ps_ambientTransfer( out_VS_screenquad IN ) : SV_Target
{
    float2 vPos = IN.uv * _renderTarget_size;

    // Pixel being shaded 
    int2 ssC = vPos;

    //float depth = loadLinearDepth( ssPixel );
    //return depth;
    float3 cPos = getPosition( ssC );

    bool earlyOut = cPos.z > FAR_PLANE_Z || cPos.z < 0.4f || any( ssC < 8 ) || any( ssC >= (_renderTarget_size - 8) );// || shadows >= 1.f ;
    [branch]
    if ( earlyOut )
    {
        return (float4)1;
    }

    float randomPatternRotationAngle = (3 * ssC.x ^ ssC.y + ssC.x * ssC.y) * 10; // + _randomRot;
    float ssDiskRadius = (1080 / 1080.0 * _renderTarget_size.y) * 0.5 * fastRcpNR0( max( cPos.z, 0.1f ) );
    
    float3 N = reconstructCSFaceNormal( cPos );
    float3 T = normalize( cross( N, float3(0.1, 0, 0.1) ) );
    float3 B = cross( N, T );
    float3x3 TBN = float3x3(T, B, N);

    // Interleaved sampling
    float3 r1 = texNoise.Sample( samplerNoise, vPos ).xyz;
    float3 r3 = float3(0, 0, 1);
    float3 r2 = float3(r1.y, -r1.x, 0);
    TBN = mul( float3x3(r1, r2, r3), TBN );

    const float rad = 0.5;
    const float bias = 0.1f;
    const int n = 8;

    float4 W = float4( n, n, n, 0); // n = count

    //[loop]
    for ( int k = 0; k < n; k++ ) 
    {
        // Offset on the unit disk, spun for this pixel
        float ssR;
        float2 unitOffset = tapLocation( k, randomPatternRotationAngle, ssR );
        ssR *= ssDiskRadius;

        // The occluding point in camera space
        float3 Q = getOffsetPosition( ssC, unitOffset, ssR );
        float3 v = Q - cPos;

        //W.xyz = v;
        //break;

        // Transform samples to camera space
        float3 sCPos = cPos + mul( TBN, v ) * rad;
        float4 sSPos = mul( _camera_proj, float4(sCPos, 1) );

        sSPos.xy = ( sSPos.xy / sSPos.w ) * 0.5 + 0.5;
        
        float vv = dot( v, v );
        float vn = dot( v, N );

        const float epsilon = 0.02f;
        float f = max( _radius2 - vv, 0.0 );
        f = f * max( (vn - _bias) * fastRcpNR0( epsilon + vv ), 0.0 );
        W.rgb -= ( 1.f - texAlbedo.Sample( samplerAlbedo, sSPos.xy ).rgb )* f;
    }

    //W.rgb /= n;
    return W / n;

}
#endif
