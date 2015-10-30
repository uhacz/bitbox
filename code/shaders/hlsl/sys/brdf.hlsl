#ifndef BRDF_HLSL
#define BRDF_HLSL

float3 F_Schlick( float3 specularColor, float NoL )
{
    float powBase = 1.0 - NoL;
    float powBase2 = powBase*powBase;
    float powBase5 = powBase2*powBase2*powBase;
    return specularColor + ( 1.0 - specularColor )*powBase5; // TODO: Rewrite for less instructions.
}

float F_Schlick1( float f0, float d )
{
    float powBase = 1.0 - d;
    float powBase2 = powBase*powBase;
    float powBase5 = powBase2*powBase2*powBase;
    return f0 + (1.0 - f0)*powBase5; // TODO: Rewrite for less instructions.
}

// -- helper for computing the GGX visibility term
float GGX_V1( in float m2, in float nDotX )
{
    return 1.0f / (nDotX + sqrt( m2 + (1 - m2) * nDotX * nDotX ));
}

float computeDiffuse( in float diffuseCoeff, in float fresnel )
{
    return diffuseCoeff * (1.0 - fresnel);
}

float2 computeSpecular( in float specularCoeff, in float roughness, in float NdotH, in float NdotL, in float NdotV, in float HdotL )
{
    float m2 = roughness * roughness;
    // Calculate the distribution term
    float a = NdotH * NdotH * (m2 - 1) + 1;
    float d = m2 / (PI * a*a);
    // Calculate the matching visibility term
    float v1i = GGX_V1( m2, NdotL );
    float v1o = GGX_V1( m2, NdotV );
    float vis = v1i * v1o;

    // Calculate the Fresnel term
    float f = F_Schlick1( specularCoeff, HdotL );
    
    // Put it all together
    float specular = d * vis * f;
    return float2( specular, f );
}
float computeSpecOcclusion( float NdotV, float AO, float roughness )
{
    return saturate( pow( NdotV + AO, roughness ) - 1 + AO );
}

#endif