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
class bxGfxViewFrustum_Tiles;
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
    int cullPointLights( bxGfxLightList* list, bxGfxLight_Point* dstBuffer, int dstBufferSize, bxGfxViewFrustum_Tiles* frustumTiles );

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
struct bxGfxViewFrustumLRBT;
class bxGfxViewFrustum_Tiles
{
public:
    bxGfxViewFrustum_Tiles( bxAllocator* allocator = bxDefaultAllocator() );
    ~bxGfxViewFrustum_Tiles();

    void setup( const Matrix4& viewProjInv, int numTilesX, int numTilesY, int tileSize );
    const bxGfxViewFrustumLRBT& frustum( int tileX, int tileY ) const;

    int numTilesX() const { return _numTilesX; }
    int numTilesY() const { return _numTilesY; }
    int tileSize () const { return _tileSize; }

private:
    Matrix4 _viewProjInv;

    bxAllocator* _allocator;
    void*   _memoryHandle;
    bxGfxViewFrustumLRBT* _frustums;
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
    bxGfxLightList( bxAllocator* allocator = bxDefaultAllocator() );
    ~bxGfxLightList();

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
