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

#ifdef USE_SKYBOX
textureCUBE<float3> skybox              : register( t4 );
#endif

#define PI	   (3.14159265f)
#define PI_RCP (0.31830988618379067154f)

float GGX_V1( in float m2, in float nDotX )
{
    return 1.0f / ( nDotX + sqrt( m2 + ( 1 - m2 ) * nDotX * nDotX ) );
}

float Fresnel( in float f0, in float VdotH )
{
    return f0 + ( 1.0 - f0 ) * exp2( ( -5.55473 * VdotH - 6.98316 ) * VdotH );
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
float3 F_Schlick(float3 F0, float NoL)
{
    float powBase = 1.0 - NoL;
    float powBase2 = powBase*powBase;
    float powBase5 = powBase2*powBase2*powBase;
    return F0 + (1.0 - F0)*powBase5; // TODO: Rewrite for less instructions.
}
float3 ps_lighting(in_PS IN) : SV_Target0
{
    //float2 render_target_size = float2( 1920, 1080 );
    int3 positionSS = int3( ( int2 )( IN.uv * render_target_size ), 0 );
    
    float4 albedo_spec = gbuffer_albedo_spec.Load( positionSS );
    float4 wpos_rough = gbuffer_wpos_rough.Load( positionSS );
    float4 wnrm_metal = gbuffer_wnrm_metal.Load( positionSS );
    float depthCS = depthTexture.Load( positionSS );

    float4 positionCS = float4( ( ( float2( positionSS.xy ) + 0.5 ) * render_target_size_rcp ) * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 ), depthCS, 1.0 );
    float4 positionWS = mul( view_proj_inv, positionCS );
    positionWS.xyz *= rcp( positionWS.w );

    //const float3 positionWS = wpos_rough.xyz;
    
    float metalness = wnrm_metal.w;
    float3 diffuseColor = albedo_spec.rgb - albedo_spec.rgb*wnrm_metal.w;

    const float3 N = wnrm_metal.xyz;
    const float3 V = normalize( camera_eye - positionWS.xyz );
    const float3 L = sun_L;
    const float3 sunL = SunSpecularL(positionWS.xyz, N, L);
    const float3 H = normalize( L+V );
    
#ifdef USE_SKYBOX
    if( depthCS == 1.0 )
    {
        return float4( skybox.SampleLevel( _samp_linear, -V, 0.0 ), 1.0 ) * sky_intensity * PI_RCP;
    }
#endif

    const float NdotH = saturate( dot( N, H ) );
    const float NdotL = saturate( dot( N, L ) );
    const float NdotSunL = saturate( dot( N, sunL ) );
    const float NdotV = saturate( dot( N, V ) );
    const float VdotH = saturate( dot( V, H ) );
    const float HdotSunL = saturate( dot( H, sunL ) );

    const float3 R = 2.0*NdotV*N - V;

    float m = wpos_rough.w;
    float m2 = m * m;

    // Calculate the distribution term
    float d = m2 / ( PI * pow( NdotH * NdotH * ( m2 - 1 ) + 1, 2.0f ) );
    // Calculate the matching visibility term
    float v1i = GGX_V1( m2, NdotSunL );
    float v1o = GGX_V1( m2, NdotV );
    float vis = v1i * v1o;

    // Calculate the Fresnel term
    float spec = albedo_spec.w;
    float3 f0 = lerp(0.04, albedo_spec.rgb, metalness);
    //float f0 = 0.16 * spec*spec;
    float f = F_Schlick( f0, HdotSunL );

    // Put it all together
    float specular = d * vis * f;

    float3 diffuse = diffuseColor * ( 1.0f - f );
    float3 color = ( specular.xxx + diffuse ) * NdotL * sun_intensity * PI_RCP;

#ifdef USE_SKYBOX
    float environmentMapWidth = 1024;
    float glossyExponent = roughnessToShininess( m );
    float MIPlevel = log2( environmentMapWidth * sqrt( 3 ) ) - 0.5 * log2( glossyExponent + 1 );    
    float3 diffuse_env = skybox.SampleLevel( _samp_bilinear, N, 10.0 ).rgb;
    float3 specular_env = skybox.SampleLevel( _samp_bilinear, R, MIPlevel ).rgb;

    color += ( diffuse * diffuse_env + albedo_spec.w * specular_env ) * sky_intensity * PI_RCP;
#else
    float ambientCoeff = 0.015f;
    float NdotL_ambient = saturate( -dot( N, -L ) ) * ambientCoeff * 0.1 + ambientCoeff;
    float3 ambient = NdotL_ambient * albedo_spec.rgb * PI_RCP * sky_intensity;
    color += ambient;
#endif
    //float3 envColor = diffuse_env + specular_env; // skybox.Sample( _samp_point, N ).rgb;
    //color += envColor;
    
    return float4( color, 1.0 );
}