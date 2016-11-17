#pragma once

#include <util/type.h>
#include <util/ring_buffer.h>
#include <util/thread/mutex.h>
#include <util/debug.h>

#include "renderer_type.h"

namespace bx{ namespace gfx{

static inline ActorID makeMeshID( u32 hash )
{
    ActorID mi = {};
    mi.i = hash;
    return mi;
}
static inline ActorID makeInvalidMeshID()
{
    return makeMeshID( 0 );
}


union ActorHandle
{
    enum
    {
        eINDEX_BITS = 24,
        eGENERATION_BITS = 8,
    };

    u32 hash = 0;
    struct
    {
        u32 index : eINDEX_BITS;
        u32 generation : eGENERATION_BITS;
    };
    ActorHandle() {}
    ActorHandle( u32 h ) : hash( h ) {}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct ActorHandleManager
{
    enum
    {
        eMINIMUM_FREE_INDICES = 1024,
        eMAX_COUNT = 1024 * 8,
    };

    RingArray<u32> _free_indices_ring;
    u32     _free_indices[eMAX_COUNT] = {};
    u8      _generation[eMAX_COUNT] = {};
    Scene   _scene[eMAX_COUNT] = {};
    u32     _data_index[eMAX_COUNT] = {};

    //u32 _num_free_indices = 0;
    u32 _size = 0;

    bxBenaphore _lock;

    void StartUp();
    void ShutDown();

    ActorID acquire();
    void release( ActorID* mi );

    bool alive( ActorID mi ) const
    {
        ActorHandle h( mi.i );
        SYS_ASSERT( h.index < eMAX_COUNT );
        return _generation[h.index] == h.generation;
    }

    void setData( ActorID mi, Scene scene, u32 dataIndex )
    {
        ActorHandle h = { mi.i };
        SYS_ASSERT( alive( mi ) );
        _scene[h.index] = scene;
        _data_index[h.index] = dataIndex;
    }

    u32 getDataIndex( ActorID mi ) const
    {
        SYS_ASSERT( alive( mi ) );
        const ActorHandle h( mi.i );
        return _data_index[h.index];
    }
    Scene getScene( ActorID mi )
    {
        SYS_ASSERT( alive( mi ) );
        const ActorHandle h( mi.i );
        return _scene[h.index];
    }
};
}}///

