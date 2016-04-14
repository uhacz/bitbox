passes:
{
    skyPreetham =
    {
        vertex = "vs_screenquad";
        pixel = "ps_main";

        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
        };
    };
};#~header

#include <sys/base.hlsl>
#include <sys/types.hlsl>
#include <sys/vs_screenquad.hlsl>

#define in_PS out_VS_screenquad

#ifdef skyPreetham
float saturatedDot( in float3 a, in float3 b )
{
    return saturate( dot( a, b ) );
}

float3 YxyToXYZ( in float3 Yxy )
{
    float Y = Yxy.r;
    float x = Yxy.g;
    float y = Yxy.b;

    float X = x * (Y / y);
    float Z = (1.0 - x - y) * (Y / y);

    return float3( X, Y, Z );
}

float3 XYZToRGB( in float3 XYZ )
{
    // CIE/E
    float3x3 M = float3x3
    (
        2.3706743, -0.9000405, -0.4706338,
        -0.5138850, 1.4253036, 0.0885814,
        0.0052982, -0.0146949, 1.0093968
    );

    return mul( M, XYZ );
}


float3 YxyToRGB( in float3 Yxy )
{
    float3 XYZ = YxyToXYZ( Yxy );
    float3 RGB = XYZToRGB( XYZ );
    return RGB;
}

void calculatePerezDistribution( in float t, out float3 A, out float3 B, out float3 C, out float3 D, out float3 E )
{
    A = float3( 0.1787 * t - 1.4630, -0.0193 * t - 0.2592, -0.0167 * t - 0.2608 );
    B = float3( -0.3554 * t + 0.4275, -0.0665 * t + 0.0008, -0.0950 * t + 0.0092 );
    C = float3( -0.0227 * t + 5.3251, -0.0004 * t + 0.2125, -0.0079 * t + 0.2102 );
    D = float3( 0.1206 * t - 2.5771, -0.0641 * t - 0.8989, -0.0441 * t - 1.6537 );
    E = float3( -0.0670 * t + 0.3703, -0.0033 * t + 0.0452, -0.0109 * t + 0.0529 );
}

float3 calculateZenithLuminanceYxy( in float t, in float thetaS )
{
    float chi = (4.0 / 9.0 - t / 120.0) * (PI - 2.0 * thetaS);
    float Yz = (4.0453 * t - 4.9710) * tan( chi ) - 0.2155 * t + 2.4192;

    float theta2 = thetaS * thetaS;
    float theta3 = theta2 * thetaS;
    float T = t;
    float T2 = t * t;

    float xz =
        (0.00165 * theta3 - 0.00375 * theta2 + 0.00209 * thetaS + 0.0)     * T2 +
        (-0.02903 * theta3 + 0.06377 * theta2 - 0.03202 * thetaS + 0.00394) * T +
        (0.11693 * theta3 - 0.21196 * theta2 + 0.06052 * thetaS + 0.25886);

    float yz =
        (0.00275 * theta3 - 0.00610 * theta2 + 0.00317 * thetaS + 0.0)     * T2 +
        (-0.04214 * theta3 + 0.08970 * theta2 - 0.04153 * thetaS + 0.00516) * T +
        (0.15346 * theta3 - 0.26756 * theta2 + 0.06670 * thetaS + 0.26688);

    return float3( Yz, xz, yz );
}
float3 calculatePerezLuminanceYxy( in float theta, in float gamma, in float3 A, in float3 B, in float3 C, in float3 D, in float3 E )
{
    float cosGamma2 = cos( gamma );
    cosGamma2 *= cosGamma2;
    return (1.0 + A * exp( B / ( cos( theta ) + 0.01 ) )) * (1.0 + C * ( exp( D * gamma ) - exp( D * PI*0.5f) ) + E * cosGamma2 );
}

float3 calculateSkyLuminanceRGB( in float3 s, in float3 e, in float t )
{
    float3 A, B, C, D, E;
    calculatePerezDistribution( t, A, B, C, D, E );

    float thetaS = acos( saturatedDot( s, float3( 0, 1, 0 ) ) );
    float thetaE = acos( saturatedDot( e, float3( 0, 1, 0 ) ) );
    float gammaE = acos( saturatedDot( s, e ) );

    float3 Yz = calculateZenithLuminanceYxy( t, thetaS );

    float3 fThetaGamma = calculatePerezLuminanceYxy( thetaE, gammaE, A, B, C, D, E );
    float3 fZeroThetaS = calculatePerezLuminanceYxy( 0.0, thetaS, A, B, C, D, E );

    float3 Yp = Yz * (fThetaGamma / fZeroThetaS);

    return YxyToRGB( Yp );
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

struct PS_OUT
{
    float4 rgba : SV_Target0;
    //float depth : SV_Depth;
};

PS_OUT ps_main( in_PS IN )
{
    float2 uv_m11 = float2(IN.uv.x, 1.0 - IN.uv.y) * 2.0 - 1.0;
    uv_m11.x *= _camera_aspect; // apect
    
    float turbidity = 3.5;
    float3 sunDir = -_sunDirection; // normalize( float3( -1.f, 1.0f, 0.f ) );
    float3 viewDir = normalize( mul( ( float3x3 )_camera_world, float3( uv_m11, -2.0 ) ) );
    float3 skyLuminance = calculateSkyLuminanceRGB( sunDir, viewDir, turbidity );
    skyLuminance = ACESFilm( skyLuminance * 0.005 ); //(float3)1.0 - exp( -(skyLuminance * 0.1 ) );
    
    PS_OUT output;
    output.rgba = float4( skyLuminance * _skyIlluminanceInLux , 1.0 );
    //output.depth = 1.0;

    return output;
    //return float4(skyLuminance * 0.05f, 1.0);
}

#endif
