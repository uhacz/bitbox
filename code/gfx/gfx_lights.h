#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/handle_manager.h>
#include <gdi/gdi_backend.h>
#include <gdi/gdi_context.h>

namespace bxGfx
{
    struct LightningData
    {
        u32 numTilesX;
        u32 numTilesY;
        u32 numTiles;
        u32 tileSize;
        u32 maxLights;
        f32 tileSizeRcp;
        
        f32 sunAngularRadius;
        f32 sunIlluminanceInLux;
        f32 skyIlluminanceInLux;

        float3_t sunDirection;
        float3_t sunColor;
        u32 padding__[1];
        float3_t skyColor;
    };
}///

struct bxGfxLight_Point
{
    float3_t position;
    f32 radius;
    float3_t color;
    f32 intensity;
};

struct bxGfxLight_Sun
{
    float3_t dir;
    float3_t sunColor;
    float3_t skyColor;
    f32 sunIlluminance;
    f32 skyIlluminance;
};

class bxGfxLightList;
class bxGfxViewFrustum_Tiles;
struct bxGfxCamera;
class bxGfxLightManager
{
public:
    struct PointInstance{ u32 id; };
    struct SpotInstance { u32 id; };

public:
    bxGfxLightManager();

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
    int cullPointLights( bxGfxLightList* list, bxGfxLight_Point* dstBuffer, int dstBufferSize, const bxGfx::LightningData& lightData, const bxGfxCamera& camera );

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
struct bxGfxViewFrustum_LRBT;
class bxGfxViewFrustum_Tiles
{
public:
    bxGfxViewFrustum_Tiles( bxAllocator* allocator = bxDefaultAllocator() );
    ~bxGfxViewFrustum_Tiles();

    void setup( const Matrix4& viewProjInv, int numTilesX, int numTilesY, int tileSize );
    const bxGfxViewFrustum_LRBT& frustum( int tileX, int tileY ) const;

    int numTilesX() const { return _numTilesX; }
    int numTilesY() const { return _numTilesY; }
    int tileSize () const { return _tileSize; }

private:
    Matrix4 _viewProjInv;

    bxAllocator* _allocator;
    void*   _memoryHandle;
    bxGfxViewFrustum_LRBT* _frustums;
    u8*     _validFlags;

    i32 _numTilesX;
    i32 _numTilesY;
    i32 _tileSize;
    i32 _memorySize;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class bxGfxLightList
{
public:
    bxGfxLightList();
    ~bxGfxLightList();

    void startup( bxAllocator* allocator = bxDefaultAllocator() );
    void shutdown();

    void setup( int nTilesX, int nTilesY, int nLights );

    int appendPointLight( int tileX, int tileY, int lightIndex );

    const u32* items() const { return _items; }
    const u32* tiles() const { return _tiles; }

private:
    union Item
    {
        u32 hash;
        struct  
        {
            u16 index_pointLight;
            u16 index_spotLight;
        };
    };

    union Tile
    {
        u32 hash;
        struct
        {
            u16 count_pointLight;
            u16 count_spotLight;
        };
    };
    
    bxAllocator* _allocator;
    void* _memoryHandle;
    
    u32* _tiles; // 2D grid of tiles
    u32* _items; // 1D list of lights for each tile
    i32 _numTilesX;
    i32 _numTilesY;
    i32 _numLights;
    i32 _memorySize;
};

////
////
struct bxGfxLights
{
    bxGfxLightManager lightManager;
    bxGfxLightList lightList;

    bxGfx::LightningData data;

    bxGdiBuffer buffer_lightsData;
    bxGdiBuffer buffer_lightsTileIndices;
    bxGdiBuffer cbuffer_lightningData;

    bxGfxLight_Point* culledPointLightsBuffer;

    void _Startup( bxGdiDeviceBackend* dev, int maxLights, int tileSize, int rtWidth, int rtHeight, bxAllocator* allocator = bxDefaultAllocator() );
    void _Shutdown( bxGdiDeviceBackend* dev );

    void setSunDir( const Vector3& dir );
    void setSunColor( const float3_t& rgb );
    void setSkyColor( const float3_t& rgb );
    void setSunIlluminance( float lux );
    void setSkyIlluminance( float lux );

    bxGfxLight_Sun sunLight() const;
    
    void cullLights( const bxGfxCamera& camera );
    void bind( bxGdiContext* ctx );
};

struct bxGfxLightsGUI
{
    u32 flag_isVisible;

    bxGfxLightsGUI();
    void show( bxGfxLights* lights, const bxGfxLightManager::PointInstance* instances, int nInstances );
};


