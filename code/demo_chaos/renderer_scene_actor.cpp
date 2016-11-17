#include "renderer_scene_actor.h"

namespace bx{ namespace gfx{



    void ActorHandleManager::StartUp()
    {
        _free_indices_ring.init( _free_indices, eMAX_COUNT );
        for( u32 i = 0; i < eMAX_COUNT; ++i )
        {
            _free_indices[i] = UINT32_MAX;
            _generation[i] = 1;
            _scene[i] = nullptr;
            _data_index[i] = UINT32_MAX;
        }
    }

    void ActorHandleManager::ShutDown()
    {

    }

    bx::gfx::ActorID ActorHandleManager::acquire()
    {
        static_assert( sizeof( ActorID ) == sizeof( ActorHandle ), "Handle mismatch" );

        _lock.lock();
        u32 idx;

        //if( _num_free_indices == eMINIMUM_FREE_INDICES )
        if( _free_indices_ring.size() > eMINIMUM_FREE_INDICES )
        {
            //idx = queue::front( _free_indices );
            //queue::pop_front( _free_indices );
            bool bres = _free_indices_ring.pop_front( &idx );
            SYS_ASSERT( bres );
        }
        else
        {
            //idx = array::push_back( _generation, u8( 1 ) );
            //array::push_back( _scene, (Scene)nullptr );
            //array::push_back( _data_index, UINT32_MAX );
            //SYS_ASSERT( idx < ( 1 << MeshHandle::eINDEX_BITS ) );

            idx = _size++;
            _generation[idx] = 1;
            _scene[idx] = nullptr;
            _data_index[idx] = UINT32_MAX;
            SYS_ASSERT( idx < ( 1 << ActorHandle::eINDEX_BITS ) );
        }

        _lock.unlock();

        SYS_ASSERT( idx < eMAX_COUNT );
        SYS_ASSERT( _scene[idx] == nullptr );
        SYS_ASSERT( _data_index[idx] == UINT32_MAX );

        ActorHandle h;
        h.generation = _generation[idx];
        h.index = idx;
        return makeMeshID( h.hash );
    }

    void ActorHandleManager::release( ActorID* mi )
    {
        ActorHandle* h = (ActorHandle*)mi;
        u32 idx = h->index;
        h->hash = 0;

        ++_generation[idx];
        _scene[idx] = nullptr;
        _data_index[idx] = UINT32_MAX;

        _lock.lock();
        bool bres = _free_indices_ring.push_back( idx );
        //queue::push_back( _free_indices, idx );
        _lock.unlock();
        SYS_ASSERT( bres );
    }

}}///
