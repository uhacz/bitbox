passes:
{
    color =
    {
        vertex = "vs_main";
        pixel = "ps_main";
        hwstate =
        {
            depth_test = 1;
            depth_write = 0;
            //fill_mode = "WIREFRAME";
            //color_mask = "";
        };
    };
}; #~header

struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float3 pos	  	  : POSITION;
    float3 normal 	  : NORMAL;
};

struct in_PS
{
    float4 h_pos	: SV_Position;
    float4 s_pos    : TEXCOORD0;
    float3 w_pos	: TEXCOORD1;
    nointerpolation float3 w_normal : TEXCOORD2;
};

struct out_PS
{
    float4 rgba : SV_Target0;
    //float4 albedo : SV_Target1;
};

#include <sys/types.hlsl>
#include <sys/base.hlsl>
#include <sys/brdf.hlsl>
#include <sys/lights.hlsl>


in_PS vs_main( in_VS IN )
{
    in_PS OUT = (in_PS)0;

    float4 row0, row1, row2;
    float3 row0IT, row1IT, row2IT;

    fetchWorld( row0, row1, row2, IN.instanceID );
    fetchWorldIT( row0IT, row1IT, row2IT, IN.instanceID );

    float4 localPos = float4(IN.pos, 1.0);

    OUT.w_pos = transformPosition( row0, row1, row2, localPos );
    OUT.w_normal = transformNormal( row0IT, row1IT, row2IT, IN.normal );
    OUT.h_pos = mul( _camera_viewProj, float4(OUT.w_pos, 1.f) );
    OUT.s_pos = OUT.h_pos;

    return OUT;
}

shared cbuffer MaterialData: register(b3)
{
    MATERIAL_VARIABLES;
};

Texture2D _texSAO : register(t4);
Texture2D _texShadow : register(t5);
SamplerState _samplSAO : register(s4);


#define SUN_ANGULAR_DIAMETER  0.00471238898038468986
#define SUN_ANGULAR_DIAMETER_SIN 0.00471237153937342249
#define SUN_ANGULAR_DIAMETER_COS 0.9999888967155959894

float3 calculateSunDirectionSpecular( float3 sunDirection, float3 worldPos )
{
    float3 D = sunDirection;
    float3 R = normalize( normalize( worldPos ) - D );
    float r = SUN_ANGULAR_DIAMETER_SIN; // Disk radius
    float d = SUN_ANGULAR_DIAMETER_COS; // Distance to disk
    // Closest point to a disk ( since the radius is small , this is
    // a good approximation
    float DdotR = dot( D, R );
    float3 S = R - DdotR * D;
    float3 L = DdotR < d ? normalize( d * D + normalize( S ) * r ) : R;    return L;
}

out_PS ps_main( in_PS IN )
{
    out_PS OUT;
    const float2 screenPos01 = (IN.s_pos.xy / IN.s_pos.w) * 0.5 + 0.5;
    const float2 shadowUV = float2(screenPos01.x, 1.0 - screenPos01.y);
    
    const float3 N = normalize( IN.w_normal );
    const float3 V = -_camera_viewDir.xyz;
    const float shadow = _texShadow.SampleLevel( _samplSAO, shadowUV, 0.0 ).r;
    const float ssao = _texSAO.SampleLevel( _samplSAO, shadowUV, 0.0 ).r;

    const float3 L = -_sunDirection;
    const float3 Ls = -calculateSunDirectionSpecular( _sunDirection, IN.w_pos );
    const float3 H = normalize( Ls + V );

    const float NdotL_raw = dot( N, L );
    const float NdotL = saturate( NdotL_raw );
    const float NdotLs = saturate( dot( N, Ls ) );
    const float NdotV = saturate( dot( N, V ) );
    const float NdotH = saturate( dot( N, H ) );
    const float HdotLs = saturate( dot( H, Ls ) );

    float2 specular = computeSpecular( specularCoeff, roughnessCoeff, NdotH, NdotLs, NdotV, HdotLs );
    float diffuse = diffuseCoeff * (1.0 - specular.y);

    float3 direct;
    direct = diffuse * diffuseColor;
    direct += specular.x * fresnelColor * computeSpecOcclusion( NdotV, ssao, roughnessCoeff );
    direct *= _sunIlluminanceInLux* PI_RCP;
    direct *= NdotL;

    float NdotL_ambient = saturate( -NdotL_raw ) * ambientCoeff*0.25 + ambientCoeff;
    float3 ambient;
    ambient = NdotL_ambient * diffuseColor * ambientColor;
    ambient *= ambientCoeff * ssao;
    ambient *= _skyIlluminanceInLux;

    float3 c = lerp( ambient, direct, NdotL * shadow );

        OUT.rgba = float4(c, 1.0);
    return OUT;
}