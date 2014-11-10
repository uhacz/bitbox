passes:
{
    simple =
    {
        vertex = "vs_main";
        pixel = "ps_main";
		define = 
		{
			DIFFUSE_TEX = "1";
		};
        hwstate = 
		{
			depth_test = 1;
			depth_write = 0;
		};
    };
	simple_notex =
    {
        vertex = "vs_main";
        pixel = "ps_main";
        
        hwstate = 
		{
			depth_test = 1;
			depth_write = 0;
		};
    };
};#~header

#include <sys/frame_data.hlsl>
#include <sys/instance_data.hlsl>
#include <sys/shading_data.hlsl>
#include <sys/types.hlsl>

shared cbuffer MaterialData : register(b3)
{
	float3 diffuse_color;
	float fresnel_coeff;
	float rough_coeff;
};

#ifdef DIFFUSE_TEX
Texture2D diffuse_tex;
SamplerState diffuse_sampl;
#endif

struct in_VS
{
	uint   instanceID : SV_InstanceID;
	float4 pos	  	  : POSITION;
	float3 normal 	  : NORMAL;
#ifdef DIFFUSE_TEX
	float2 uv	  	  :	TEXCOORD0;
#endif
};

struct in_PS
{
    float4 h_pos	: SV_Position;
    float2 winpos   : TEXCOORD0;
	float3 w_pos	: TEXCOORD1;
	nointerpolation float3 w_normal:TEXCOORD2;
#ifdef DIFFUSE_TEX
	float2 uv		: TEXCOORD3;
#endif
};
struct out_PS
{
	float4 rgba : SV_Target0;
};

float4 mul_as_float3( in float4x4 m, in float4 v )
{
    return v.x*m[0] + ( v.y*m[1] + ( v.z*m[2] + m[3] ) );
}
in_PS vs_main( in_VS input )
{
    in_PS output;
	matrix wm = world_matrix[input.instanceID];
	matrix wmit = world_it_matrix[input.instanceID];
	float4 world_pos = mul( wm, input.pos );
    world_pos.w = 1.f;
    
    float4 hpos = mul( view_proj_matrix, world_pos );
    
    output.h_pos = hpos;
	output.w_pos = world_pos.xyz;
    output.w_normal = mul( (float3x3)wmit, input.normal );
    output.winpos = hpos.xy;
#ifdef DIFFUSE_TEX
	output.uv = input.uv;
#endif
    return output;
}

Texture2D<float2> _tex_shadow_ssao : register( t0 );
SamplerState _samp_shadow_ssao : register ( s0 );

#ifdef DIFFUSE_TEX
float4 evaluate_diffuse_tex( float2 uv )
{
	return diffuse_tex.Sample( diffuse_sampl, uv );
}
#endif

/////////////////////////////////////////////////////////////////
float sqr(float x) { return x*x; }

float Beckmann(float m, float t)
{
    float M = m*m;
    float T = t*t;
    return exp((T-1)/(M*T)) / (M*T*T);
}

float Fresnel(float f0, float u)
{
    // from Schlick
    return f0 + (1-f0) * pow(1-u, 5);
}

float D_Exponential(float c, float t)
{
    return exp(-t/c);
}

float G1_GGX(float Ndotv, float alphaG)
{
    const float NdotV2 = Ndotv*Ndotv;
    return 2/(1 + sqrt(1 + alphaG*alphaG * (1-NdotV2)/(NdotV2) ));
}
float2 BRDF( float3 L, float3 V, float3 N )
{
    float3 H = normalize( L + V );
    float NdotH = ( dot(N, H) );
    float VdotH = ( dot(V, H) );
    float NdotL = ( dot(N, L) );
    float NdotV = ( dot(N, V) );
	float LdotH = ( dot(L, H) );
    
    float diffuse = NdotL + ( -NdotL*Fresnel( fresnel_coeff, VdotH ) );// ( 1 - Fresnel( fresnel_coeff, VdotH ) ) * NdotL;
    float rough_sqr = rough_coeff*rough_coeff;
	float D = D_Exponential( rough_coeff, acos(NdotH) );
    D *= ( 1 + 4*rough_sqr ) / ( 2*rough_sqr * ( 1 + exp(-(PI/(2*rough_coeff) ))) * PI );
	
	float F = Fresnel( fresnel_coeff, LdotH );
	float alphaG = 0.001f;
	float G = G1_GGX(NdotL, alphaG) * G1_GGX(NdotV, alphaG);
	float denom = 1.f / 4.0f * NdotL*NdotV;
    float specular = D*F*G * denom;
    
    return float2( diffuse, specular );
}
/////////////////////////////////////////////////////////////////
out_PS ps_main( in_PS input )
{
	out_PS OUT;
    float3 L = -normalize( sun_dir.xyz );
    float3 N = normalize( input.w_normal );
    float3 V = normalize( eye_pos - input.w_pos );
    float2 brdf = BRDF( L, V, N );

    float2 winpos = input.h_pos.xy * render_target_size_rcp.xy;

    float2 shadow_ssao = _tex_shadow_ssao.SampleLevel( _samp_shadow_ssao, winpos, 0.f ).xy;
    float ssao_term = shadow_ssao.y;
    float shadow_term = shadow_ssao.x;
    float ambient_base = -min( 0.f, brdf.x );
    float ambient_factor = ( 0.1f - ( ambient_base * 0.05 ) ) * ssao_term;
    
    float3 ambient_c = diffuse_color * ambient_factor;
    float3 diffuse_c = saturate( diffuse_color * sun_color.xyz * brdf.x );

#ifdef DIFFUSE_TEX
	float3 diffuse_tex_color = evaluate_diffuse_tex( input.uv );
	diffuse_c *= diffuse_tex_color;
    ambient_c *= diffuse_tex_color;
#endif
    float3 specular_c = ( saturate( ( float3 )brdf.y ) );
    specular_c = lerp( specular_c, specular_c * shadow_term, 0.98f );
    
    diffuse_c = lerp( ambient_c, diffuse_c + ambient_c, shadow_term );
    float3 lit_color = diffuse_c + specular_c;

	OUT.rgba = float4( lit_color, 1.0 );
    //OUT.rgba = float4( ambient_c, 1.0 );
    //OUT.rgba = float4( 1, 1, 1, 1 );
    return OUT;
}
