#pragma once

#include <util/containers.h>

union bxEntity
{
    enum
    {
        eINDEX_BITS = 24,
        eGENERATION_BITS = 8,
    };
    
    u32 hash;
    struct
    {
        u32 index : eINDEX_BITS;
        u32 generation : eGENERATION_BITS;
    };
};
inline bool operator == (const bxEntity ea, const bxEntity eb) { 
    return ea.hash == eb.hash; 
}
inline bool operator != (const bxEntity ea, const bxEntity eb) { 
    return ea.hash != eb.hash; 
}
inline bxEntity bxEntity_null() {
    bxEntity e = { 0 }; return e;
}

struct bxEntityManager
{
    bxEntity create();
    void release( bxEntity* e );

    bool alive( bxEntity e ) const { 
        return _generation[e.index] == e.generation;
    }

private:
    enum 
    {
        eMINIMUM_FREE_INDICES = 1024,
    };

    array_t<u8> _generation;
    queue_t<u32> _freeIndeices;
};


#include <util/vectormath/vectormath.h>
#include <util/bbox.h>
struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;
struct bxGdiRenderSurface;

struct bxMeshComponent_Instance 
{ 
    u32 i;
};

struct bxMeshComponent_Matrix
{
    Matrix4* data;
    i32 n;
};

struct bxMeshComponentManager
{
    bxMeshComponent_Instance create( bxEntity e, int nMatrices );
    void release( bxMeshComponent_Instance i );
    bxMeshComponent_Instance lookup( bxEntity e );
    
    bxGdiRenderSource* renderSource( bxMeshComponent_Instance i );
    void setRenderSource( bxMeshComponent_Instance i, bxGdiRenderSource* rsource );

    bxGdiShaderFx_Instance shader( bxMeshComponent_Instance i );
    void setShader( bxMeshComponent_Instance i, bxGdiShaderFx_Instance* fxI );

    bxAABB localAABB( bxMeshComponent_Instance i );
    void setLocalAABB( bxMeshComponent_Instance i, const bxAABB& aabb );

    bxMeshComponent_Matrix matrix( bxMeshComponent_Instance i );
    Matrix4 matrix( bxMeshComponent_Instance i, int index );
    void setMatrix( bxMeshComponent_Instance i, int index, const Matrix4& mx );

public:
    struct InstanceData
    {
        bxEntity*                entity;
        bxGdiRenderSource**      rsource;
        bxGdiShaderFx_Instance** fxI;
        bxGdiRenderSurface*      surface;
        bxAABB*                  localAABB;
        bxMeshComponent_Matrix*  matrix;
        i32                      n;
        i32                      capacity;
    };
    const InstanceData& data() const { return _data; }
    void gc( const bxEntityManager& em );
    bxMeshComponentManager();

private:
    void _SetDefaults();
    void _Allocate( int n );

    bxAllocator* _alloc_singleMatrix;
    bxAllocator* _alloc_multipleMatrix;
    bxAllocator* _alloc_main;

    void* _memoryHandle;
    InstanceData _data;
    hashmap_t _entityMap;
};
