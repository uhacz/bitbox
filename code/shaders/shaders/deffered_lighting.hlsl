passes:
{
    lighting = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_lighting";
    };

}; #~header

#include <sys/samplers.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/binding_map.h>

#define in_PS out_VS_screenquad

texture2D gbuffer_albedo_spec : register(t0);
texture2D gbuffer_wpos_rough : register(t1);
texture2D gbuffer_wnrm_metal : register(t2);

cbuffer FrameData : register(BSLOT( SLOT_FRAME_DATA ) )
{
    float3 cameraEye;
    float3 cameraDir;
    float3 sunColor;
    float sunIntensity;
    float3 sunL; // L means direction TO light
};
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
    float3 surfaceToCamera = normalize( cameraEye - wpos );
    float3 r = reflect( surfaceToCamera, N );
    float3 D = dot( L, r ) * r - L;
    float3 P = L + D * saturate( radius_over_distance * rsqrt( dot( D, D ) ) );
    float3 specular_l = normalize( P );
    return specular_l;
}

float3 ps_lighting(in_PS IN) : SV_Target0
{
    
    float4 albedo_spec = gbuffer_albedo_spec.SampleLevel(_samp_point, IN.uv, 0.0);
    float4 wpos_rough = gbuffer_wpos_rough.SampleLevel(_samp_point, IN.uv, 0.0);
    
    const float3 N = gbuffer_wnrm_metal.SampleLevel( _samp_point, IN.uv, 0.0).rgb;
    const float3 V = normalize( cameraEye - wpos_rough.xyz );
    const float3 L = sunL;
    const float3 sunL = SunSpecularL( wpos_rough.xyz, N, L );
    const float3 H = normalize( L+V );

    const float NdotH = saturate( dot( N, H ) );
    const float NdotL = saturate( dot( N, L ) );
    const float NdotSunL = saturate( dot( N, sunL ) );
    const float NdotV = saturate( dot( N, V ) );
    const float VdotH = saturate( dot( V, H ) );
    const float HdotSunL = saturate( dot( H, sunL ) );

    float m = wpos_rough.w;
    float m2 = m * m;

    // Calculate the distribution term
    float d = m2 / ( PI * pow( NdotH * NdotH * ( m2 - 1 ) + 1, 2.0f ) );
    // Calculate the matching visibility term
    float v1i = GGX_V1( m2, NdotSunL );
    float v1o = GGX_V1( m2, NdotV );
    float vis = v1i * v1o;

    // Calculate the Fresnel term
    float f = Fresnel( albedo_spec.w, HdotSunL );

    // Put it all together
    float specular = d * vis * f;

    float3 diffuse = albedo_spec.rgb * ( 1.0f - f );
    float3 color = ( specular.xxx + diffuse * PI_RCP ) * NdotL;

    float ambientCoeff = 0.015f;
    float NdotL_ambient = saturate( -dot( N, -L ) ) * ambientCoeff * 0.1 + ambientCoeff;
    float3 ambient = NdotL_ambient * albedo_spec.rgb * PI_RCP;
    
    color += ambient;
    
    return float4( color, 1.0 );
}