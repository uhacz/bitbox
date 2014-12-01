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
    union Instance
    {
        u32 id;
        struct
        {
            u32 type  : 4;
            u32 magic : 16;
            u32 index : 12;
        };
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
    inline int hasPointLight( Instance i ) const{
        const Index& idx = _pointLight_indices[i.index];
        return idx.i.magic == i.magic && idx.index != 0xFFFF;
    }


private:
    struct Index
    {
        Instance i;
        u16 index;
        u16 next;
    };

    enum
    {
        eTYPE_POINT = 0,
    };

    void* _memoryHandle;
    Index*    _pointLight_indices;
    Vector4*  _pointLight_position_radius;
    Vector4*  _pointLight_color_intensity;

    i32 _count_pointLights;
    i32 _capacity_lights;

    u16 _pointLight_freelistEnqueue;
    u16 _pointLight_freelistDequeue;
};
