#ifndef SHADOW_DATA
#define SHADOW_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    matrix cameraWorld;
    matrix lightViewProj;
    float4 lightDirectionWS;
    float2 shadowMapSize;
    float2 reprojectInfo;
    float2 reprojectDepthInfo; // x: scale, y: bias
};

#endif