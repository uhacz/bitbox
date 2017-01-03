#ifndef SSAO_DATA
#define SSAO_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    matrix g_ViewMatrix;
    float4 g_ReprojectInfoHalfResFromInt;
    float2 g_renderTargetSize;
    float g_SSAOPhase;
};

#endif