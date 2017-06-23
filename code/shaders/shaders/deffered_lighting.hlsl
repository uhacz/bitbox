passes:
{
    lighting = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_lighting";
    };
    lighting_skybox = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_lighting";
        define = 
        {
            USE_SKYBOX = 1;
        };
    };

}; #~header

#include <sys/samplers.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/binding_map.h>
#include <deffered_lighting_data.h>

#define in_PS out_VS_screenquad

Texture2D<float>    depthTexture        : register( t0 );
texture2D<float4>   gbuffer_albedo_spec : register( t1 );
texture2D<float4>   gbuffer_wpos_rough  : register( t2 );
texture2D<float4>   gbuffer_wnrm_metal  : register( t3 );
texture2D <float>   shadowMap           : register( t4 );
texture2D <float2>  ssaoMap             : register( t5 );

#ifdef USE_SKYBOX
textureCUBE<float3> skybox              : register( t6 );
#endif

#define PI	   (3.14159265f)
#define PI_RCP (0.31830988618379067154f)

float GGX_V1( in float m2, in float nDotX )
{
    return 1.0f / ( nDotX + sqrt( m2 + ( 1 - m2 ) * nDotX * nDotX ) );
}

float3 Fresnel( in float3 f0, in float VdotH )
{
    return f0 + ( 1.0 - f0 ) * exp2( ( -5.55473 * VdotH - 6.98316 ) * VdotH );

    //float powBase = 1.0 - NoL;
    //float powBase2 = powBase*powBase;
    //float powBase5 = powBase2*powBase2*powBase;
    //return F0 + (1.0 - F0)*powBase5; // TODO: Rewrite for less instructions.
}

float3 SunSpecularL( in float3 wpos, in float3 N, in float3 L )
{
    const float radius_over_distance = 0.00465;
    float3 surfaceToCamera = normalize( camera_eye - wpos );
    float3 r = reflect( surfaceToCamera, N );
    float3 D = dot( L, r ) * r - L;
    float3 P = L + D * saturate( radius_over_distance * rsqrt( dot( D, D ) ) );
    float3 specular_l = normalize( P );
    return specular_l;
}

float roughnessToShininess( float roughness )
{
    return 2.0 / ( roughness*roughness ) - 1.999;
}

float3 ComputeDiffuse( float3 baseColor, float3 f0, float NdotL )
{
    return baseColor * ( 1.0f - Fresnel( f0, NdotL ) ) * NdotL * PI_RCP;
}
float3 ComputeSpecular( float3 baseColor, float3 f0, float roughness, float HdotL, float NdotH, float NdotV, float NdotL )
{
    float m = roughness;
    float m2 = m * m;
       
    float3 F = Fresnel( f0, HdotL );

    float denom = NdotH*( NdotH*m2 - NdotH ) + 1.0;	// 2 madds.
    float D = m2 / ( PI * denom * denom );

    float l_part = NdotV*sqrt( m2 + ( 1.0 - m2 )*NdotL*NdotL );
    float v_part = NdotL*sqrt( m2 + ( 1.0 - m2 )*NdotV*NdotV );
    float G = 0.5 / max( l_part + v_part, 10e-7 );

    return F * ( D*G*NdotL );
}

float3 ps_lighting(in_PS IN) : SV_Target0
{
    int3 positionSS = int3( ( int2 )( IN.uv * render_target_size ), 0 );
    
    float4 albedo_spec = gbuffer_albedo_spec.Load( positionSS );
    float4 wpos_rough  = gbuffer_wpos_rough.Load( positionSS );
    float4 wnrm_metal  = gbuffer_wnrm_metal.Load( positionSS );
    float  depthCS     = depthTexture.Load( positionSS );
    float shadow       = shadowMap.SampleLevel( _samp_bilinear, IN.uv, 0 );
    float ssao         = ssaoMap.SampleLevel( _samp_point, IN.uv, 0 ).r;
    ssao = ssao*0.5 + 0.5;


    float metalness  = wnrm_metal.w;
    float roughness  = wpos_rough.w;
    float reflectivness = albedo_spec.w;
    float3 baseColor = albedo_spec.rgb - albedo_spec.rgb*wnrm_metal.w;
    float3 f0 = lerp( reflectivness.xxx, baseColor, metalness );

    float4 positionCS = float4( ( ( float2( positionSS.xy ) + 0.5 ) * render_target_size_rcp ) * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 ), depthCS, 1.0 );
    float4 positionWS = mul( view_proj_inv, positionCS );
    positionWS.xyz   *= rcp( positionWS.w );

    const float3 V = normalize( camera_eye.xyz - positionWS.xyz );
    const float3 N = normalize( wnrm_metal.xyz );
    const float3 L = sun_L;
    //const float3 sunL = SunSpecularL(positionWS.xyz, N, L);
    const float3 H = normalize( L + V );
    
#ifdef USE_SKYBOX
    if( depthCS == 1.0 )
    {
        return float4( skybox.SampleLevel( _samp_bilinear, -V, 0.0 ), 1.0 ) * sky_intensity;
    }
#endif

    const float NdotH = saturate( dot( N, H ) );
    const float NdotL = saturate( dot( N, L ) );
    //const float NdotSunL = saturate( dot( N, sunL ) );
    const float NdotV = saturate( dot( N, V ) );
    const float VdotH = saturate( dot( V, H ) );
    const float HdotL = saturate( dot( H, L ) );
    //const float HdotSunL = saturate( dot( H, sunL ) );

    const float3 R = 2.0*NdotV*N - V;
        
    float3 diffuse  = ComputeDiffuse( baseColor, f0, NdotL );
    float3 specular = ComputeSpecular( baseColor, f0, roughness, HdotL, NdotH, NdotV, NdotL );
    
    float3 color = sun_color * ( specular + diffuse ) * sun_intensity; // *shadow;
    
#ifdef USE_SKYBOX
    //float glossyExponent = roughnessToShininess( roughness );
    float a2 = roughness * roughness;
    float specPower = ( 1.0 / a2 - 1.0 ) * 2.0;
    //float specGloss = log2( specPower ) / 19.0;

    float MIPlevel = log2( environment_map_width * sqrt( 3 ) ) - 0.5 * log2( specPower + 1 );    
    float3 diffuse_env = skybox.SampleLevel( _samp_bilinear, N, environment_map_max_mip ).rgb;
    float3 specular_env = skybox.SampleLevel( _samp_bilinear, R, MIPlevel ).rgb;
    float3 color_from_sky_diff = ( diffuse * diffuse_env );
    float3 color_from_sky_spec = ( specular_env * Fresnel( f0, NdotV ) );
    float3 color_from_sky = ( color_from_sky_diff + color_from_sky_spec ) * sky_intensity * PI_RCP;
    color += color_from_sky;
#else
    float ambientCoeff = 0.015f;
    float NdotL_ambient = saturate( -dot( N, -L ) ) * ambientCoeff * 0.1 + ambientCoeff;
    float3 ambient = NdotL_ambient * albedo_spec.rgb * PI_RCP * sky_intensity;
#endif

    color = lerp( color*0.5f, color, shadow );
    color = lerp( color*ssao, color, shadow );
    return float4( color, 1.0 );
}