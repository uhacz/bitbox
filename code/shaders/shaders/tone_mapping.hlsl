passes:
{
    //bloom_threshold = 
    //{
	//    vertex = "vs_screenquad";
    //    pixel = "PS_bloom_threshold";
    //    hwstate = 
    //    {
    //        depth_test = 0;
    //        depth_write = 0;
    //    };
    //};
    //bloom_blurh = 
    //{
	//    vertex = "vs_screenquad";
    //    pixel = "PS_bloom_blurh";
    //    hwstate = 
    //    {
    //        depth_test = 0;
    //        depth_write = 0;
    //    };
    //};
    //bloom_blurv = 
    //{
	//    vertex = "vs_screenquad";
    //    pixel = "PS_bloom_blurv";
    //    hwstate = 
    //    {
    //        depth_test = 0;
    //        depth_write = 0;
    //    };
    //};

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

    //scale = 
    //{
    //    vertex = "vs_screenquad";
    //    pixel = "PS_scale";
    //    hwstate = 
    //    {
    //        depth_test = 0;
    //        depth_write = 0;
    //    };
    //};

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

#include <sys/vs_screenquad.hlsl>
#include <sys/samplers.hlsl>
#include <sys/binding_map.h>
#include <tone_mapping_data.h>

Texture2D tex_input0 : register( t0 );
Texture2D tex_input1 : register( t1 );
Texture2D tex_input2 : register( t2 );

#if 0 
// Approximates luminance from an RGB value
float CalcLuminance( float3 color )
{
    return max( dot( color, float3( 0.299f, 0.587f, 0.114f ) ), 0.000001f );
}

// Retrieves the log-average lumanaince from the texture
float GetAvgLuminance( Texture2D lumTex, float2 texCoord )
{
    return max( exp( lumTex.Load( int3( 0, 0, 10 ) ).x ), 0.000001 );
    //return exp( lumTex.SampleLevel( _samp_linear, texCoord, 10.0 ).x );
}
float3 ToneMap_ACESFilm( float3 x )
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate( ( x * ( a * x + b ) ) / ( x * ( c * x + d ) + e ) );
}
// Applies the filmic curve from John Hable's presentation
float3 ToneMap_FilmicALU( float3 color )
{
    color = max( 0, color - 0.004f );
    color = ( color * ( 6.2f * color + 0.5f ) ) / ( color * ( 6.2f * color + 1.7f ) + 0.06f );

    // result has 1/2.2 baked in
    return pow( color, 2.2f );
}
// Determines the color based on exposure settings
float3 CalcExposedColor( float3 color, float avgLuminance, float threshold, out float exposure )
{
    exposure = 0;

    // Use geometric mean        
    avgLuminance = max( avgLuminance, 0.000001f );

    float keyValue = 0;
    if( use_auto_exposure == 0 )
        keyValue = exposure_key_value;
    else
        keyValue = 1.03f - ( 2.0f / ( 2 + log10( avgLuminance + 1 ) ) );

    float linearExposure = ( keyValue / avgLuminance );
    exposure = log2( max( linearExposure, 0.000001f ) );
    exposure -= threshold;
    return exp2( exposure ) * color;
}

float3 ToneMap( float3 color, float avgLuminance, float threshold, out float exposure )
{
    float pixelLuminance = CalcLuminance( color );
    color = CalcExposedColor( color, avgLuminance, threshold, exposure );
    color = ToneMap_ACESFilm( color );
    //color = ToneMap_FilmicALU( color );
    return color;
}
// Applies exposure and tone mapping to the input, and combines it with the
// results of the bloom pass
void PS_composite( in out_VS_screenquad input,
				out float4 output_color : SV_Target0 )
{
    // Tone map the primary input
    float avg_luminance = GetAvgLuminance( tex_input1, input.uv );
    float3 color = tex_input0.Sample( _samp_point, input.uv ).rgb;
    float exposure = 0;
    color = ToneMap( color, avg_luminance, 0, exposure );
#ifdef WITH_BLOOM
    // Sample the bloom
    float3 bloom = tex_input2.Sample( _samp_linear, input.uv ).rgb;
    bloom = bloom * bloom_magnitude;

    // Add in the bloom
	color = color + bloom;
#endif
    output_color = float4( color, 1.0f );
}

// Creates the luminance map for the scene
float4 PS_luminance_map( in out_VS_screenquad input ) : SV_Target
{
    // Sample the input
    float3 color = tex_input0.Sample( _samp_linear, input.uv ).rgb;
   
    // calculate the luminance using a weighted average
    float luminance = CalcLuminance( color );
    return luminance.xxxx;
}

