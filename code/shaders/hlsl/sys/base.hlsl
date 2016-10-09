#ifndef BASE_HLSL
#define BASE_HLSL

#include <sys/frame_data.hlsl>
#include <sys/vertex_transform.hlsl>
#include <sys/material.hlsl>

//////////////////////////////////////////////////////////////////////////
// system bindings:
// - b0 : frame data
// - b1 : instance offset
// - b2 : lightning data
// - b3 : material data
// - t0 : instance data (world matrices)
// - t1 : instance data (worldIT matrices)

// - t2 : lights data 
// - t3 : lights indices 

// - t4-t7 : material textures -> forward
// - t2-t5 : material textures -> deffered

////
shared
cbuffer LightningData : register(BSLOT(SLOT_LIGHTNING_DATA))
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

Buffer<float4> _lightsData : register(TSLOT(SLOT_LIGHTS_DATA) );
Buffer<uint> _lightsIndices : register(TSLOT(SLOT_LIGHTS_INDICES) );

////
struct ShadingData
{
    float3 V;
    float3 N;
    float shadow;
    float ssao;
};

#endif