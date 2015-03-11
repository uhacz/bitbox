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
struct bxGdiRenderSource;
struct bxGdiShaderFx_Instance;


struct bxMeshComponent_Instance 
{ 
    u32 i; 
};

struct bxMeshComponent_Matrix
{
    Matrix4* data;
    u16 n;
};

struct bxMeshComponentManager
{
    bxMeshComponent_Instance create( bxEntity e );
    bxMeshComponent_Instance lookup( bxEntity e );
    void release( bxMeshComponent_Instance* i );
    void gc( const bxEntityManager& em );


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
        bxGdiRenderSource**      rsource;
        bxGdiShaderFx_Instance** fxI;
        bxGdiRenderSurface*      surface;
        bxAABB*                  localAABB;
        bxMeshComponent_Matrix   matrix;
        i32                      n;
    } _data;

private:
    void _Allocate( int n );

    bxAllocator* _alloc_singleMatrix;
    bxAllocator* _alloc_multipleMatrix;

    void* _memoryHandle;
    bxAllocator* _alloc_main;
};
