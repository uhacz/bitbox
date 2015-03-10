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
    void gc();

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