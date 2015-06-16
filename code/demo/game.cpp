#include "game.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include "util/random.h"
#include "gfx/gfx_debug_draw.h"

namespace bxGame
{
    struct FlockParticles
    {
        void* memoryHandle;
        
        Vector3* pos0;
        Vector3* pos1;
        Vector3* vel;

        i32 size;
        i32 capacity;
    };
    void _FlockParticles_allocate( FlockParticles* fp, int capacity )
    {
        if ( fp->capacity >= capacity )
            return;

        int memSize = 0;
        memSize += capacity * sizeof( *fp->pos0 );
        memSize += capacity * sizeof( *fp->pos1 );
        memSize += capacity * sizeof( *fp->vel );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );

        FlockParticles newFp;
        newFp.memoryHandle = mem;
        newFp.size = fp->size;
        newFp.capacity = capacity;
        newFp.pos0 = chunker.add< Vector3 >( capacity );
        newFp.pos1 = chunker.add< Vector3 >( capacity );
        newFp.vel  = chunker.add< Vector3 >( capacity );
        chunker.check();

        if( fp->size )
        {
            BX_CONTAINER_COPY_DATA( &newFp, fp, pos0 );
            BX_CONTAINER_COPY_DATA( &newFp, fp, pos1 );
            BX_CONTAINER_COPY_DATA( &newFp, fp, vel );
        }

        BX_FREE0( bxDefaultAllocator(), fp->memoryHandle );
        fp[0] = newFp;
    }
    int _FlockParticles_add( FlockParticles* fp, const Vector3& pos )
    {
        if( fp->size + 1 >= fp->capacity )
        {
            _FlockParticles_allocate( fp, fp->capacity * 2 + 8 );
        }

        const int index = fp->size++;
        fp->pos0[index] = pos;
        fp->pos1[index] = pos;
        fp->vel[index] = Vector3( 0.f );
        return index;
    }

    struct FlockHashmap
    {
        u32* items;
        i32 numBuckets;
        i32 bucketSize;
        i32 bucketCapacity;

        bxAllocator* alloc;
    };
    //void flock_hashMapAdd( FlockHashmap* hmap, const Vector3& pos, int index )
    //{
    //    
    //}

    struct Flock
    {
        FlockParticles particles;
    };

    Flock* flock_new()
    {
        Flock* fl = BX_NEW( bxDefaultAllocator(), Flock );
        memset( &fl->particles, 0x00, sizeof( FlockParticles ) );
        return fl;
    }

    void flock_delete( Flock** flock )
    {
        if ( !flock[0] )
            return;

        BX_FREE0( bxDefaultAllocator(), flock[0]->particles.memoryHandle );
        BX_DELETE0( bxDefaultAllocator(), flock[0] );
    }

    void flock_init( Flock* flock, int nBoids, const Vector3& center, float radius )
    {
        _FlockParticles_allocate( &flock->particles, nBoids );

        bxRandomGen rnd( TypeReinterpert( radius ).u );
        for( int iboid = 0; iboid < nBoids; ++iboid )
        {
            const float dx = rnd.getf( -radius, radius );
            const float dy = rnd.getf( -radius, radius );
            const float dz = rnd.getf( -radius, radius );

            const Vector3 dpos = Vector3( dx, dy, dz );
            const Vector3 pos = center + dpos;

            _FlockParticles_add( &flock->particles, pos );
        }
    }

    void flock_tick( Flock* flock, float deltaTime )
    {
        const int nBoids = flock->particles.size;
        for ( int iboid = 0; iboid < nBoids; ++iboid )
        {
            const Vector3& pos = flock->particles.pos0[iboid];

            bxGfxDebugDraw::addSphere( Vector4( pos, 0.1f ), 0xFFFF00FF, true );
        }



    }

}///