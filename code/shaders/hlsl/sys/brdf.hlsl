#ifndef BRDF_HLSL
#define BRDF_HLSL

/*
struct Material
{
float3 diffuseColor;
float3 fresnelColor;

float diffuseCoeff;
float roughnessCoeff;
float specularCoeff;
float ambientCoeff;
};
*/

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

float3 BRDF( in float3 L, in float3 V, in float3 N, in Material mat )
{
    float3 H = normalize( L + V );
    float NdotH = saturate( dot( N, H ) );
    float NdotL_raw = (dot( N, L ));
    float NdotL = saturate( NdotL_raw );
    float NdotV = saturate( dot( N, V ) );
    float LdotH = saturate( dot( L, H ) );

    float m = mat.roughnessCoeff;
    float m2 = m * m;

    // Calculate the distribution term
    float d = m2 / (3.14159f * pow( NdotH * NdotH * (m2 - 1) + 1, 2.0f ));
    // Calculate the matching visibility term
    float v1i = GGX_V1( m2, NdotL );
    float v1o = GGX_V1( m2, NdotV );
    float vis = v1i * v1o;

    float3 fspec = Fresnel( mat.fresnelColor, LdotH );
    float f = Fresnel( (float3)mat.specularCoeff, LdotH ).x;

    float3 specular = d * vis * fspec * mat.specularCoeff;

    float ambientBase = -min( 0.f, NdotL_raw );
    float ambientFactor = (mat.ambientCoeff - (ambientBase * mat.ambientCoeff));
    
    float3 diffuse = saturate( 1.f - (f + ambientBase) ) * mat.diffuseColor * mat.diffuseCoeff * PI_RCP;
        float3 ambient = ambientFactor * mat.diffuseColor * PI_RCP;
    return ( diffuse+specular ) * NdotL + ambient;
}

float3 BRDF_diffuseOnly( in float3 L, in float3 V, in float3 N, in Material mat )
{
    float3 H = normalize( L + V );
    float NdotL_raw = ( dot( N, L ) );
    float NdotL = saturate( NdotL_raw );
    float LdotH = saturate( dot( L, H ) );
    
    float f = Fresnel( (float3)mat.specularCoeff, LdotH ).x;

    float ambientBase = -min( 0.f, NdotL_raw );
    float ambientFactor = (mat.ambientCoeff - (ambientBase * mat.ambientCoeff));

    float3 diffuse = saturate( 1.f - (f + ambientBase) ) * mat.diffuseColor * mat.diffuseCoeff * PI_RCP;
        float3 ambient = ambientFactor * mat.diffuseColor * PI_RCP;
    return ( diffuse * NdotL * PI_RCP ); + ambient;
}
float3 BRDF_specularOnly( in float3 L, in float3 V, in float3 N, in Material mat )
{
    float3 H = normalize( L + V );
    float NdotH = saturate( dot( N, H ) );
    float NdotL = saturate( dot( N, L ) );
    float NdotV = saturate( dot( N, V ) );
    float LdotH = saturate( dot( L, H ) );

    float m = mat.roughnessCoeff;;
    float m2 = m * m;

    // Calculate the distribution term
    float d = m2 / (3.14159f * pow( NdotH * NdotH * (m2 - 1) + 1, 2.0f ));
    // Calculate the matching visibility term
    float v1i = GGX_V1( m2, NdotL );
    float v1o = GGX_V1( m2, NdotV );
    float vis = v1i * v1o;

    float3 fspec = Fresnel( mat.fresnelColor, LdotH );
    float3 specular = d * vis * fspec * mat.specularCoeff;
    return specular * NdotL;
}
#endif