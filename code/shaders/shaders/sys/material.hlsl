#ifndef MATERIAL1_HLSL
#define MATERIAL1_HLSL


#define MATERIAL_VARIABLES \
    float3 diffuse_color; \
    float  diffuse; \
    float  specular; \
    float  roughness; \
    float  metallic

#define _MATERIAL_TEXTURES( type ) \
    type diffuse_tex   TREGISTER( SLOT_MATERIAL_TEXTURE0 ); \
    type specular_tex  TREGISTER( SLOT_MATERIAL_TEXTURE1 ); \
    type roughness_tex TREGISTER( SLOT_MATERIAL_TEXTURE2 ); \
    type metallic_tex  TREGISTER( SLOT_MATERIAL_TEXTURE3 )

#define MATERIAL_TEXTURES _MATERIAL_TEXTURES( texture2D )

#ifdef BX_CPP
    #define MATERIAL_TEXTURES_CPP _MATERIAL_TEXTURES( const char* )
    #define MATERIAL_TEXTURE_RESOURCES_CPP _MATERIAL_TEXTURES( bx::ResourceID )
    #define MATERIAL_TEXTURE_OBJECTS_CPP _MATERIAL_TEXTURES( bx::rdi::TextureRO )

#define MATERIAL_DATA_CPP \
    MATERIAL_VARIABLES; \
    MaterialData() {} \
    MaterialData( float3 dc, float d, float s, float r, float m ) \
        : diffuse_color(dc), diffuse( d ), specular( s ), roughness( r ), metallic( m ) {}

#endif

#endif