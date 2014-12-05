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

class bxGfxLightList;
struct bxGfxCamera;
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

    int maxLights() const { return _capacity_lights; }
    int countPointLights() const { return _count_pointLights; }
    int cullPointLights( bxGfxLightList* list, bxGfxLight_Point* dstBuffer, int dstBufferSize, const bxGfxCamera& camera );

private:
    typedef bxHandleManager<u32> Indices;

    Indices   _pointLight_indices;
    void*     _memoryHandle;
    Vector4*  _pointLight_position_radius;
    Vector4*  _pointLight_color_intensity;

    u32 _count_pointLights;
    u32 _capacity_lights;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class bxGfxLightList
{
public:
    void startup( int nTilesX, int nTilesY, int nLights, bxAllocator* allocator );
    void shutdown( bxAllocator* allocator );

    void clear();

    int appendLight( int tileX, int tileY, int lightIndex );

    const u32* items() const { return _items; }
    const u32* tiles() const { return _tiles; }

private:
    union Item
    {
        u32 hash;
        struct  
        {
            u32 lightIndex;
        };
    };

    union Tile
    {
        u32 hash;
        struct
        {
            u16 itemIndex;
            u16 itemCount;
        };
    };

    u32* _tiles; // 2D grid of tiles
    u32* _items; // 1D list of lights for each tile
    i32 _numItems;
    i32 _numTilesX;
    i32 _numTilesY;
};
