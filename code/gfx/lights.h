#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

struct bxGfxLight_Point
{
    float3_t position;
    f32 radius;
    float3_t color;
    f32 intensity;
};

class bxGfxLights
{
public:
    struct Instance
    {
        u32 id;
    };

public:
    bxGfxLights();

    void startup( int maxLights );
    void shutdown();

    Instance createPointLight( const bxGfxLight_Point& light );
    Instance createSpotLight( /*not implemented*/ );
    void releaseLight( Instance i );

    bxGfxLight_Point pointLight( Instance i );
    void setPointLight( Instance i, const bxGfxLight_Point& light );

private:
    void* _memoryHandle;
    Vector4* _pointLight_position_radius;
    Vector4* _pointLight_color_intensity;

    i32 _count_pointLights;
    i32 _capacity_lights;
};
