#ifndef BINDING_MAP_H
#define BINDING_MAP_H


#define SLOT_FRAME_DATA 0
#define SLOT_INSTANCE_OFFSET 1
#define SLOT_LIGHTNING_DATA 2
#define SLOT_MATERIAL_DATA 3

//world matrices (vertex shader)
#define SLOT_INSTANCE_DATA_WORLD 0 
#define SLOT_INSTANCE_DATA_WORLD_IT 1
#define SLOT_INSTANCE_PARTICLE_DATA 2

//pixel shader
#define SLOT_LIGHTS_DATA 0
#define SLOT_LIGHTS_INDICES 1

#define SLOT_MATERIAL_TEXTURE0 2
#define SLOT_MATERIAL_TEXTURE1 3
#define SLOT_MATERIAL_TEXTURE2 4
#define SLOT_MATERIAL_TEXTURE3 5
#define SLOT_MATERIAL_TEXTURE_COUNT 4

#define BSLOT( slot ) b##slot
#define TSLOT( slot ) t##slot
#define SSLOT( slot ) s##slot

#ifndef __cplusplus
#define TREGISTER( slot ) : register( TSLOT( slot ) )
#define BREGISTER( slot ) : register( BSLOT( slot ) )
#define CBUFFER shared cbuffer

#else

#include <util/vectormath/vectormath.h>
typedef float2_t float2;
typedef float3_t float3;
typedef float4_t float4;
typedef u32 uint;
typedef Matrix4 matrix;



#define TREGISTER( slot ) = {}
#define BREGISTER( slot )
#define CBUFFER struct
#endif


#endif