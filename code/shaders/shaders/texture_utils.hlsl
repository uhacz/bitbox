passes:
{
    copy_rgb =
    {
        vertex = "vs_screenquad";
        pixel = "ps_copy_rgb";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
    };
	copy_rgba = 
	{
		vertex = "vs_screenquad";
        pixel = "ps_copy_rgba";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
	};
    copy_rgba_level = 
	{
		vertex = "vs_screenquad";
        pixel = "ps_copy_rgba_level";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
	};

    copy_r_as_rgb = 
	{
		vertex = "vs_screenquad";
        pixel = "ps_copy_r_as_rgb";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
	};

    mul_rgba = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_mul_rgba";
        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
    }

};#~header

#include <sys/vs_screenquad.hlsl>
#include <sys/samplers.hlsl>

#define in_PS out_VS_screenquad

Texture2D gtexture : register(t0);
Texture2D gtexture1 : register( t1 );


float3 ps_copy_rgb( in_PS IN ) : SV_Target0
{
    return gtexture.SampleLevel(_samp_point, IN.uv, 0).xyz;
}

float4 ps_copy_rgba( in_PS IN ) : SV_Target0
{
    return gtexture.SampleLevel( _samp_point, IN.uv, 0 );
}
float4 ps_copy_rgba_level( in_PS IN ) : SV_Target0
{
    return gtexture.SampleLevel(_samp_point, float3(IN.uv, 10.f), 10.f);
}

float4 ps_copy_r_as_rgb( in_PS IN ) : SV_Target0
{
    float r = gtexture.SampleLevel(_samp_point, IN.uv, 0).r;
    return float4( r, r, r, 1.0f );
}

float4 ps_mul_rgba( in_PS IN ) : SV_Target0
{
    float4 color = gtexture.SampleLevel(_samp_point, IN.uv, 0);
    float scale = gtexture1.SampleLevel(_samp_point, IN.uv, 0).r;
    return color * scale;
}
