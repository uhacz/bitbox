passes:
{
    bloom_threshold = 
    {
	    vertex = "vs_screenquad";
        pixel = "PS_bloom_threshold";
        hwstate = 
        {
            depth_test = 0;
            depth_write = 0;
        };
    };
    bloom_blurh = 
    {
	    vertex = "vs_screenquad";
        pixel = "PS_bloom_blurh";
        hwstate = 
        {
            depth_test = 0;
            depth_write = 0;
        };
    };
    bloom_blurv = 
    {
	    vertex = "vs_screenquad";
        pixel = "PS_bloom_blurv";
        hwstate = 
        {
            depth_test = 0;
            depth_write = 0;
        };
    };

    luminance_map = 
    {
        vertex = "vs_screenquad";
        pixel = "PS_luminance_map";
        hwstate = 
        {
            depth_test = 0;
            depth_write = 0;
        };
    };

    composite =
    {
        vertex = "vs_screenquad";
        pixel = "PS_composite";
        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
        };
    };

    scale = 
    {
        vertex = "vs_screenquad";
        pixel = "PS_scale";
        hwstate = 
        {
            depth_test = 0;
            depth_write = 0;
        };
    };

    adapt_luminance = 
    {
        vertex = "vs_screenquad";
        pixel = "PS_adapt_luminance";
        hwstate = 
        {
            depth_test = 0;
            depth_write = 0;
        };
    };
};#~header

#include <sys/frame_data.hlsl>
#include <sys/vs_screenquad.hlsl>


Texture2D _tex_input0 : register( t0 );
Texture2D _tex_input1 : register( t1 );
Texture2D _tex_input2 : register( t2 );

SamplerState _samp_point  : register( s0 );
SamplerState _samp_linear : register( s1 );

shared cbuffer MaterialData : register(b3)
{
    float2 input_size0;
    float delta_time;
    float bloom_thresh;
    float bloom_blur_sigma;
    float bloom_magnitude;
    float lum_tau;
    float auto_exposure_key_value;

    float camera_aperture;
    float camera_shutterSpeed;
    float camera_iso;
};

/*
* Get an exposure using the Saturation-based Speed method.
*/

#define sqr( x ) ( x * x )
float getSaturationBasedExposure(float aperture,
                                 float shutterSpeed,
                                 float iso)
{
    float l_max = (7800.0f / 65.0f) * sqr(aperture) / (iso * shutterSpeed);
    return 1.0f / l_max;
}
 
/*
* Get an exposure using the Standard Output Sensitivity method.
* Accepts an additional parameter of the target middle grey.
*/
float getStandardOutputBasedExposure(float aperture,
                                     float shutterSpeed,
                                     float iso,
                                     float middleGrey = 0.18f)
{
    float l_avg = (1000.0f / 65.0f) * sqr(aperture) / (iso * shutterSpeed);
    return middleGrey / l_avg;
}

// Approximates luminance from an RGB value
float Calc_luminance(float3 color)
{
    return max(dot(color, float3(0.299f, 0.587f, 0.114f)), 0.0001f);
}

// Retrieves the log-average lumanaince from the texture
float Get_avg_luminance(Texture2D tex_lum, float2 uv )
{
	return exp( tex_lum.SampleLevel( _samp_linear, uv, 10.f ).x );
}

// Applies the filmic curve from John Hable's presentation
float3 Tone_map_filmic_ALU( float3 color )
{
    color = max(0, color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f)+ 0.06f);

    // result has 1/2.2 baked in
    return pow(color, 2.2f);
    //return color;
}

// Determines the color based on exposure settings
float3 Calc_exposed_color( float3 color, float avg_luminance, float threshold, out float exposure )
{    
    avg_luminance = max(avg_luminance, 0.001f);
    float key_value = auto_exposure_key_value;// 1.03f - ( 2.0f / ( 2 + log10( avg_luminance + 1 ) ) );
    
    //float linear_exposure = ( key_value / avg_luminance );
    float linear_exposure = getStandardOutputBasedExposure( camera_aperture, camera_shutterSpeed, camera_iso, key_value );

    exposure = log2( max( linear_exposure, 0.0001f ) );

    exposure -= threshold;
    return exp2(exposure) * color;
}

