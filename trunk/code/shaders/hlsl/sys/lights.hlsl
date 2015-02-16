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
    float _skyIlluminanceInLux;
    
    float3 _sunDirection;
    float3 _sunColor;
    float3 _skyColor;
};



Buffer<float4> _lightsData    : register(t0);
Buffer<uint>   _lightsIndices : register(t1);
uint2 computeTileXY( in float2 screenPos, in uint2 numTilesXY, in float2 rtSize, in float tileSizeRcp )
{
    const uint2 unclamped = (uint2)(screenPos * rtSize * tileSizeRcp);
    return clamp( unclamped, uint2(0, 0), numTilesXY - uint2(1,1) );
}

float3 evaluateSunLight( in ShadingData shd, float3 surfPos, in Material mat, in float shadow )
{
    const float3 N = shd.N;
    const float r = sin( _sunAngularRadius );
    const float d = cos( _sunAngularRadius );
    const float3 D = -_sunDirection;
    const float3 R = normalize( D - normalize( surfPos ) );
    const float DdotR = dot( D, R );
    const float3 S = R - DdotR * D;    const float3 L = DdotR < d ? normalize( d * D + normalize( S ) * r ) : R;

    float w = mat.ambientCoeff;
    float n = 4.f;
    float ind = saturate( dot( N, D*float3(-1.f,0.f,-1.f) ) );
    const float illuminance = _sunIlluminanceInLux * ( ( wrappedLambert( saturate( dot( N, D ) ), w, n ) ) * shadow + (ind*0.2) );
    //const float illuminance = sunLux * ( saturate( dot( N, D ) ) + ind );

    float3 skyIndirect = saturate( 0.5 * 0.5*N.y ) * _skyColor * _skyIlluminanceInLux * 0.3;
    const float3 Fd = BRDF_diffuseOnly( D, shd, mat );
    const float3 Fr = BRDF_specularOnly( L, shd, mat );

    return (Fd + Fr) * illuminance + skyIndirect;
    //return Fd * illuminance;
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