passes:
{
    depth =
    {
        vertex = "vs_shadowDepth";
        pixel = "ps_shadowDepth";
        hwstate =
        {
            depth_test = 1;
            depth_write = 1;
            color_mask = "";
        };
    };

    resolve =
    {
        vertex = "vs_screenquad";
        pixel = "ps_shadowResolve";

        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            color_mask = "R";
        };
    };

};#~header

#include <sys/vs_screenquad.hlsl>
#include <sys/samplers.hlsl>
#include <sys/util.hlsl>
#include <sys/binding_map.h>
#include <shadow_data.h>

//#define NUM_CASCADES 4
//#define NUM_CASCADES_INV ( 1.0 / (float)NUM_CASCADES )
//#define FILTER_SIZE 7

Texture2D<float> lightDepthTex;
Texture2D<float> sceneDepthTex;
texture2D<float4> sceneNormalsTex;
SamplerComparisonState samplShadowMap;

#define in_PS_shadow out_VS_screenquad

float shadowMap_sample( float lightDepth, float2 shadowUV )
{
    return lightDepthTex.SampleCmpLevelZero( samplShadowMap, shadowUV.xy, lightDepth );
}
float shadowMap_sample1( float2 base_uv, float u, float v, float2 mapSizeRcp, float lightDepth )
{
    float2 uv = base_uv + float2( u, v ) * mapSizeRcp;
    return lightDepthTex.SampleCmpLevelZero( samplShadowMap, uv, lightDepth );
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

float4 computeShadowUV( in float3 shadowPos, in float scaleU, in float offsetU )
{
    float2 texelSize = depthMapSizeRcp;
    float2 uv = shadowPos.xy * depthMapSize.xy;
    
    float2 base_uv;
    base_uv.x = floor( uv.x + 0.5 );
    base_uv.y = floor( uv.y + 0.5 );

    float s = (uv.x + 0.5 - base_uv.x);
    float t = (uv.y + 0.5 - base_uv.y);

    base_uv -= float2(0.5, 0.5);
    base_uv *= texelSize;

    //const float offsetUV = cascadeIdx * NUM_CASCADES_INV;
    base_uv.x = ( base_uv.x * scaleU ) + offsetU;
    base_uv.y = 1.f - base_uv.y;
    base_uv += texelSize * 0.5f;

    return float4( base_uv, s, t );
}

float sampleShadowMap_simple( in float3 shadowPos, in float bias, in float scaleU, in float offsetU )
{
    //const float bias = biasGet( cascadeIdx ); // *(1.f + pow( 2.f, (float)cascadeIdx )) / shadowMapSize.y;
    float lightDepth = saturate( shadowPos.z ) - bias;
    float2 base_uv = computeShadowUV( shadowPos, scaleU, offsetU ).xy;
    return shadowMap_sample( lightDepth, base_uv );
}

float sampleShadowMap_optimizedPCF( in float3 shadowPos, in float bias, in float scaleU, in float offsetU )
{
    //const float bias = 0.001f;
    //const float bias = biasGet( cascadeIdx ); // *(1.f + pow( 2.f, (float)cascadeIdx )) / shadowMapSize.y;
    float lightDepth = saturate( shadowPos.z ) - bias;
    //float2 shadowMapSizeInv = 1.0 / shadowMapSize.xy;
    float4 base_uvst = computeShadowUV( shadowPos, scaleU, offsetU );

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

    sum += uw0 * vw0 * shadowMap_sample1( base_uv, u0, v0, shadowMapSizeInv, lightDepth );
    sum += uw1 * vw0 * shadowMap_sample1( base_uv, u1, v0, shadowMapSizeInv, lightDepth );
    sum += uw0 * vw1 * shadowMap_sample1( base_uv, u0, v1, shadowMapSizeInv, lightDepth );
    sum += uw1 * vw1 * shadowMap_sample1( base_uv, u1, v1, shadowMapSizeInv, lightDepth );
                                                                                                   
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
                                                                                                   
    sum += uw0 * vw0 * shadowMap_sample1( base_uv, u0, v0, shadowMapSizeInv, lightDepth );
    sum += uw1 * vw0 * shadowMap_sample1( base_uv, u1, v0, shadowMapSizeInv, lightDepth );
    sum += uw2 * vw0 * shadowMap_sample1( base_uv, u2, v0, shadowMapSizeInv, lightDepth );
                                                                                       
    sum += uw0 * vw1 * shadowMap_sample1( base_uv, u0, v1, shadowMapSizeInv, lightDepth );
    sum += uw1 * vw1 * shadowMap_sample1( base_uv, u1, v1, shadowMapSizeInv, lightDepth );
    sum += uw2 * vw1 * shadowMap_sample1( base_uv, u2, v1, shadowMapSizeInv, lightDepth );
                                                                                       
    sum += uw0 * vw2 * shadowMap_sample1( base_uv, u0, v2, shadowMapSizeInv, lightDepth );
    sum += uw1 * vw2 * shadowMap_sample1( base_uv, u1, v2, shadowMapSizeInv, lightDepth );
    sum += uw2 * vw2 * shadowMap_sample1( base_uv, u2, v2, shadowMapSizeInv, lightDepth );
                                                                                                  
    return sum * 1.0f / 144;                                                                      
                                                                                                  
#else // FilterSize_ == 7                                                                         
    //float uw0 = (5 * s - 6);                                                                      
    //float uw1 = (11 * s - 28);                                                                    
    //float uw2 = -(11 * s + 17);                                                                   
    //float uw3 = -(5 * s + 1);                                                                     
    

    //float u0 = (4 * s - 5) / uw0 - 3;                                                             
    //float u1 = (4 * s - 16) / uw1 - 1;                                                            
    //float u2 = -(7 * s + 5) / uw2 + 1;                                                            
    //float u3 = -s / uw3 + 3;                                                                      
    

    //float vw0 = (5 * t - 6);                                                                      
    //float vw1 = (11 * t - 28);                                                                    
    //float vw2 = -(11 * t + 17);                                                                   
    //float vw3 = -(5 * t + 1);                                                                     
    
    //float v0 = (4 * t - 5) / vw0 - 3;                                                             
    //float v1 = (4 * t - 16) / vw1 - 1;                                                            
    //float v2 = -(7 * t + 5) / vw2 + 1;                                                            
    //float v3 = -t / vw3 + 3;                                                                      
    

    //sum += uw0 * vw0 * shadowMap_sample1( base_uv, u0, v0, shadowMapSizeInv, lightDepth );
    //sum += uw1 * vw0 * shadowMap_sample1( base_uv, u1, v0, shadowMapSizeInv, lightDepth );
    //sum += uw2 * vw0 * shadowMap_sample1( base_uv, u2, v0, shadowMapSizeInv, lightDepth );
    //sum += uw3 * vw0 * shadowMap_sample1( base_uv, u3, v0, shadowMapSizeInv, lightDepth );
    //                                                                                   
    //sum += uw0 * vw1 * shadowMap_sample1( base_uv, u0, v1, shadowMapSizeInv, lightDepth );
    //sum += uw1 * vw1 * shadowMap_sample1( base_uv, u1, v1, shadowMapSizeInv, lightDepth );
    //sum += uw2 * vw1 * shadowMap_sample1( base_uv, u2, v1, shadowMapSizeInv, lightDepth );
    //sum += uw3 * vw1 * shadowMap_sample1( base_uv, u3, v1, shadowMapSizeInv, lightDepth );
    //                                                                                   
    //sum += uw0 * vw2 * shadowMap_sample1( base_uv, u0, v2, shadowMapSizeInv, lightDepth );
    //sum += uw1 * vw2 * shadowMap_sample1( base_uv, u1, v2, shadowMapSizeInv, lightDepth );
    //sum += uw2 * vw2 * shadowMap_sample1( base_uv, u2, v2, shadowMapSizeInv, lightDepth );
    //sum += uw3 * vw2 * shadowMap_sample1( base_uv, u3, v2, shadowMapSizeInv, lightDepth );
    //                                                                                   
    //sum += uw0 * vw3 * shadowMap_sample1( base_uv, u0, v3, shadowMapSizeInv, lightDepth );
    //sum += uw1 * vw3 * shadowMap_sample1( base_uv, u1, v3, shadowMapSizeInv, lightDepth );
    //sum += uw2 * vw3 * shadowMap_sample1( base_uv, u2, v3, shadowMapSizeInv, lightDepth );
    //sum += uw3 * vw3 * shadowMap_sample1( base_uv, u3, v3, shadowMapSizeInv, lightDepth );
    float4 uw = float4(
        (5 * s - 6),
        (11 * s - 28),
        -(11 * s + 17),
        -(5 * s + 1)
        );
    float4 u = float4(
        (4 * s - 5) / uw[0] - 3,
        (4 * s - 16) / uw[1] - 1,
        -(7 * s + 5) / uw[2] + 1,
        -s / uw[3] + 3
        );
    float4 vw = float4(
        (5 * t - 6),
        (11 * t - 28),
        -(11 * t + 17),
        -(5 * t + 1)
        );
    float4 v = float4(
        (4 * t - 5) / vw[0] - 3,
        (4 * t - 16) / vw[1] - 1,
        -(7 * t + 5) / vw[2] + 1,
        -t / vw[3] + 3
        );
    for ( int i = 0; i < 4; i++ )
    {
        for ( int j = 0; j < 4; j++ )
        {
            sum += uw[j] * vw[i] * shadowMap_sample1( base_uv, u[j], v[i], depthMapSizeRcp, lightDepth );
        }
    }

    return sum * 1.0f / 2704;

#endif
}

#include <sys/vertex_transform.hlsl>
struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float3 pos	  	  : POSITION;
};