// Applies exposure and tone mapping to the specific color, and applies
// the threshold to the exposure value. 
float3 Tone_map( float3 color, float avg_luminance, float threshold, out float exposure )
{
    float pixel_luminance = Calc_luminance( color );
    color = Calc_exposed_color( color, avg_luminance, threshold, exposure );
    color = Tone_map_filmic_ALU(color);
    return color;
}

// Calculates the gaussian blur weight for a given distance and sigmas
float Calc_gaussian_weight( int sample_dist, float sigma )
{
	float g = 1.0f / sqrt( 2.0f * 3.14159 * sigma * sigma );  
	return ( g * exp( -( sample_dist * sample_dist ) / (2 * sigma * sigma) ) );
}

// Performs a gaussian blue in one direction
float4 Blur(in out_VS_screenquad input, float2 tex_scale, float sigma )
{
    float2 input_size_inv = float2( 1.f, 1.f ) / input_size0;
    float4 color = 0;
    
    for (int i = -6; i < 6; i++)
    {   
		float weight = Calc_gaussian_weight( i, sigma );
        float2 texcoord = input.uv;
		texcoord += ( i * input_size_inv ) * tex_scale;
		float4 sample = _tex_input0.Sample( _samp_point, texcoord );
		color += sample * weight;
    }

    return color;
}


// ================================================================================================
// Shader Entry Points
// ================================================================================================
// Uses a lower exposure to produce a value suitable for a bloom pass
float4 PS_bloom_threshold(in out_VS_screenquad input) : SV_Target
{             
    float3 color = 0;

    color = _tex_input0.Sample( _samp_linear, input.uv ).rgb;

    // Tone map it to threshold
    float avg_luminance = Get_avg_luminance( _tex_input1, input.uv );
	float exposure = 0;
    color = Tone_map( color, avg_luminance, bloom_thresh, exposure );
    
    return float4(color, 1.0f);
}

// Uses hw bilinear filtering for upscaling or downscaling
float4 PS_scale(in out_VS_screenquad input) : SV_Target
{
    return _tex_input0.Sample( _samp_linear, input.uv );
}

// Horizontal gaussian blur
float4 PS_bloom_blurh(in out_VS_screenquad input) : SV_Target
{
    return Blur( input, float2(1, 0), bloom_blur_sigma );
}

// Vertical gaussian blur
float4 PS_bloom_blurv(in out_VS_screenquad input) : SV_Target
{
    return Blur( input, float2(0, 1), bloom_blur_sigma );
}

// Applies exposure and tone mapping to the input, and combines it with the
// results of the bloom pass
void PS_composite(in out_VS_screenquad input, 
				out float4 output_color    : SV_Target0 )
{
    // Tone map the primary input
    float avg_luminance = Get_avg_luminance( _tex_input1, input.uv );
    float3 color = _tex_input0.Sample( _samp_point, input.uv ).rgb;
    float exposure = 0;
	color = Tone_map(color, avg_luminance, 0, exposure);
#ifdef WITH_BLOOM
    // Sample the bloom
    float3 bloom = _tex_input2.Sample( _samp_linear, input.uv ).rgb;
    bloom = bloom * bloom_magnitude;

    // Add in the bloom
	color = color + bloom;
#endif
    output_color = float4(color, 1.0f);
}

// Creates the luminance map for the scene
float4 PS_luminance_map(in out_VS_screenquad input) : SV_Target
{
    // Sample the input
    float3 color = _tex_input0.Sample(_samp_linear, input.uv ).rgb;
   
    // calculate the luminance using a weighted average
    float luminance = Calc_luminance( color );
    return float4(luminance, luminance, luminance, luminance );
}

// Slowly adjusts the scene luminance based on the previous scene luminance
float4 PS_adapt_luminance(in out_VS_screenquad input) : SV_Target
{
    float last_lum    = exp( _tex_input0.Sample( _samp_point, input.uv ).x );
    float current_lum = _tex_input1.Sample( _samp_point, input.uv ).x;
       
    // Adapt the luminance using Pattanaik's technique    
    float adapted_lum = last_lum + ( current_lum - last_lum ) * ( 1 - exp( -delta_time * lum_tau ) );

    return float4( log( adapted_lum ).xxxx );
}