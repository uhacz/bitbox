passes:
{
    depth = 
    {
	    vertex = "vs_depth";
        pixel = "ps_depth";

        hwstate = 
		{
		    color_mask = "";
            //cull_mode = "FRONT";
            //depth_function = "LESS";
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

    shadowVolume =
    {
        vertex = "vs_screenquad";
        pixel = "ps_shadowVolume";

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
#define NUM_CASCADES_INV ( 1.0 / (float)NUM_CASCADES )
#define FILTER_SIZE 7

shared cbuffer MaterialData : register(b3)
{
    float4x4 worldToShadowSpace[NUM_CASCADES];
    float4x4 viewToShadowSpace[NUM_CASCADES];
    float4 clipPlanes_bias_nOffset[NUM_CASCADES];
    float3 lightDirectionWS;
    float2 occlusionTextureSize;
    float2 shadowMapSize;
    int useNormalOffset;
};

float biasGet( in uint cascadeIdx )
{
    return clipPlanes_bias_nOffset[cascadeIdx].z;
}
float normalOffsetGet( in uint cascadeIdx )
{
    return clipPlanes_bias_nOffset[cascadeIdx].w;
}
float2 clipPlaneGet( in uint cascadeIdx )
{
    return clipPlanes_bias_nOffset[cascadeIdx].xy;
}

Texture2D<float> shadowMap;
Texture2D<float> sceneDepthTex;
Texture2D normalsVS;
//Texture2D gnormals_vs;

SamplerState sampl;
SamplerComparisonState samplShadowMap;
SamplerState samplNormalsVS;

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
	return output;
}

#define in_PS_shadow out_VS_screenquad


float shadowMap_sample( float lightDepth, float2 shadowUV )
{
    return shadowMap.SampleCmpLevelZero( samplShadowMap, shadowUV.xy, lightDepth );
}
float shadowMap_sample1( float2 base_uv, float u, float v, float2 shadowMapSizeInv, uint cascadeIdx, float lightDepth/*, float2 receiverPlaneDepthBias*/ )
{
    float2 uv = base_uv + float2(u, v) * shadowMapSizeInv;
    //float z = ( useReceiverPlaneDepthBias ) ? ( lightDepth + dot( float2(u, v) * shadowMapSizeInv, receiverPlaneDepthBias ) ) : lightDepth;
    float z = lightDepth;
    return shadowMap.SampleCmpLevelZero( samplShadowMap, uv, z );
}
float2 computeReceiverPlaneDepthBias( float3 texCoordDX, float3 texCoordDY )
{
    float2 biasUV;
    biasUV.x = texCoordDY.y * texCoordDX.z - texCoordDX.y * texCoordDY.z;
    biasUV.y = texCoordDX.x * texCoordDY.z - texCoordDY.x * texCoordDX.z;
    biasUV *= 1.0f / ((texCoordDX.x * texCoordDY.y) - (texCoordDX.y * texCoordDY.x));
    return biasUV;
}
//-------------------------------------------------------------------------------------------------
// The method used in The Witness
//-------------------------------------------------------------------------------------------------

float4 computeShadowUV( in float3 shadowPos, in uint cascadeIdx )
{
    float2 texelSize = 1.0f / shadowMapSize;
        
    float2 uv = shadowPos.xy * shadowMapSize.xy;
    float2 shadowMapSizeInv = 1.0 / shadowMapSize.xy;

    float2 base_uv;
    base_uv.x = floor( uv.x + 0.5 );
    base_uv.y = floor( uv.y + 0.5 );

    float s = (uv.x + 0.5 - base_uv.x);
    float t = (uv.y + 0.5 - base_uv.y);

    base_uv -= float2(0.5, 0.5);
    base_uv *= shadowMapSizeInv;

    const float offsetUV = cascadeIdx * NUM_CASCADES_INV;
    base_uv.x = (base_uv.x * NUM_CASCADES_INV) + offsetUV;
    base_uv.y = 1.f - base_uv.y;
    base_uv += texelSize * 0.5f;

    return float4( base_uv, s, t );
}

float sampleShadowMap_simple( in float3 shadowPos, in uint cascadeIdx )
{
    const float bias = biasGet( cascadeIdx ); // *(1.f + pow( 2.f, (float)cascadeIdx )) / shadowMapSize.y;
    float lightDepth = saturate( shadowPos.z ) - bias;
    float2 base_uv = computeShadowUV( shadowPos, cascadeIdx ).xy;
    return shadowMap_sample( lightDepth, base_uv );
}

float sampleShadowMap_optimizedPCF( in float3 shadowPos, in uint cascadeIdx )
{
    //const float bias = 0.001f;
    const float bias = biasGet( cascadeIdx ); // *(1.f + pow( 2.f, (float)cascadeIdx )) / shadowMapSize.y;
    float lightDepth = saturate( shadowPos.z ) - bias;
    float2 shadowMapSizeInv = 1.0 / shadowMapSize.xy;
    float4 base_uvst = computeShadowUV( shadowPos, cascadeIdx );

    float2 base_uv = base_uvst.xy;
    float s = base_uvst.z;
    float t = base_uvst.w;

    float sum = 0;

#if FILTER_SIZE == 2
    return shadowMap_sample( lightDepth, base_uv );
#elif FILTER_SIZE == 3

    float uw0 = (3 - 2 * s);
    float uw1 = (1 + 2 * s);

    float u0 = (2 - s) / uw0 - 1;
    float u1 = s / uw1 + 1;

    float vw0 = (3 - 2 * t);
    float vw1 = (1 + 2 * t);

    float v0 = (2 - t) / vw0 - 1;
    float v1 = t / vw1 + 1;

    sum += uw0 * vw0 * shadowMap_sample1( base_uv, u0, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw0 * shadowMap_sample1( base_uv, u1, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw0 * vw1 * shadowMap_sample1( base_uv, u0, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw1 * shadowMap_sample1( base_uv, u1, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
                                                                                                   
    return sum * 1.0f / 16;                                                                        
                                                                                                   
#elif FILTER_SIZE == 5                                                                             
                                                                                                   
    float uw0 = (4 - 3 * s);                                                                       
    float uw1 = 7;                                                                                 
    float uw2 = (1 + 3 * s);                                                                       
                                                                                                   
    float u0 = (3 - 2 * s) / uw0 - 2;                                                              
    float u1 = (3 + s) / uw1;                                                                      
    float u2 = s / uw2 + 2;                                                                        
                                                                                                   
    float vw0 = (4 - 3 * t);                                                                       
    float vw1 = 7;                                                                                 
    float vw2 = (1 + 3 * t);                                                                       
                                                                                                   
    float v0 = (3 - 2 * t) / vw0 - 2;                                                              
    float v1 = (3 + t) / vw1;                                                                      
    float v2 = t / vw2 + 2;                                                                        
                                                                                                   
    sum += uw0 * vw0 * shadowMap_sample1( base_uv, u0, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw0 * shadowMap_sample1( base_uv, u1, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw2 * vw0 * shadowMap_sample1( base_uv, u2, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
                                                                                                   /*                        */
    sum += uw0 * vw1 * shadowMap_sample1( base_uv, u0, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw1 * shadowMap_sample1( base_uv, u1, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw2 * vw1 * shadowMap_sample1( base_uv, u2, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
                                                                                                   /*                        */
    sum += uw0 * vw2 * shadowMap_sample1( base_uv, u0, v2, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw2 * shadowMap_sample1( base_uv, u1, v2, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw2 * vw2 * shadowMap_sample1( base_uv, u2, v2, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
                                                                                                  
    return sum * 1.0f / 144;                                                                      
                                                                                                  
#else // FilterSize_ == 7                                                                         
                                                                                                  
    float uw0 = (5 * s - 6);                                                                      
    float uw1 = (11 * s - 28);                                                                    
    float uw2 = -(11 * s + 17);                                                                   
    float uw3 = -(5 * s + 1);                                                                     
                                                                                                  
    float u0 = (4 * s - 5) / uw0 - 3;                                                             
    float u1 = (4 * s - 16) / uw1 - 1;                                                            
    float u2 = -(7 * s + 5) / uw2 + 1;                                                            
    float u3 = -s / uw3 + 3;                                                                      
                                                                                                  
    float vw0 = (5 * t - 6);                                                                      
    float vw1 = (11 * t - 28);                                                                    
    float vw2 = -(11 * t + 17);                                                                   
    float vw3 = -(5 * t + 1);                                                                     
                                                                                                  
    float v0 = (4 * t - 5) / vw0 - 3;                                                             
    float v1 = (4 * t - 16) / vw1 - 1;                                                            
    float v2 = -(7 * t + 5) / vw2 + 1;                                                            
    float v3 = -t / vw3 + 3;                                                                      
                                                                                                  
    sum += uw0 * vw0 * shadowMap_sample1( base_uv, u0, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw0 * shadowMap_sample1( base_uv, u1, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw2 * vw0 * shadowMap_sample1( base_uv, u2, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw3 * vw0 * shadowMap_sample1( base_uv, u3, v0, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
                                                                                                   /*                        */
    sum += uw0 * vw1 * shadowMap_sample1( base_uv, u0, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw1 * shadowMap_sample1( base_uv, u1, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw2 * vw1 * shadowMap_sample1( base_uv, u2, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw3 * vw1 * shadowMap_sample1( base_uv, u3, v1, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
                                                                                                   /*                        */
    sum += uw0 * vw2 * shadowMap_sample1( base_uv, u0, v2, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw2 * shadowMap_sample1( base_uv, u1, v2, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw2 * vw2 * shadowMap_sample1( base_uv, u2, v2, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw3 * vw2 * shadowMap_sample1( base_uv, u3, v2, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
                                                                                                   /*                        */
    sum += uw0 * vw3 * shadowMap_sample1( base_uv, u0, v3, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw1 * vw3 * shadowMap_sample1( base_uv, u1, v3, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw2 * vw3 * shadowMap_sample1( base_uv, u2, v3, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );
    sum += uw3 * vw3 * shadowMap_sample1( base_uv, u3, v3, shadowMapSizeInv, cascadeIdx, lightDepth/*, receiverPlaneDepthBias*/ );

    return sum * 1.0f / 2704;

#endif
}

uint selectSplit( float z )
{
    uint currentSplit = 0;
    for( uint i = 0; i < NUM_CASCADES; ++i )
    {
        [flatten]
        if( z < clipPlaneGet(i).x )
        {
            currentSplit = i;
        }
    }

    return currentSplit;
}

float ps_shadow( in in_PS_shadow input ) : SV_Target0
{
    // Reconstruct view-space position from the depth buffer
    float pixelDepth  = sceneDepthTex.SampleLevel( sampl, input.uv, 0.0f ).r;
    float linearDepth = resolveLinearDepth( pixelDepth );
    float2 screenPos_m11 = input.screenPos;
    float3 posVS = resolvePositionVS( screenPos_m11, -linearDepth, _camera_projParams.xy );

    uint currentSplit = selectSplit( posVS.z );
    float4 posWS = mul( _camera_world, float4(posVS, 1.0) );

    if( useNormalOffset )
    {
        const float3 nrmVS = cross( normalize( ddy_fine(posVS) ), normalize( ddx_fine(posVS) ) );
        //const float3 nrmVS = normalsVS.SampleLevel( samplNormalsVS, input.uv, 0.0 );
        const float3 N = normalize( mul( (float3x3)_camera_world, nrmVS ) );
        const float scale = 1.f - saturate(dot( lightDirectionWS, N ));
        const float offsetScale  = scale * normalOffsetGet( currentSplit );
        const float3 posOffset = N * offsetScale; 
        posWS.xyz += posOffset;
    }

    float4 shadowPos = mul( worldToShadowSpace[currentSplit], posWS );
    float shadowValue = sampleShadowMap_optimizedPCF( shadowPos.xyz, currentSplit );
    return shadowValue;
}

#define NUM_STEPS 64
float ps_shadowVolume( in in_PS_shadow input ) : SV_Target0
{
    // Reconstruct view-space position from the depth buffer
    float pixelDepth  = sceneDepthTex.SampleLevel( sampl, input.uv, 0.0f ).r;
    float linearDepth = resolveLinearDepth( pixelDepth );
    float2 screenPos_m11 = input.screenPos;
    float3 posVS = resolvePositionVS( screenPos_m11, -linearDepth, _camera_projParams.xy );

    uint currentSplit = selectSplit( posVS.z );
    float4 posWS = mul( _camera_world, float4(posVS, 1.0) );

    float3 rayDirVS = posVS;
    const float rayLengthVS = length( rayDirVS );
    rayDirVS *= rcp( rayLengthVS );
    float step = rayLengthVS / (float)(NUM_STEPS);
    float currRayLen = _camera_zNear;
    float value = 0.f;
    while( currRayLen < rayLengthVS )
    {
        const float3 currSamplePosVS = rayDirVS*currRayLen;
        const float viewZ = currSamplePosVS.z;
        const uint currSampleCascadeIdx = selectSplit( viewZ );
        const float3 currShadowPos = mul( viewToShadowSpace[currSampleCascadeIdx], float4( currSamplePosVS, 1.0 ) ).xyz;
        value += sampleShadowMap_simple( currShadowPos, currSampleCascadeIdx );
        currRayLen += step;
    }

    value /= NUM_STEPS;

    return value;
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



