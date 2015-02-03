passes:
{
    depth = 
    {
	    vertex = "vs_depth";
        pixel = "ps_depth";

        hwstate = 
		{
		    color_mask = "";
		};
    };

    shadow =
    {
        vertex = "vs_screenquad";
        pixel = "ps_shadow";

        hwstate = 
		{
			depth_test = 0;
			depth_write = 0;
            color_mask = "R";
		};
    };

    add_ssao = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_add_ssao";

        hwstate = 
        {
            depth_test = 0;
            depth_write = 0;
            blend_enable = 1;
            blend_equation = "MIN";
            blend_src_factor = "ONE";
            blend_dst_factor = "ONE";
            color_mask = "G";
        };
    };

};#~header

#include <sys/frame_data.hlsl>
#include <sys/util.hlsl>
#include <sys/instance_data.hlsl>
#include <sys/vs_screenquad.hlsl>

#define NUM_CASCADES 4
#define PCF_FILTER_SIZE 5

shared cbuffer MaterialData : register(b3)
{
    matrix light_view_proj[NUM_CASCADES];
    float4 clip_planes[NUM_CASCADES];
    float3 light_direction_ws;
    float2 occlusion_texture_size;
    float2 shadow_map_size;
};



Texture2D<float> shadowMap;
Texture2D<float> sceneDepthTex;
//Texture2D gnormals_vs;

SamplerState sampl;
SamplerComparisonState samplShadowMap;

/////////////////////////////////////////////////////////////////
struct in_VS_depth
{
	uint   instanceID : SV_InstanceID;
	float4 pos	  	  : POSITION;
};
struct in_PS_depth
{
	float4 hpos	: SV_Position;
};
struct out_PS_depth 
{
//    float linearDepth : SV_Depth;
};
in_PS_depth vs_depth( in_VS_depth input )
{
	in_PS_depth output;
	float4 wpos = mul( world_matrix[input.instanceID], input.pos );
    float4 hpos = mul( _camera_viewProj, wpos );
    output.hpos = hpos;
    return output;
}
[earlydepthstencil]
out_PS_depth ps_depth( in_PS_depth input )
{
	out_PS_depth output;
    //output.linearDepth = resolveLinearDepth( input.hpos.z / input.hpos.w, camera_params.x, camera_params.y );
	return output;
}

#define in_PS_shadow out_VS_screenquad

float sample_shadow_map( float lightDepth, float2 shadowUV, float bias )
{
    //float shadowValue = ( shadowMap.SampleLevel( samplShadowMap, shadowUV, 0 ).x + bias );
    //return smoothstep( 0.f, bias, shadowValue - lightDepth );
    //return step( lightDepth, shadowValue );
    //return ( shadowValue < lightDepth ) ? 0.0f : 1.0f;
    return shadowMap.SampleCmpLevelZero( samplShadowMap, shadowUV.xy, lightDepth - bias );
}

float sample_shadow_pcf( float light_depth, float2 shadow_uv, float bias )
{
    float result = 0.0;
    const float2 shadow_map_size_inv = float2( 1.0, 1.0 ) / shadow_map_size;
    
    const int r = PCF_FILTER_SIZE - 2;

    for(int x=-r; x<=r; x++)
    {
        for(int y=-r; y<=r; y++)
        {
            float2 off = float2( x, y ) * shadow_map_size_inv;
            result += sample_shadow_map( light_depth, shadow_uv+off, bias );
        }
    }
    return result / ( PCF_FILTER_SIZE*PCF_FILTER_SIZE );
}

