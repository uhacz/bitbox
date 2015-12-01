#include "phx.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <string.h>

namespace bx
{
    struct PhxContacts
    {
        void* memoryHandle;
        Vector3* normal;
        f32* depth;
        u16* index;

        i32 size;
        i32 capacity;
    };
    void phxContactsAllocateData( PhxContacts* con, int newCap )
    {
        if ( newCap <= con->capacity )
            return;

        int memSize = 0;
        memSize += newCap * sizeof( *con->normal );
        memSize += newCap * sizeof( *con->depth );
        memSize += newCap * sizeof( *con->index );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );

        PhxContacts newCon;
        newCon.memoryHandle = mem;
        newCon.normal = chunker.add< Vector3 >( newCap );
        newCon.depth = chunker.add< f32 >( newCap );
        newCon.index = chunker.add< u16 >( newCap );
        chunker.check();

        newCon.size = con->size;
        newCon.capacity = newCap;

        if ( con->size )
        {
            BX_CONTAINER_COPY_DATA( &newCon, con, normal );
            BX_CONTAINER_COPY_DATA( &newCon, con, depth );
            BX_CONTAINER_COPY_DATA( &newCon, con, index );
        }

        BX_FREE( bxDefaultAllocator(), con->memoryHandle );
        *con = newCon;
    }

    void phxContactsCreate( PhxContacts** c, int capacity )
    {
        PhxContacts* contacts = BX_NEW( bxDefaultAllocator(), PhxContacts );
        memset( contacts, 0x00, sizeof( PhxContacts ) );
        phxContactsAllocateData( contacts, capacity );
        c[0] = contacts;
    }
    void phxContactsDestroy( PhxContacts** c )
    {
        BX_FREE0( bxDefaultAllocator(), c[0]->memoryHandle );
        BX_DELETE0( bxDefaultAllocator(), c[0] );
    }

    int phxContactsPushBack( PhxContacts* con, const Vector3& normal, float depth, u16 index )
    {
        if ( con->size + 1 > con->capacity )
        {
            phxContactsAllocateData( con, con->size * 2 + 8 );
        }

        int i = con->size++;
        con->normal[i] = normal;
        con->depth[i] = depth;
        con->index[i] = index;
        return i;
    }
    int phxContactsSize( PhxContacts* con )
    {
        return con->size;
    }
    void phxContactsGet( PhxContacts* con, Vector3* normal, float* depth, u16* index0, u16* index1, int i )
    {
        SYS_ASSERT( i >= 0 || i < con->size );

        normal[0] = con->normal[i];
        depth[0] = con->depth[i];
        index0[0] = con->index[i];
        index1[0] = 0xFFFF;
    }
    void phxContactsClear( PhxContacts* con )
    {
        con->size = 0;
    }

    

}///