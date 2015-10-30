#ifndef LIGHTS_HLSL
#define LIGHTS_HLSL

uint2 computeTileXY( in float2 screenPos, in uint2 numTilesXY, in float2 rtSize, in float tileSizeRcp )
{
    const uint2 unclamped = (uint2)(screenPos * rtSize * tileSizeRcp);
    return clamp( unclamped, uint2(0, 0), numTilesXY - uint2(1,1) );
}

void evaluateSunLight( out float3 lcol, out float illuminance, in ShadingData shd, float3 surfPos, in Material mat )
{
    const float3 N = shd.N;
    const float3 D = -_sunDirection;
    const float r = sin( _sunAngularRadius );
    const float d = cos( _sunAngularRadius );
    const float3 R = normalize( D - normalize( surfPos ) );
    const float DdotR = dot( D, R );
    const float3 S = R - DdotR * D;    const float3 L = DdotR < d ? normalize( d * D + normalize( S ) * r ) : R;

    //float w = 1.0; // mat.ambientCoeff;
    //float n = 1.f;
    //illuminance = _sunIlluminanceInLux * wrappedLambert( saturate( dot( N, D ) ), w, n );
    illuminance = _sunIlluminanceInLux * saturate( dot( N, D ) );
    const float3 Fd = BRDF_diffuseOnly( D, shd, mat );
    const float3 Fr = BRDF_specularOnly( L, shd, mat );

    //const float NoL_diff = saturate( dot( shd.N, D ) );
    //const float NoL_spec = saturate( dot( shd.N, L ) );
    //const float  NoV = saturate( dot( shd.N, shd.V ) );
    //const float3 H = normalize( shd.V + L );
    //const float  NoH = saturate( dot( shd.N, H ) );
    //const float  VoH = saturate( dot( shd.V, H ) );
    //
    //const float3 Fd = diffuseLambertFresnel( mat.diffuseColor, mat.fresnelColor, NoL_diff );
    //const float3 Fr = specularTorranceSparrow( mat.fresnelColor, mat.roughnessCoeff, NoV, NoL_spec, NoH, VoH );
    
    lcol = ( Fd + Fr ) * shd.shadow;
    //return Fd * illuminance;
}

void evaluateAmbientLight( out float3 ambient, out float illuminance, in ShadingData shd, in Material mat )
{
    float aNdotL = saturate( -( clamp( dot( shd.N, -_sunDirection ), -mat.ambientCoeff * 0.5f, -mat.ambientCoeff ) ) );
    ambient = aNdotL * mat.diffuseColor * mat.ambientColor;
    //ambient = ( ( 1.f - ambient ) * ambient );
    ambient *= mat.ambientCoeff * shd.ssao;
    illuminance = _skyIlluminanceInLux;
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
