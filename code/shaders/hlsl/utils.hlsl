passes:
{
    z_prepass =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    
        hwstate = 
		{
			depth_test = 1;
			depth_write = 1;
            //color_mask = "";
		};
    };
	
    linearize_depth = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_linearize_depth";
        hwstate = 
		{
			depth_test = 0;
		};
    };

    color_add_shadow = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_color_add_shadow";
        hwstate = 
		{
			depth_test = 0;
            depth_write = 0;
		};
    };

    zPrepassDepthOnly =
    {
        vertex = "vs_main";
        pixel = "ps_main";
        hwstate =
        {
            depth_test = 1;
            depth_write = 1;
            //color_mask = "";
        };
    };

}; #~header

#include <sys/frame_data.hlsl>
#include <sys/instance_data.hlsl>
#include <sys/vs_screenquad.hlsl>
#include <sys/util.hlsl>

Texture2D<float> gtex_hwdepth;
SamplerState gsamp_hwdepth;

shared cbuffer MaterialData : register(b3)
{
};

#ifdef z_prepass
struct in_VS
{
	uint   instanceID : SV_InstanceID;
	float4 pos	  	  : POSITION;
	float3 nrm		  : NORMAL;
};

struct in_PS
{
    float4 hpos	: SV_Position;
    /*nointerpolation*/ float3 wnrm : TEXCOORD0;
};

struct out_PS
{
	float3 vnormal : SV_Target0;
};
in_PS vs_main( in_VS input )
{
    in_PS output;
    float4 wpos = mul( world_matrix[input.instanceID], input.pos );
    float3 wnrm = mul( (float3x3)world_it_matrix[input.instanceID], input.nrm.xyz );
    float4 hpos = mul( _camera_viewProj, wpos );
    output.hpos = hpos;
    output.wnrm = wnrm;
    return output;
}

[earlydepthstencil]
out_PS ps_main( in_PS input )
{
    out_PS output;
    output.vnormal.xyz = mul( (float3x3)_camera_view, input.wnrm.xyz );
    //output.vnormal.w = 0.f;
    return output;
}
#endif

#ifdef zPrepassDepthOnly
#include <sys/vertex_transform.hlsl>
struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float4 pos	  	  : POSITION;
};

struct in_PS
{
    float4 hpos	: SV_Position;
};

struct out_PS
{};
in_PS vs_main( in_VS IN )
{
    in_PS OUT = (in_PS)0;

    float4 row0, row1, row2;
    float3 row0IT, row1IT, row2IT;

    fetchWorld( row0, row1, row2, IN.instanceID );
    fetchWorldIT( row0IT, row1IT, row2IT, IN.instanceID );

    float3 wpos = transformPosition( row0, row1, row2, IN.pos );
    OUT.hpos = mul( _camera_viewProj, float4(wpos, 1.f) );
    return OUT;
}

[earlydepthstencil]
out_PS ps_main( in_PS input )
{
    out_PS OUT;
    return OUT;
}
#endif

float ps_linearize_depth( in out_VS_screenquad input ) : SV_Target0
{
    float hwDepth = gtex_hwdepth.SampleLevel( gsamp_hwdepth, input.uv, 0.f ).r;
    return resolveLinearDepth( hwDepth );

    //return ( proj_params.w / ( hw_depth + proj_params.z ) );// *-0.01f;

    
    //float c1 = camera_params.w / camera_params.z;
    //float c0 = 1.0 - c1;
    //return 1.0 / ( c0 * hw_depth + c1 );
}

Texture2D gtex_color;
Texture2D gtex_shadow_ssao;
SamplerState gsamp_color;
SamplerState gsamp_shadow_ssao;

float4 ps_color_add_shadow( in out_VS_screenquad input ) : SV_Target0
{
    float4 color = gtex_color.SampleLevel( gsamp_color, input.uv, 0.f );
    float2 shadow_ssao = gtex_shadow_ssao.SampleLevel( gsamp_shadow_ssao, input.uv, 0.f );

    return color;
}

