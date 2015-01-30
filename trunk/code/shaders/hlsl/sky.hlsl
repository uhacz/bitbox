passes:
{
    sky =
    {
        vertex = "vs_screenquad";
        pixel = "ps_main";
		
		hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
		};
    };
};#~header

#include <sys/frame_data.hlsl>
#include <sys/shading_data.hlsl>
#include <sys/vs_screenquad.hlsl>
#define in_PS out_VS_screenquad

float3 calcExtinction(float dist) 
{
	return exp(dist * sky_params6.xyz);
}
 
float3 calcScattering(float cos_theta) 
{
    float r_phase = (cos_theta * cos_theta) * sky_params6.w + sky_params6.w;
    float m_phase = sky_params1.w * pow(sky_params2.w * cos_theta + sky_params3.w, -1.5);
    return sky_params2.xyz * r_phase + sky_params3.xyz * m_phase;
}
 
float baseOpticalDepth(in float3 ray) 
{
    float a1 = sky_params4.x * ray.y;
    return sqrt(a1 * a1 + sky_params4.w) - a1;
}
 
float opticalDepth(in float3 pos, in float3 ray) 
{
    pos.y += sky_params4.x;
    float a0 = sky_params4.y - dot(pos, pos);
    float a1 = dot(pos, ray);
    return sqrt(a1 * a1 + a0) - a1;
}

float4 ps_main( in_PS IN ) : SV_Target0
{
    float2 uv_m11 = float2( IN.uv.x, 1.0 - IN.uv.y ) * 2.0 - 1.0;
    uv_m11.x *= _camera_aspect; // apect
    float3 sun_vector =-( sun_dir.xyz );
    
    float3 view_vec = normalize( mul( (float3x3)_camera_world, float3( uv_m11, -2.0 ) ) );

    float cos_theta = dot(view_vec, sun_vector);
 
    // optical depth along view ray
    float ray_dist = baseOpticalDepth(view_vec);
 
    // extinction of light along view ray
    float3 extinction = calcExtinction(ray_dist);
 
    // optical depth for incoming light hitting the view ray
    float3 light_ray_pos = view_vec * (ray_dist * sky_params4.z);
    float light_ray_dist = opticalDepth(light_ray_pos, sun_vector);
 
    // optical depth for edge of atmosphere:
    // this handles the case where the sun is low in the sky and
    // the view is facing away from the sun; in this case the distance
    // the light needs to travel is much greater
    float light_ray_dist_full = opticalDepth(view_vec * ray_dist, sun_vector);
 
    light_ray_dist = max(light_ray_dist, light_ray_dist_full);
 
    // cast a ray towards the sun and calculate the incoming extincted light
    float3 incoming_light = calcExtinction(light_ray_dist);
 
    // calculate the in-scattering
    float3 scattering = calcScattering(cos_theta);
    scattering *= 1.0 - extinction;
 
    // combine
    float3 in_scatter = incoming_light * scattering;
 
    // sun disk
    float sun_strength = saturate( cos_theta * sky_params1.x + sky_params1.y );
    sun_strength *= sun_strength;
    float3 sun_disk = extinction * sun_strength;

    float3 tmp = sky_params5.w * sun_disk;
    float4 color;
    color.xyz = sky_params5.xyz * ( tmp + in_scatter );
    color.w = 1.f;
    return color;
    //return float4( sun_strength.xxxx );
	//return float4( color, 1.0 );
}