#ifndef BASE_HLSL
#define BASE_HLSL

#include <sys/frame_data.hlsl>
#include <sys/vertex_transform.hlsl>
#include <sys/material.hlsl>

////
shared cbuffer LighningData : register( b2 )
{
    uint2 _numTilesXY;
    uint  _numTiles;
    uint  _tileSize;
    uint  _maxLights;
    float _tileSizeRcp;

    float _sunAngularRadius;
    float _sunIlluminanceInLux;
    float _skyIlluminanceInLux;

    float3 _sunDirection;
    //float3 _sunColor;
    //float3 _skyColor;
};

Buffer<float4> _lightsData    : register( t2 );
Buffer<uint>   _lightsIndices : register( t3 );

////
struct ShadingData
{
    float3 V;
    float3 N;
    float shadow;
    float ssao;
};

#endif