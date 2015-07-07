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

namespace bxEntity
{
    
}///