float sample_shadow_gauss5x5( float light_depth, float2 base_uv, float bias )
{
    const float gaussKernel5x5[25] = 
    { 
	    0.012477641543232876f, 0.02641516735431067f, 0.03391774626899505f, 0.02641516735431067f, 0.012477641543232876f, 
	    0.02641516735431067f, 0.05592090972790156f, 0.07180386941492609f, 0.05592090972790156f, 0.02641516735431067f, 
	    0.03391774626899505f, 0.07180386941492609f, 0.09219799334529226f, 0.07180386941492609f, 0.03391774626899505f, 
	    0.02641516735431067f, 0.05592090972790156f, 0.07180386941492609f, 0.05592090972790156f, 0.02641516735431067f, 
	    0.012477641543232876f, 0.02641516735431067f, 0.03391774626899505f, 0.02641516735431067f, 0.012477641543232876f, 
	};

    float result = 0;
    float2 sm_size_inv = 1.0f / shadow_map_size;
	const int r = 2;
	for ( int x = -r; x <= r; ++x )
	{
		for ( int y = -r; y <= r; ++y )
		{
			float2 offset = float2(x, y) * sm_size_inv;
			float2 sample_uv = base_uv + offset;

			float weight = gaussKernel5x5[ (x+2)*5 + y+2 ];
            float sample = sample_shadow_map( light_depth, sample_uv.xy, bias );
			result += sample * weight;
		}
	}

	return result;
}

float ps_shadow( in in_PS_shadow input ) : SV_Target0
{
    // Reconstruct view-space position from the depth buffer
    float pixel_depth  = sceneDepthTex.SampleLevel( sampl, input.uv, 0.0f ).r;
    float linear_depth = resolveLinearDepth( pixel_depth );

    float2 screenPos_m11 = input.wpos01 * 2.0 - 1.0;
    float3 vs_pos = resolvePositionVS( screenPos_m11, -linear_depth, _camera_projParams.xy );
    
    int current_split = 0;

    for( int i = 1; i < NUM_CASCADES; i++ )
    {
        if( vs_pos.z <= clip_planes[i].x && vs_pos.z >= clip_planes[i].y )
        {
            current_split = i;
            break;
        }
    }
    const float NUM_CASCADES_INV = 1.0f / (float)NUM_CASCADES;
    float offset = (float)current_split * NUM_CASCADES_INV ;

    //float3 N = gnormals_vs.SampleLevel( _samplerr, input.uv, 0.f ).xyz;
    //float3 L = light_direction_ws;
    
    float4 ws_pos = mul( _camera_world, float4(vs_pos, 1.0) );
    //float4 ws_nrm = mul( camera_world, float4( N, 1.0 ) );

    //const float n_dot_l = saturate( dot( ws_nrm, L ) );
    //float3 shadow_offset = get_shadow_pos_offset( n_dot_l, ws_nrm );
    //ws_pos.xyz += shadow_offset;

    float4 light_hpos = mul( light_view_proj[current_split], ws_pos );
    light_hpos /= light_hpos.w;
    // Transform from light space to shadow map texture space.
    float2 shadow_uv = 0.5 *  light_hpos.xy + float2(0.5f, 0.5f);
    shadow_uv.x = ( shadow_uv.x * NUM_CASCADES_INV ) + offset;
    shadow_uv.y = 1.f-shadow_uv.y;
    shadow_uv = shadow_uv;
    // Offset the coordinate by half a texel so we sample it correctly
    //shadow_uv += ( 0.5f / shadow_map_size );
	
    float light_depth = light_hpos.z;
	//light_depth = resolveLinearDepth( light_depth, clip_planes[current_split].x, clip_planes[current_split].y );

    const float bias = (1.f + pow( 2.f, (float)current_split )) / shadow_map_size.y;
    //float shadow_value = sample_shadow_gauss5x5( light_depth, shadow_uv, bias );
    float shadow_value = sample_shadow_map( light_depth, shadow_uv, bias );
    
    return shadow_value;
    //return float2(offset, 1.f);
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
Texture2D<float> gtex_ssao;
SamplerState gsamp_ssao;

float2 ps_add_ssao( in out_VS_screenquad input ) : SV_Target0
{
    float ssao_value = gtex_ssao.SampleLevel( gsamp_ssao, input.uv, 0.f ).r;
    float2 result;
    result.x = 1.f;
    result.y = ssao_value;
    return result;
}



