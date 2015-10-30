#ifndef BRDF_HLSL
#define BRDF_HLSL

//////////////////////////////////////////////////////////////////////////
// Mapping for similar size of specular reflection beteween GGX/Beckmann and Blinn NDF.
float roughnessToShininess( float roughness )
{
    return 2.0 / ( roughness*roughness ) - 1.999;
}

// Normal Distribution Functions (NDF)
// --

float D_GGX( float roughness, float NoH )
{
    float m2 = roughness*roughness;
    //float denom = NoH*NoH*(m2-1.0) + 1.0;	// sub mul mad
    float denom = NoH*( NoH*m2 - NoH ) + 1.0;	// 2 madds.
    return m2 / ( PI * denom * denom );
}

// Fresnel (returns the fraction of reflected light).
// --

// [An Inexpensive BRDF Model for Physically-based Rendering, Schlick]
float3 F_Schlick( float3 specularColor, float NoL )
{
    float powBase = 1.0 - NoL;
    float powBase2 = powBase*powBase;
    float powBase5 = powBase2*powBase2*powBase;
    return specularColor + ( 1.0 - specularColor )*powBase5; // TODO: Rewrite for less instructions.
}

// Visibilty functions (G/4*NoL*NoV)
// ---

// [Geometrical shadowing of a random rough surface, Smith]
float V_SmithGGX( float roughness, float NoL, float NoV )
{
    float a2 = roughness*roughness;
    float GL = NoL + sqrt( a2 + ( NoL - a2*NoL )*NoL );
    float GV = NoV + sqrt( a2 + ( NoV - a2*NoV )*NoV );
    return rcp( GL*GV );
}

// Approximation of SmithGGX.
// [An Inexpensive BRDF Model for Physically-based Rendering, Schlick]
float V_SchlickSmithGGX( float roughness, float NoL, float NoV )
{
    float a = roughness * 0.5;
    float oneMinusA = 1.0 - a;
    float GL = NoL*oneMinusA + a;
    float GV = NoV*oneMinusA + a;
    return 0.25 / ( GL*GV );
}

// Height-correlated SmithGGX (acounts for correlation between NoL and NoV).
// Brighter at grazing angles than Smith GGX.
// [Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs, Heitz]
float V_SmithHeitzGGX( float roughness, float NoL, float NoV )
{
    float a2 = roughness*roughness;
    float Lpart = NoV*sqrt( a2 + ( 1.0 - a2 )*NoL*NoL );
    float Vpart = NoL*sqrt( a2 + ( 1.0 - a2 )*NoV*NoV );
    return 0.5 / ( Lpart + Vpart );
}

// BRDFs
// ---

float3 diffuseLambertFresnel( float3 diffuseColor, float3 specularColor, float NoL )
{
    return diffuseColor * ( 1.0 - F_Schlick( specularColor, NoL ) ) * ( NoL * PI_RCP );
}

float3 specularTorranceSparrow( float3 specularColor, float roughness, float NoV, float NoL, float NoH, float VoH )
{
    float3 F = F_Schlick( specularColor, VoH );
    float  D = D_GGX( roughness, NoH );
    float  G = V_SchlickSmithGGX( roughness, NoL, NoV );

    return F*( D*G*NoL );
}

float3 specularTorranceSparrow( float3 specularColor, float roughness, float3 V, float3 N, float3 L )
{
    const float  NoV = saturate( dot( N, V ) );
    const float  NoL = saturate( dot( N, L ) );
    const float3 H = normalize( V + L );
    const float  NoH = saturate( dot( N, H ) );
    const float  VoH = saturate( dot( V, H ) );

    return specularTorranceSparrow( specularColor, roughness, NoV, NoL, NoH, VoH );
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

float3 computeSpecularBRDF( in float3 fresnelColor, in float specCoeff, in float roughness, in float NdotH, in float NdotL, in float NdotV, in float LdotH )
{
    float m2 = roughness * roughness;

    // Calculate the distribution term
    float a = NdotH * NdotH * (m2 - 1) + 1;
    float d = m2 / (PI * a*a);
    // Calculate the matching visibility term
    float v1i = GGX_V1( m2, NdotL );
    float v1o = GGX_V1( m2, NdotV );
    float vis = v1i * v1o;

    float3 fspec = F_Schlick( fresnelColor, LdotH );
    float3 specular = d * vis * fspec * specCoeff;
    return specular;
}

float3 computeDiffuseBRDF( in float3 specularColor, in float3 diffuseColor, in float LdotH )
{
    float f = F_Schlick( specularColor, LdotH ).x;
    float3 diffuse = saturate( 1.f - ( f ) ) * diffuseColor * PI_RCP;
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

    float3 specular = computeSpecularBRDF( mat.fresnelColor, mat.specularCoeff, mat.roughnessCoeff, NdotH, NdotL, NdotV, LdotH );
    float3 diffuse = computeDiffuseBRDF( mat.fresnelColor, mat.diffuseColor ,LdotH );
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
    
    float3 diffuse = computeDiffuseBRDF( mat.fresnelColor, mat.diffuseColor, LdotH );
    return diffuse * mat.diffuseCoeff * NdotL;
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

    float3 specular = computeSpecularBRDF( mat.fresnelColor, mat.specularCoeff, mat.roughnessCoeff, NdotH, NdotL, NdotV, LdotH );
    return specular * NdotL;
}
#endif