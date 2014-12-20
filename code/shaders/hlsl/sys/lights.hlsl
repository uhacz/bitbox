#ifndef LIGHTS_HLSL
#define LIGHTS_HLSL

////
////
shared cbuffer LighningData : register(b2)
{
    uint2 _numTilesXY;
    uint  _numTiles;
    uint  _tileSize;
    uint  _maxLights;
    float _tileSizeRcp;

    float _sunAngularRadius;
    float _sunIlluminanceInLux;
    float __padding0;
    float3 _sunDirection;
    float3 _sunColor;
    float __padding1;
};

Buffer<float4> _lightsData    : register(t0);
Buffer<uint>   _lightsIndices : register(t1);
uint2 computeTileXY( in float2 screenPos, in uint2 numTilesXY, in float2 rtSize, in float tileSizeRcp )
{
    const uint2 unclamped = (uint2)(screenPos * rtSize * tileSizeRcp);
    return clamp( unclamped, uint2(0, 0), numTilesXY - uint2(1,1) );
}

float2 evaluateSunLight( float3 V, float3 N, float3 sunDirection, float sunAngRad, float sunLux, float3 surfPos, in float rough, in float reflectance )
{
    const float r = sin( sunAngRad );
    const float d = cos( sunAngRad );
    const float3 D = -sunDirection;
    const float3 R = normalize( D - normalize( surfPos ) );
    const float DdotR = dot( D, R );
    const float3 S = R - DdotR * D;    const float3 L = DdotR < d ? normalize( d * D + normalize( S ) * r ) : R;
    const float illuminance = sunLux * saturate( dot( N, D ) );

    const float Fd = BRDF_Diffuse( D, V, N, rough );
    const float Fr = BRDF_Specular( L, V, N, rough, reflectance );

    return float2(Fd, Fr) * illuminance;
}

////
////

////
////
float smoothDistanceAtt( float squaredDistance, float invSqrAttRadius )
{
    float factor = squaredDistance * invSqrAttRadius;
    float smoothFactor = saturate( 1.0 - factor * factor );
    return smoothFactor * smoothFactor;
}
float getDistanceAtt( float3 unormalizedLightVector, float invSqrAttRadius )
{
    float sqrDist = dot( unormalizedLightVector, unormalizedLightVector );
    float attenuation = 1.0 / (max( sqrDist, 0.01*0.01 ));
    attenuation *= smoothDistanceAtt( sqrDist, invSqrAttRadius );

    return attenuation;
}
#endif