// Slowly adjusts the scene luminance based on the previous scene luminance
float4 PS_adapt_luminance( in out_VS_screenquad input ) : SV_Target
{
    float last_lum = exp( tex_input0.Sample( _samp_point, input.uv ).x );
    float current_lum = tex_input1.Sample( _samp_point, input.uv ).x;
    
    //float adapted_lum = last_lum + ( current_lum - last_lum ) * ( 1.f - exp( -delta_time * lum_tau ) );
    float adaptation_rate = 0.01f;
    float adapted_lum = last_lum - last_lum * adaptation_rate + current_lum * adaptation_rate; // above equation rewritten to allow better instruction scheduling
    return log( max( adapted_lum, 0.00001f ) );
}
#endif

#if 1

/*
* Get an exposure using the Saturation-based Speed method.
*/

#define sqr( x ) ( (x) * (x) )
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


float computeEV100( float aperture, float shutterTime, float ISO )
{
    return log2( sqr( aperture ) / shutterTime * 100.f / ISO );
}
float computeEV100FromAvgLuminance( float avgLuminance )
{
    return log2( avgLuminance * 100.0f / 12.5f );
}

float convertEV100ToExposure( float EV100 )
{
    float maxLuminance = 1.2f * pow( 2.0f, EV100 );    return 1.0f / maxLuminance;
}

#define EPSILON 0.001
// Approximates luminance from an RGB value
float Calc_luminance(float3 color)
{
    return max(dot(color, float3(0.299f, 0.587f, 0.114f)), EPSILON );
}

// Retrieves the log-average lumanaince from the texture
float Get_avg_luminance(Texture2D tex_lum, float2 uv )
{
	//return exp( tex_lum.SampleLevel( _samp_linear, uv, 10.f ).x );
    return max( exp( tex_lum.Load( int3( 0, 0, 10 ) ).x ), EPSILON );
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Applies the filmic curve from John Hable's presentation
float3 Tone_map_filmic_ALU( float3 color )
{
    return ACESFilm(color);
    //color = max(0, color - 0.004f);
    //color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f)+ 0.06f);
    //
    //// result has 1/2.2 baked in
    //return pow(color, 2.2f);
    //return color;
}

// Determines the color based on exposure settings
float3 Calc_exposed_color( float3 color, float avgLuminance, float threshold )
{    
    float currentEV = 0.f;    if( use_auto_exposure )    {        currentEV = computeEV100FromAvgLuminance( avgLuminance );    }    else    {        currentEV = computeEV100(camera_aperture, camera_shutterSpeed, camera_iso);    }    float exposure = convertEV100ToExposure(currentEV);    exposure -= threshold;    //float currentEV = computeEV100(camera_aperture, camera_shutterSpeed, camera_iso);            
    
    return (exposure) * color;
}

// Applies exposure and tone mapping to the specific color, and applies
// the threshold to the exposure value. 
float3 Tone_map( float3 color, float avg_luminance, float threshold )
{
    //float pixel_luminance = Calc_luminance( color );
    float3 exposedColor = Calc_exposed_color( color, avg_luminance, threshold );
    return exposedColor;
    //return Tone_map_filmic_ALU( exposedColor );
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
		float4 sample = tex_input0.Sample( _samp_point, texcoord );
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

    color = tex_input0.Sample( _samp_linear, input.uv ).rgb;

    // Tone map it to threshold
    float avg_luminance = Get_avg_luminance( tex_input1, input.uv );
	color = Tone_map( color, avg_luminance, bloom_thresh );
    
    return float4(color, 1.0f);
}

// Uses hw bilinear filtering for upscaling or downscaling
float4 PS_scale(in out_VS_screenquad input) : SV_Target
{
    return tex_input0.Sample( _samp_linear, input.uv );
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
    float avg_luminance = Get_avg_luminance( tex_input1, input.uv );
    float3 color = tex_input0.Sample( _samp_point, input.uv ).rgb;
    color = Tone_map(color, avg_luminance, 0 );
#ifdef WITH_BLOOM
    // Sample the bloom
    float3 bloom = tex_input2.Sample( _samp_linear, input.uv ).rgb;
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
    float3 color = tex_input0.Sample(_samp_linear, input.uv ).rgb;
   
    // calculate the luminance using a weighted average
    float luminance = Calc_luminance( color );
    return float4(luminance, luminance, luminance, luminance );
}

// Slowly adjusts the scene luminance based on the previous scene luminance
float4 PS_adapt_luminance(in out_VS_screenquad input) : SV_Target
{
    float last_lum    = exp( tex_input0.Sample( _samp_point, input.uv ).x );
    float current_lum = tex_input1.Sample( _samp_point, input.uv ).x;
    
    //float adapted_lum = last_lum + ( current_lum - last_lum ) * ( 1.f - exp( -delta_time * lum_tau ) );
    float adaptation_rate = 0.1f;
    float adapted_lum = last_lum - last_lum * adaptation_rate + current_lum * adaptation_rate; // above equation rewritten to allow better instruction scheduling
    return log( max( adapted_lum, EPSILON ) );
}
#endif