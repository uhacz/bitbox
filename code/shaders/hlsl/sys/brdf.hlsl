#ifndef BRDF_HLSL
#define BRDF_HLSL

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
    float d = m2 / (3.14159f * pow( NdotH * NdotH * (m2 - 1) + 1, 2.0f ));
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

float3 computeAmbientBRDF( in float NdotL, in Material mat )
{
    float ambientBase = -NdotL;
    float ambientFactor = (ambientBase * mat.ambientCoeff);
    float3 ambient = ambientFactor * mat.diffuseColor;
    return ambient;
}

float3 BRDF( in float3 L, in float3 V, in float3 N, in Material mat )
{
    float3 H = normalize( L + V );
    float NdotH = saturate( dot( N, H ) );
    float NdotL_raw = (dot( N, L ));
    float NdotL = saturate( NdotL_raw );
    float NdotV = saturate( dot( N, V ) );
    float LdotH = saturate( dot( L, H ) );

    float3 specular = computeSpecularBRDF( NdotH, NdotL, NdotV, LdotH, mat );
    float3 diffuse = computeDiffuseBRDF( LdotH, mat );
    float3 ambient = float3( 0.f, 0.f, 0.f );
    
    //if( NdotL_raw <= 0.f )
        //ambient = computeAmbientBRDF( NdotL_raw, mat );

    return ( diffuse + specular ) * NdotL;
}

float3 BRDF_diffuseOnly( in float3 L, in float3 V, in float3 N, in Material mat )
{
    float3 H = normalize( L + V );
    float NdotL_raw = ( dot( N, L ) );
    float NdotL = saturate( NdotL_raw );
    float LdotH = saturate( dot( L, H ) );
    
    float3 diffuse = computeDiffuseBRDF( LdotH, mat );
    float3 ambient = float3( 0.f, 0.f, 0.f );
    
    //if( NdotL_raw <= 0.f )
        //ambient = computeAmbientBRDF( NdotL_raw, mat );

    float w = mat.ambientCoeff;
    float n = 1.f;
    
    return (diffuse) * wrappedLambert( NdotL, w, n ); // *dif + ambient;
    //return ambient;
}

float3 BRDF_specularOnly( in float3 L, in float3 V, in float3 N, in Material mat )
{
    float3 H = normalize( L + V );
    float NdotH = saturate( dot( N, H ) );
    float NdotL = saturate( dot( N, L ) );
    float NdotV = saturate( dot( N, V ) );
    float LdotH = saturate( dot( L, H ) );

    float3 specular = computeSpecularBRDF( NdotH, NdotL, NdotV, LdotH, mat );
    return specular * NdotL;
}
#endif