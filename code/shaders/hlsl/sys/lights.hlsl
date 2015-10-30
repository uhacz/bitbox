#ifndef LIGHTS_HLSL
#define LIGHTS_HLSL

uint2 computeTileXY( in float2 screenPos, in uint2 numTilesXY, in float2 rtSize, in float tileSizeRcp )
{
    const uint2 unclamped = (uint2)(screenPos * rtSize * tileSizeRcp);
    return clamp( unclamped, uint2(0, 0), numTilesXY - uint2(1,1) );
}

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
