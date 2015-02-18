#ifndef BRDF_HLSL
#define BRDF_HLSL

struct ShadingData
{
    float3 V;
    float3 N;
};

////
////
float3 Fresnel( float3 f0, float u )
{
    return f0 + (1.f - f0) * pow( 1.f - u, 5.f );
}


// -- helper for computing the GGX visibility term
float GGX_V1( in float m2, in float nDotX )
{
    return 1.0f / (nDotX + sqrt( m2 + (1 - m2) * nDotX * nDotX ));
}

float wrappedLambert( float NdotL, float w, float n )
{
    return pow( saturate( (NdotL + w) / (1.0f + w) ), n ) * (n + 1) / (2 * (1 + w));
}

float3 computeSpecularBRDF( in float NdotH, in float NdotL, in float NdotV, in float LdotH, in Material mat )
{
    float m = mat.roughnessCoeff;
    float m2 = m * m;

    // Calculate the distribution term
    float a = NdotH * NdotH * (m2 - 1) + 1;
    float d = m2 / (PI * a*a);
    // Calculate the matching visibility term
    float v1i = GGX_V1( m2, NdotL );
    float v1o = GGX_V1( m2, NdotV );
    float vis = v1i * v1o;

    float3 fspec = Fresnel( mat.fresnelColor, LdotH );

    float3 specular = d * vis * fspec * mat.specularCoeff;
    
    return specular;
}

float3 computeDiffuseBRDF( in float LdotH, in Material mat )
{
    float f = Fresnel( (float3)mat.specularCoeff, LdotH ).x;
    float3 diffuse = saturate( 1.f - (f) ) * mat.diffuseColor * mat.diffuseCoeff * PI_RCP;
    
    return diffuse;
}

float3 BRDF( in float3 L, in ShadingData shd, in Material mat )
{
    float3 N = shd.N;
    float3 V = shd.V;
    float3 H = normalize( L + V );
    float NdotH = saturate( dot( N, H ) );
    float NdotL_raw = (dot( N, L ));
    float NdotL = saturate( NdotL_raw );
    float NdotV = saturate( dot( N, V ) );
    float LdotH = saturate( dot( L, H ) );

    float3 specular = computeSpecularBRDF( NdotH, NdotL, NdotV, LdotH, mat );
    float3 diffuse = computeDiffuseBRDF( LdotH, mat );
    float a = NdotL_raw * 0.5f + 0.5f;
    diffuse = lerp( diffuse*mat.ambientCoeff, diffuse, a );
    return ( diffuse + specular ) * NdotL;
}

float3 BRDF_diffuseOnly( in float3 L, in ShadingData shd, in Material mat )
{
    float3 N = shd.N;
    float3 V = shd.V;
    float3 H = normalize( L + V );
    float NdotL_raw = ( dot( N, L ) );
    float NdotL = saturate( NdotL_raw );
    float LdotH = saturate( dot( L, H ) );
    
    float3 diffuse = computeDiffuseBRDF( LdotH, mat );
    float a = NdotL_raw * 0.5f + 0.5f;
    return lerp( diffuse*mat.ambientCoeff, diffuse, a );// wrappedLambert( NdotL, w, n );
}

float3 BRDF_specularOnly( in float3 L, in ShadingData shd, in Material mat )
{
    float3 N = shd.N;
    float3 V = shd.V;
    float3 H = normalize( L + V );
    float NdotH = saturate( dot( N, H ) );
    float NdotL = saturate( dot( N, L ) );
    float NdotV = saturate( dot( N, V ) );
    float LdotH = saturate( dot( L, H ) );

    float3 specular = computeSpecularBRDF( NdotH, NdotL, NdotV, LdotH, mat );
    return specular * NdotL;
}
#endif