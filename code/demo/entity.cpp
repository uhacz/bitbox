#include "entity.h"
#include <util/array.h>
#include <util/queue.h>
#include <util/debug.h>

bxEntity bxEntityManager::create()
{
    u32 idx;
    if( queue::size( _freeIndeices ) > eMINIMUM_FREE_INDICES )
    {
        idx = queue::front( _freeIndeices );
        queue::pop_front( _freeIndeices );
    }
    else
    {
        idx = array::push_back( _generation, u8(0) );
        SYS_ASSERT( idx < (1 << bxEntity::eINDEX_BITS) );
    }

    bxEntity e;
    e.generation = _generation[idx];
    e.index = idx;

    return e;
}

void bxEntityManager::release( bxEntity* e )
{
    u32 idx = e->index;
    ++_generation[idx];
    queue::push_back( _freeIndeices, idx );
    e->hash = 0;
}
