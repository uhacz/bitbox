#pragma once

#include <util/containers.h>
#include <util/tag.h>

union bxEntity_Id
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
inline bool operator == (const bxEntity_Id ea, const bxEntity_Id eb) 
{ 
    return ea.hash == eb.hash; 
}
inline bool operator != (const bxEntity_Id ea, const bxEntity_Id eb) 
{ 
    return ea.hash != eb.hash; 
}
inline bxEntity_Id bxEntity_null() 
{
    bxEntity_Id e = { 0 }; return e;
}

struct bxEntity_Manager;
typedef void bxEntity_releaseCallback( bxEntity_Id entity, void* userData );
typedef void bxEntity_garbageCollectorCallback( const bxEntity_Manager& em );

struct bxEntity_Manager
{
    bxEntity_Id create();
    void release( bxEntity_Id* e );

    bool alive( bxEntity_Id e ) const { 
        return _generation[e.index] == e.generation;
    }

    bxEntity_Manager();
    ~bxEntity_Manager();

    void register_releaseCallback( bxEntity_releaseCallback* cb, void* userData );
private:
    enum 
    {
        eMINIMUM_FREE_INDICES = 1024,
    };
    
    struct Callback
    {
        void* ptr;
        void* userData;
    };

    array_t<u8> _generation;
    queue_t<u32> _freeIndeices;

    array_t<Callback> _callback_releaseEntity;
};
////
////

union bxEntity_ComponentItem
{
    u64 hash;
    struct  
    {
        u32 handle;
        i32 next;
    };
};

struct bxEntity_ComponentMap
{
    void add( bxEntity_Id eid, u32 component );
    void remove( bxEntity_Id eid, u32 component );
    
    bxEntity_ComponentItem begin( bxEntity_Id eid );
    bxEntity_ComponentItem next( bxEntity_Id eid, bxEntity_ComponentItem current );

private:
    hashmap_t _map;
    array_t<bxEntity_ComponentItem> _items;
};