struct in_PS
{
    float4 hpos	: SV_Position;
};

struct out_PS
{};

in_PS vs_shadowDepth( in_VS IN )
{
    in_PS OUT = (in_PS)0;

    float4 row0, row1, row2;
    fetchWorld( row0, row1, row2, IN.instanceID );

    float3 wpos = transformPosition( row0, row1, row2, float4(IN.pos,1.0 ) );
    OUT.hpos = mul( lightViewProj, float4( wpos, 1.f ) );
    return OUT;
}

[earlydepthstencil]
out_PS ps_shadowDepth( in_PS input )
{
    out_PS OUT;
    return OUT;
}

float ps_shadowResolve( in in_PS_shadow IN) : SV_Target0
{
    int3 positionSS = int3( (int2)( IN.uv * shadowMapSize ), 0 );
    float depthCS = sceneDepthTex.Load( positionSS );
    [branch]
    if( depthCS > 0.99999f )
        discard;


    //float linearDepth = resolveLinearDepth( pixelDepth, reprojectDepthInfo );
    float2 screenPos_m11 = IN.screenPos;

    float4 positionCS = float4( ( ( float2( positionSS.xy ) + 0.5 ) * shadowMapSizeRcp ) * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 ), depthCS, 1.0 );
    //float4 positionCS = float4( screenPos_m11, depthCS, 1.0 );
    float4 positionWS = mul( cameraViewProjInv, positionCS );
    positionWS.xyz *= rcp( positionWS.w );

    //float4 posVS = float4( ( ( float2( positionSS.xy ) + 0.5 ) * render_target_size_rcp ) * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 ), pixelDepth, 1.0 );
    //float3 posVS = resolvePositionVS( screenPos_m11, -linearDepth, reprojectInfo );
    //float4 posWS = mul( cameraWorld, float4(posVS, 1.0) );

    const float normalOffset = 0.05f;
    {
    //    const float3 nrmVS = cross( normalize( ddy_fine( posVS ) ), normalize( ddx_fine( posVS ) ) );
    //    //const float3 nrmVS = normalsVS.SampleLevel( samplNormalsVS, input.uv, 0.0 );
    //    const float3 N = normalize( mul( (float3x3)cameraWorld, nrmVS ) );
        const float3 N = sceneNormalsTex.Load( positionSS ).xyz;
        const float scale = 1.f - saturate( dot( lightDirectionWS.xyz, N ) );
        const float offsetScale = scale * normalOffset;
        const float3 posOffset = N * offsetScale;
        positionWS.xyz += posOffset;
    }

    float4 shadowPos = mul( lightViewProj_01, float4( positionWS.xyz, 1.0 ) );
    const float bias = 0.001f;
    const float offsetU = 0.f;
    float shadowValue = sampleShadowMap_optimizedPCF( shadowPos.xyz, bias, 1.f, offsetU );
    return shadowValue;

}
