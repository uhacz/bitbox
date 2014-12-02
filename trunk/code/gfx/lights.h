#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/handle_manager.h>

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
    struct PointInstance{ u32 id; };
    struct SpotInstance { u32 id; };

public:
    bxGfxLights();

    void startup( int maxLights );
    void shutdown();

    PointInstance createPointLight( const bxGfxLight_Point& light );
    SpotInstance createSpotLight( /*not implemented*/ );
    void releaseLight( PointInstance i );

    
    bxGfxLight_Point pointLight( PointInstance i );
    void setPointLight( PointInstance i, const bxGfxLight_Point& light );
    inline int hasPointLight( PointInstance i ) const{
        return _pointLight_indices.isValid( Indices::Handle( i.id ) );
    }


private:
    typedef bxHandleManager<u32> Indices;

    Indices _pointLight_indices;
    void* _memoryHandle;
    Vector4*  _pointLight_position_radius;
    Vector4*  _pointLight_color_intensity;

    u32 _count_pointLights;
    u32 _capacity_lights;
};
