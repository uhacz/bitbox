#ifndef SHADOW_DATA
#define SHADOW_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    matrix lightViewProj;
    matrix lightViewProj_01;
    matrix cameraViewProjInv;
    float4 lightDirectionWS;
    float2 depthMapSize;
    float2 depthMapSizeRcp;
    float2 shadowMapSize;
    float2 shadowMapSizeRcp;
};

#endif