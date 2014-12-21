#ifndef BRDF_HLSL
#define BRDF_HLSL

////
////
float F_Schlick( float f0, float f90, float u )
{
    return f0 + (f90 - f0) * pow( 1.f - u, 5.f );
}

float Diffuse( float NdotV, float NdotL, float LdotH, float linearRough )
{
    //float energyBias = lerp( 0, 0.5, linearRough );
    //float energyFactor = lerp( 1.0, 1.f / 1.51f, linearRough );
    float fd90 = /*energyBias + */2.0 * LdotH * LdotH * linearRough;
    float f0 = 1.0f;
    float lightScatter = F_Schlick( f0, fd90, NdotL );
    float viewScatter = F_Schlick( f0, fd90, NdotV );

    return (lightScatter * viewScatter /** energyFactor*/);
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

float Specular( float VdotH, float NdotV, float NdotL, float NdotH, float rough, float reflectance )
{
    float F = F_Schlick( reflectance, 1.0f, VdotH );
    float Vis = V_SmithGGXCorrelated( NdotV, NdotL, rough );
    float D = D_GGX( NdotH, rough );
    return D * F * Vis;
}

float2 BRDF( in float3 L, in float3 V, in float3 N, in float rough, in float reflectance )
{
    float3 H = normalize( L + V );

    float NdotH = saturate( dot( N, H ) );
    float VdotH = saturate( dot( V, H ) );
    float NdotL = saturate( dot( N, L ) );
    float NdotV = abs( dot( N, V ) ) + 1e-5f;
    float LdotH = saturate( dot( L, H ) );
        
    float Fr = Specular( VdotH, NdotV, NdotL, NdotH, rough, reflectance  );
    
    float linearRough = sqrt( rough );
    float Fd = Diffuse( NdotV, NdotL, LdotH, linearRough );

    return float2( Fd, Fr ) * PI_RCP * NdotL;
}

float BRDF_Diffuse( in float3 L, in float3 V, in float3 N, in float rough )
{
    float3 H = normalize( L + V );

    float NdotL = saturate( dot( N, L ) );
    float NdotV = abs( dot( N, V ) ) + 1e-5f;
    float LdotH = saturate( dot( L, H ) );
    
    float linearRough = sqrt( rough );
    float Fd = Diffuse( NdotV, NdotL, LdotH, linearRough );

    return Fd * PI_RCP * NdotL;
}

float BRDF_Specular( in float3 L, in float3 V, in float3 N, in float rough, in float reflectance )
{
    float3 H = normalize( L + V );

    float NdotH = saturate( dot( N, H ) );
    float VdotH = saturate( dot( V, H ) );
    float NdotL = saturate( dot( N, L ) );
    float NdotV = abs( dot( N, V ) ) + 1e-5f;
    
    float Fr = Specular( VdotH, NdotV, NdotL, NdotH, rough, reflectance );

    return Fr * PI_RCP * NdotL;
}

#endif