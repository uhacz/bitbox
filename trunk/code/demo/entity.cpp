#include "entity.h"
#include <util/array.h>
#include <util/

bxEntity bxEntityManager::create()
{
    u32 idx;
    if( _freeIndeices.size() > eMINIMUM_FREE_INDICES )
    {
        idx = array::front( _freeIndeices );
        array::pop_front( _freeIndeices );
    }
    else
    {
        idx = array::push_back( _generation, 0 );
        SYS_ASSERT( idx < (1 << bxEntity::eINDEX_BITS) );
    }

    bxEntity e;
    e.generation = _generation[idx];
    e.index = idx;

    return e;
}

void bxEntityManager::release( bxEntity* e )
{
    const u32 idx = e->index;
    ++_generation[idx];
    queue::push_back( _freeIndeices, idx );
}
