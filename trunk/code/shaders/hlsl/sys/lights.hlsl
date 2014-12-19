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
float F_Schlick( float f0, float f90, float u )
{
    return f0 + (f90 - f0) * pow( 1.f - u, 5.f );
}

float Diffuse( float NdotV, float NdotL, float LdotH, float linearRough )
{
    float energyBias = lerp( 0, 0.5, linearRough );
    float energyFactor = lerp( 1.0, 1.f / 1.51f, linearRough );
    float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRough;
    float f0 = 1.0f;
    float lightScatter = F_Schlick( f0, fd90, NdotL );
    float viewScatter = F_Schlick( f0, fd90, NdotV );

    return (lightScatter * viewScatter * energyFactor);
}

float V_SmithGGXCorrelated( float NdotL, float NdotV, float alphaG )
{
    float alphaG2 = alphaG * alphaG;
    float lambda_GGXL = NdotL * sqrt( (-NdotV * alphaG2 + NdotV) * NdotV + alphaG2 );
    float lambda_GGXV = NdotV * sqrt( (-NdotL * alphaG2 + NdotL) * NdotL + alphaG2 );
    return 0.5f / (lambda_GGXL + lambda_GGXV);
}

float D_GGX( float NdotH, float m )
{
    float m2 = m*m;
    float f = (NdotH * m2 - NdotH) * NdotH + 1;
    return m2 / (f*f);
}

float2 BRDF( float3 L, float3 V, float3 N, float rough, float reflectance )
{
    float3 H = normalize( L + V );

    float NdotH = saturate( dot( N, H ) );
    float VdotH = saturate( dot( V, H ) );
    float NdotL = saturate( dot( N, L ) );
    float NdotV = abs( dot( N, V ) ) + 1e-5f;
    float LdotH = saturate( dot( L, H ) );

    float linearRough = sqrt( rough );

    float F = F_Schlick( reflectance, 1.0f, VdotH );
    float Vis = V_SmithGGXCorrelated( NdotV, NdotL, rough );
    float D = D_GGX( NdotH, rough );
    float Fr = D * F * Vis;

    float Fd = Diffuse( NdotV, NdotL, LdotH, linearRough );

    return float2( Fd, Fr ) * PI_RCP * NdotL;
}
#endif