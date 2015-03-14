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

struct bxComponent_Transform
{
    Matrix3* rotation;
    Vector3* position;
    Vector3* scale;
    i32 n;
};
struct bxComponent_Matrix
{
    Matrix4* pose;
    i32 n;
};

struct bxMeshComponent_Instance 
{ 
    u32 i;
};

struct bxMeshComponent_Data
{
    bxGdiRenderSource* rsource;
    bxGdiShaderFx_Instance* shader;
    bxGdiRenderSurface surface;
};

struct bxMeshComponent_Manager
{
    bxMeshComponent_Instance create( bxEntity e );
    void release( bxMeshComponent_Instance i );
    bxMeshComponent_Instance lookup( bxEntity e );
    
    bxMeshComponent_Data* mesh( bxMeshComponent_Instance i );
    void setMesh( bxMeshComponent_Instance i, bxGdiRenderSource* rsource, bxGdiRenderSurface surf );

    bxComponent_Matrix matrix( bxGraphComponent_Instance i );
    void setMatrix( bxGraphComponent_Instance i, const Matrix4* mx, int count, int offset = 0 );

    bxAABB localAABB( bxMeshComponent_Instance i );
    void setLocalAABB( bxMeshComponent_Instance i, const bxAABB& aabb );
    
public:
    struct InstanceData
    {
        bxEntity*                 entity;
        bxMeshComponent_Data*     mesh;
        bxComponent_Matrix*       matrix;
        bxAABB*                   localAABB;
        i32                       size;
        i32                       capacity;
    };
    const InstanceData& data() const { return _data; }
    void gc( const bxEntityManager& em );
    bxMeshComponent_Manager();

private:
    void _Allocate( int n );

    bxAllocator* _alloc_main;
    void* _memoryHandle;
    InstanceData _data;
    hashmap_t _entityMap;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct bxGraphComponent_Instance
{
    u32 i;
};


struct bxGraphComponent_Manager
{
    bxGraphComponent_Instance create( bxEntity e, int nMatrices );
    void release( bxGraphComponent_Instance i );
    bxGraphComponent_Instance lookup( bxEntity e );

    bxComponent_Matrix matrix( bxGraphComponent_Instance i );
    Matrix4 matrix( bxGraphComponent_Instance i, int index );
    void setMatrix( bxGraphComponent_Instance i, int index, const Matrix4& mx );

private:
    struct InstanceData
    {
        bxEntity* entity;
        bxComponent_Matrix* matrix;
        bxComponent_Transform* transform;

        i32 size;
        i32 capacity;
    };

    void _SetDefaults();
    void _Allocate( int newCapacity );

    bxAllocator* _alloc_singleMatrix;
    bxAllocator* _alloc_multipleMatrix;
    bxAllocator* _alloc_main;

    void* _memoryHandle;
    InstanceData _data;
    hashmap_t _entityMap;

};


