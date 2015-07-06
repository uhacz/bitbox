#include "entity.h"
#include <util/array.h>
#include <util/array_util.h>
#include <util/queue.h>
#include <util/hashmap.h>
#include <util/debug.h>
#include <util/random.h>
#include <util/pool_allocator.h>
#include <util/buffer_utils.h>
#include <gdi/gdi_render_source.h>
#include <gfx/gfx_type.h>
#include <string.h>

bxEntity bxEntity_Manager::create()
{
    u32 idx;
    if( queue::size( _freeIndeices ) > eMINIMUM_FREE_INDICES )
    {
        idx = queue::front( _freeIndeices );
        queue::pop_front( _freeIndeices );
    }
    else
    {
        idx = array::push_back( _generation, u8(1) );
        SYS_ASSERT( idx < (1 << bxEntity::eINDEX_BITS) );
    }

    bxEntity e;
    e.generation = _generation[idx];
    e.index = idx;

    return e;
}

void bxEntity_Manager::release( bxEntity* e )
{
    for( int icb = 0; icb < array::size( _callback_releaseEntity ); ++icb )
    {
        Callback cb = _callback_releaseEntity[icb];
        bxEntity_releaseCallback* ptr = (bxEntity_releaseCallback*)( cb.ptr );
        (*ptr)( *e, cb.userData );
    }
    
    u32 idx = e->index;
    e->hash = 0;

    ++_generation[idx];
    queue::push_back( _freeIndeices, idx );
}

bxEntity_Manager::bxEntity_Manager()
{

}

bxEntity_Manager::~bxEntity_Manager()
{

}

void bxEntity_Manager::register_releaseCallback( bxEntity_releaseCallback* cb, void* userData )
{
    struct Cmp{
        
        Callback _cb;
        Cmp( Callback cb ): _cb(cb) {} 

        bool operator() ( const Callback& a ){
            return a.ptr == _cb.ptr && a.userData == _cb.userData;
        }
    };    

    Callback entry;
    entry.ptr = cb;
    entry.userData = userData;

    const int foundIndex = array::find1( array::begin( _callback_releaseEntity ), array::end( _callback_releaseEntity ), Cmp(entry) );
    if( foundIndex != -1 )
    {
        bxLogWarning( "Callback (0x%p) already registered", cb );
        return;
    }
    array::push_back( _callback_releaseEntity, entry );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
