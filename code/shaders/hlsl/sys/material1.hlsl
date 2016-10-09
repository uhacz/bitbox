#ifndef MATERIAL1_HLSL
#define MATERIAL1_HLSL

#define MATERIAL_VARIABLES \
    float3 diffuse_color; \
    float  diffuse; \
    float  specular; \
    float  roughness; \
    float  metallic

#define MATERIAL_TEXTURES \
    texture2D diffuse_tex; \
    texture2D specular_tex; \
    texture2D roughness_tex; \
    texture2D metallic_tex

#endif