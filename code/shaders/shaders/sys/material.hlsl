#ifndef MATERIAL1_HLSL
#define MATERIAL1_HLSL

#define MATERIAL_VARIABLES \
    float3 diffuse_color; \
    float  diffuse; \
    float  specular; \
    float  roughness; \
    float  metallic

#define MATERIAL_TEXTURES \
    texture2D diffuse_tex : register (TSLOT(SLOT_MATERIAL_TEXTURE0)); \
    texture2D specular_tex : register (TSLOT(SLOT_MATERIAL_TEXTURE1)); \
    texture2D roughness_tex : register (TSLOT(SLOT_MATERIAL_TEXTURE2)); \
    texture2D metallic_tex : register (TSLOT(SLOT_MATERIAL_TEXTURE3))

#define MATERIAL_TEXTURES_CPP \
    const char* diffuse_tex; \
    const char* specular_tex; \
    const char* roughness_tex; \
    const char* metallic_tex; \
    MaterialTextures( const char* d, const char* s = nullptr, const char* r = nullptr, const char* m = nullptr ) \
        : diffuse_tex(d), specular_tex( s ), roughness_tex( r ), metallic_tex( m ) {}

#define MATERIAL_DATA_CPP \
    MATERIAL_VARIABLES; \
    MaterialData( float3 dc, float d, float s, float r, float m ) \
        : diffuse_color(dc), diffuse( d ), specular( s ), roughness( r ), metallic( m ) {}

#endif