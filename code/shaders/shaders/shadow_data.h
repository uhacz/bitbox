#ifndef SHADOW_DATA
#define SHADOW_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    matrix cameraViewProjInv;
    matrix lightViewProj;
    float4 lightDirectionWS;
    float2 shadowMapSize;
    float2 shadowMapSizeRcp;
};

#endif