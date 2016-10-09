#ifndef MATERIAL_HLSL
#define MATERIAL_HLSL

#define MATERIAL_VARIABLES \
    float3 diffuseColor;   \
    float3 fresnelColor;   \
    float3 ambientColor;   \
    float  diffuseCoeff;   \
    float  roughnessCoeff; \
    float  specularCoeff;  \
    float  ambientCoeff

struct Material
{
    MATERIAL_VARIABLES;
};

#define ASSIGN_MATERIAL_FROM_CBUFFER( dst ) \
    dst.diffuseColor = diffuseColor;   \
    dst.fresnelColor = fresnelColor;   \
    dst.ambientColor = ambientColor;   \
    dst.diffuseCoeff = diffuseCoeff;   \
    dst.roughnessCoeff = roughnessCoeff; \
    dst.specularCoeff = specularCoeff;   \
    dst.ambientCoeff = ambientCoeff

#endif
