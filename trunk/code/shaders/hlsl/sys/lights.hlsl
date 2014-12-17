#ifndef LIGHTS
#define LIGHTS

shared cbuffer LighningData : register(b2)
{
    uint2 _numTilesXY;
    uint  _numTiles;
    uint  _tileSize;
    uint  _maxLights;
    float _tileSizeRcp;
};

Buffer<float4> _lightsData    : register(t0);
Buffer<uint>   _lightsIndices : register(t1);

////
////
uint2 computeTileXY( in float2 screenPos, in uint2 numTilesXY, in float2 rtSize, in float tileSizeRcp )
{
    const uint2 unclamped = (uint2)(screenPos * rtSize * tileSizeRcp);
    return clamp( unclamped, uint2(0, 0), numTilesXY - uint2(1,1) );
}

////
////
void pointLight_computeParams( out float3 L, out float attenuation, in float3 lightPos, in float lightRad, in float3 surfPos )
{
    float3 surfToLight = lightPos - surfPos;
    float dist = length( surfToLight );
    L = surfToLight * rcp( dist );

    float denom = saturate( dist / lightRad ) - 1.f;
    attenuation = (denom * denom);
}

////
////
float Fresnel( in float f0, in float f90, float u )
{
    return f0 + (f90 - f0) * pow( 1 - u, 5.f );
}

float Fr_DisneyDiffuse( float NdotV, float NdotL, float LdotH, float linearRoughness )
{
    float energyBias = lerp( 0, 0.5, linearRoughness );
    float energyFactor = lerp( 1.0, 1.0 / 1.51, linearRoughness );
    float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness;
    float f0 = 1.f;
    float lightScatter = Fresnel( f0, fd90, NdotL );
    float viewScatter = Fresnel( f0, fd90, NdotV );
    return lightScatter * viewScatter * energyFactor;
}float V_SmithGGXCorrelated( float NdotL, float NdotV, float alphaG )
{
    // Original formulation of G_SmithGGX Correlated
    // lambda_v = ( -1 + sqrt ( alphaG2 * (1 - NdotL2 ) / NdotL2 + 1)) * 0.5 f;
    // lambda_l = ( -1 + sqrt ( alphaG2 * (1 - NdotV2 ) / NdotV2 + 1)) * 0.5 f;
    // G_SmithGGXCorrelated = 1 / (1 + lambda_v + lambda_l );
    // V_SmithGGXCorrelated = G_SmithGGXCorrelated / (4.0 f * NdotL * NdotV );

    // This is the optimize version
    float alphaG2 = alphaG * alphaG;
    // Caution : the " NdotL *" and " NdotV *" are explicitely inversed , this is not a mistake .
    float Lambda_GGXV = NdotL * sqrt( (-NdotV * alphaG2 + NdotV) * NdotV + alphaG2 );
    float Lambda_GGXL = NdotV * sqrt( (-NdotL * alphaG2 + NdotL) * NdotL + alphaG2 );

    return 0.5f / (Lambda_GGXV + Lambda_GGXL);
}

float D_GGX( float NdotH, float m )
{
    // Divide by PI is apply later
    float m2 = m * m;
    float f = (NdotH * m2 - NdotH) * NdotH + 1;
    return m2 / (f * f);
}

float2 BRDF( in float3 L, in float3 V, in float3 N, in float f0, in float linearRoughness )
{
    float NdotV = abs( dot( N, V ) ) + 1e-5f; // avoid artifact
    float3 H = normalize( V + L );
    float LdotH = saturate( dot( L, H ) );
    float NdotH = saturate( dot( N, H ) );
    float NdotL = saturate( dot( N, L ) );
    
    float diffuse = NdotL;
    float specular = pow( NdotH, 64 * f0 );

    return float2(diffuse, specular);
    
    //
    //// Specular BRDF
    //float roughness = linearRoughness * linearRoughness;
    //float F = Fresnel( f0, 1.f, LdotH );
    //float Vis = V_SmithGGXCorrelated( NdotV, NdotL, roughness );
    //float D = D_GGX( NdotH, roughness );
    //float denom = 1.f / 4.0f * NdotL*NdotV;
    //float Fr = D * F * Vis * denom / PI;
    //
    //// Diffuse BRDF
    //float Fd = Fr_DisneyDiffuse( NdotV, NdotL, LdotH, linearRoughness ) / PI;

    //return float2(Fd, Fr);
}
#endif