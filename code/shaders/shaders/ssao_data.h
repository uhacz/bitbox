#ifndef SSAO_DATA
#define SSAO_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    matrix g_ViewMatrix;
    float4 g_ProjectionParams;
    float4 g_ReprojectInfoFromInt;
    float2 g_reprojectionDepth;
    float2 g_renderTargetSize;
    float2 g_radiusWS; // x: radius, y: radius^2
    float g_SSAOPhase;
    
};

#endif