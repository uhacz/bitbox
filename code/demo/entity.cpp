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

bxEntity_Id bxEntity_Manager::create()
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
        SYS_ASSERT( idx < (1 << bxEntity_Id::eINDEX_BITS) );
    }

    bxEntity_Id e;
    e.generation = _generation[idx];
    e.index = idx;

    return e;
}

void bxEntity_Manager::release( bxEntity_Id* e )
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

void bxEntity_ComponentMap::add( bxEntity_Id eid, u32 component )
{
    hashmap_t::cell_t* cell = hashmap::lookup( _map, eid.hash );
    if( !cell )
    {
        cell = hashmap::insert( _map, eid.hash );
        bxEntity_ComponentItem empty = { 0 };
        empty.next = -1;
        cell->value = empty.hash;
    }
    
    bxEntity_ComponentItem item = { cell->value };
    if( item.handle == 0 )
    {
        item.handle = component;
        item.next = -1;
        cell->value = item.hash;
    }
    else
    {
        bxEntity_ComponentItem newItem;
        newItem.handle = component;
        newItem.next = item.next;
        item.next = array::push_back( _items, newItem );
    }
}

void bxEntity_ComponentMap::remove( bxEntity_Id eid, u32 component )
{
    hashmap_t::cell_t* cell = hashmap::lookup( _map, eid.hash );
    if( !cell )
    {
        bxLogError( "entity not found" );
        return;
    }

    aaa
    bxEntity_ComponentItem item = { cell->value };
    while( item.handle )
    {
        if( item.handle == component )
        {
            
            break;
        }

        if( item.next == -1 )
            break;

        item = _items[item.next];
    }
    
}

bxEntity_ComponentItem bxEntity_ComponentMap::begin( bxEntity_Id eid )
{

}

bxEntity_ComponentItem bxEntity_ComponentMap::next( bxEntity_Id eid, bxEntity_ComponentItem current )
{

}
