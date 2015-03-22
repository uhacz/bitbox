#pragma once

#include <util/containers.h>
#include <util/tag.h>

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

struct bxEntity_Manager;

typedef void bxEntity_releaseCallback( bxEntity entity, void* userData );
typedef void bxEntity_garbageCollectorCallback( const bxEntity_Manager& em );

struct bxEntity_Manager
{
    bxEntity create();
    void release( bxEntity* e );

    bool alive( bxEntity e ) const { 
        return _generation[e.index] == e.generation;
    }

    bxEntity_Manager();
    ~bxEntity_Manager();

    void register_releaseCallback( bxEntity_releaseCallback* cb, void* userData );
    void register_garbageCollectorCallback( bxEntity_garbageCollectorCallback* cb, void* userData );

    void _GarbageCollector();
private:
    enum 
    {
        eMINIMUM_FREE_INDICES = 1024,
    };

    array_t<u8> _generation;
    queue_t<u32> _freeIndeices;

    struct Callback
    {
        void* ptr;
        void* userData;
    };
    array_t<Callback> _callback_releaseEntity;
    array_t<Callback> _callback_garbageCollector;

};
////
////


////
////


#include <util/vectormath/vectormath.h>
#include <util/bbox.h>
#include <util/handle_manager.h>
#include <gdi/gdi_render_source.h>
struct bxGdiShaderFx_Instance;

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


struct bxComponent_MatrixAllocator
{
    bxComponent_Matrix alloc( int nInstances );
    void free( bxComponent_Matrix* mx );

    bxComponent_MatrixAllocator();
    void _Startup( bxAllocator* allocMain );
    void _Shutdown();

private:
    bxAllocator* _alloc_main;
    bxAllocator* _alloc_singleMatrix;
    bxAllocator* _alloc_multipleMatrix;
};
///
///

struct bxMeshComponent_Instance 
{ 
    u32 i;
};

struct bxMeshComponent_Data
{
    bxGdiRenderSource* rsource;
    bxGdiShaderFx_Instance* shader;
    bxGdiRenderSurface surface;

    u32 passIndex : 16;
    u32 mask : 8;
    u32 layer : 8;
};

struct bxMeshComponent_Manager
{
    bxMeshComponent_Instance create( bxEntity e, int nInstances );
    void release( bxMeshComponent_Instance i );
    bxMeshComponent_Instance lookup( bxEntity e );
    
    bxMeshComponent_Data mesh( bxMeshComponent_Instance i );
    void setMesh( bxMeshComponent_Instance i, const bxMeshComponent_Data data );

    bxComponent_Matrix matrix( bxMeshComponent_Instance i );
    
    bxAABB localAABB( bxMeshComponent_Instance i );
    void setLocalAABB( bxMeshComponent_Instance i, const bxAABB& aabb );
    
public:
    /// TODO : multiple components per entity
    //struct InstanceChain 
    //{
    //    bxEntity* entity;
    //    bxMeshComponent_Instance nextInstance;
    //};


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

    bxMeshComponent_Manager();
    void _Startup( bxAllocator* alloc = bxDefaultAllocator() );
    void _Shutdown();

    static void _Callback_releaseEntity( bxEntity e, void* userData );

private:
    void _Allocate( int n );
    
    typedef bxHandleManager<i32> InstanceIndices;
    int _GetIndex( bxMeshComponent_Instance i ){
        InstanceIndices::Handle h( i.i );
        SYS_ASSERT( _indices.isValid( h ) );
        const int index = _indices.get( h );
        SYS_ASSERT( index < _data.size );
        return index;
    }

    bxAllocator* _alloc;
    bxComponent_MatrixAllocator _alloc_matrix;
    void* _memoryHandle;
    InstanceData _data;
    InstanceIndices _indices;
    hashmap_t _entityMap;
};

struct bxGfxRenderList;
namespace bxComponent
{
    void mesh_createRenderList( bxGfxRenderList* rList, const bxMeshComponent_Manager& meshManager );
}///
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct bxSceneComponent
{
    struct Instance
    {
        u32 i;
    };


    Instance create( bxEntity e, int nMatrices );
    void release( Instance i );
    Instance lookup( bxEntity e );

    const Matrix4& local( Instance i ) const;
    const Matrix4& world( Instance i ) const;

    void setLocal( Instance i, const Matrix4& value );
    void setWorld( Instance i, const Matrix4& value );
        
    void link( Instance parent, Instance child );
    void unlink( Instance child );

private:
    enum
    {
        eFLAG_DIRTY = 0x1,
    };
    
    struct InstanceData
    {
        bxEntity* entity;
        Matrix4* local;
        Matrix4* world;
        u8* flags;
        i32 size;
        i32 capacity;
    };

    void _Allocate( int newCapacity );
    bxAllocator* _alloc;

    void* _memoryHandle;
    InstanceData _data;
    hashmap_t _entityMap;

};


