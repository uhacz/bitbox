#include "game.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/random.h>
#include <util/math.h>
#include "gfx/gfx_debug_draw.h"
#include "physics_pbd.h"

namespace bxGame
{
    struct FlockParticles
    {
        void* memoryHandle;
        
        Vector3* pos0;
        Vector3* pos1;
        Vector3* vel;
        
        Quat* rot0;
        Quat* rot1;
        Vector3* avel;

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
        memSize += capacity * sizeof( *fp->rot0 );
        memSize += capacity * sizeof( *fp->rot1 );
        memSize += capacity * sizeof( *fp->avel );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );

        FlockParticles newFp;
        newFp.memoryHandle = mem;
        newFp.size = fp->size;
        newFp.capacity = capacity;
        newFp.pos0 = chunker.add< Vector3 >( capacity );
        newFp.pos1 = chunker.add< Vector3 >( capacity );
        newFp.vel = chunker.add< Vector3 >( capacity );
        newFp.rot0 = chunker.add< Quat >( capacity );
        newFp.rot1 = chunker.add< Quat >( capacity );
        newFp.avel = chunker.add< Vector3 >( capacity );
        chunker.check();

        if( fp->size )
        {
            BX_CONTAINER_COPY_DATA( &newFp, fp, pos0 );
            BX_CONTAINER_COPY_DATA( &newFp, fp, pos1 );
            BX_CONTAINER_COPY_DATA( &newFp, fp, vel );
            BX_CONTAINER_COPY_DATA( &newFp, fp, rot0 );
            BX_CONTAINER_COPY_DATA( &newFp, fp, rot1 );
            BX_CONTAINER_COPY_DATA( &newFp, fp, avel );
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
        fp->rot0[index] = Quat::identity();
        fp->rot1[index] = Quat::identity();
        fp->avel[index] = Vector3( 0.f );
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

        f32 _dtAcc;
    };

    Flock* flock_new()
    {
        Flock* fl = BX_NEW( bxDefaultAllocator(), Flock );
        memset( &fl->particles, 0x00, sizeof( FlockParticles ) );
        fl->_dtAcc = 0.f;
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

    inline bool isInNeighbourhood( const Vector3& pos, const Vector3& posB, float radius )
    {
        const Vector3 vec = posB - pos;
        const float dd = lengthSqr( vec ).getAsFloat();

        return ( dd <= radius * radius );
    }
    Quat quatAim( const Vector3& v )
    {
        const Vector3 vn = normalize( v );

        Quat qr;
        qr.setX( v.getY() );
        qr.setY( -v.getX() );
        qr.setZ( zeroVec );
        qr.setW( oneVec - v.getZ() );
        return normalize( qr );
    }
    Quat quatAim( const Vector3& p1, const Vector3& p2 )
    {
        const Vector3 v = p2 - p1;
        return quatAim( v );
    }

    void flock_simulate( Flock* flock, float deltaTime )
    {
        const Vector3 com( 0.f );

        const float massInv = 1 / 1.f;
        const float boidMaxSpeed = 2.f;
        const float boidRadius = 1.5f;
        const float separation = 1.0f;
        const float alignment = 0.1f;
        const float cohesion = 0.1f;
        const float attraction = 0.6f;
        const float cellSize = 8.f;

        const float cellSizeSqr = cellSize * cellSize;
        const float boidRadiusSqr = boidRadius * boidRadius;

        FlockParticles* fp = &flock->particles;

        const int nBoids = flock->particles.size;

        for( int iboid = 0; iboid < nBoids; ++iboid )
        {
            Vector3 separationVec( 0.f );
            Vector3 cohesionVec( 0.f );
            Vector3 alignmentVec( 0.f );
            int neighbours = 0;

            Vector3 pos = fp->pos0[iboid];
            Vector3 vel = fp->vel[iboid];

            ////
            ////
            for( int iboid1 = 0; iboid1 < nBoids; ++iboid1 )
            {
                if ( iboid1 == iboid )
                    continue;

                const Vector3& posB = fp->pos0[iboid1];
                const Vector3 vec = posB - pos;
                const float dd = lengthSqr( vec ).getAsFloat();
                if( dd > cellSizeSqr )
                    continue;

                const Vector3 displ = vec * ( 1.f / dd );
                separationVec += displ;
                ++neighbours;
            }
            separationVec = normalizeSafe( separationVec );
            
            ////
            ////
            neighbours = 0;
            for( int iboid1 = 0; iboid1 < nBoids; ++iboid1 )
            {
                if( iboid1 == iboid )
                    continue;
            
                const Vector3& posB = fp->pos0[iboid1];
                if( !isInNeighbourhood( pos, posB, cellSize ) )
                    continue;

                alignmentVec += fp->vel[iboid1];
                ++neighbours;
            }

            if( neighbours > 0 )
            {
                alignmentVec = normalizeSafe( ( alignmentVec / (f32)neighbours ) - fp->vel[iboid] );
            }

            ////
            ////
            neighbours = 0;
            for( int iboid1 = 0; iboid1 < nBoids; ++iboid1 )
            {
                if( iboid1 == iboid )
                    continue;

                const Vector3& posB = fp->pos0[iboid1];
                if( !isInNeighbourhood( pos, posB, cellSize ) )
                    continue;
            
                cohesionVec += posB;
                ++neighbours;
            }

            if( neighbours )
            {
                cohesionVec = normalizeSafe( ( cohesionVec / (f32)neighbours ) - pos );
            }


            Vector3 steering( 0.f );
            steering += separationVec * separation;
            steering += alignmentVec * alignment;
            steering += cohesionVec * cohesion;
            steering += normalizeSafe( com - pos ) * attraction;

            vel += steering * deltaTime;
            pos += vel * deltaTime;

            fp->pos0[iboid] = pos;
            fp->vel[iboid] = vel;

            //fp->rot0[iboid] = quatAim( -vel );

        }
    }

    void flock_tick( Flock* flock, float deltaTime )
    {
        flock->_dtAcc += deltaTime;

        const float deltaTimeFixed = 1.f / 60.f;

        while( flock->_dtAcc >= deltaTimeFixed )
        {
            flock_simulate( flock, deltaTimeFixed );
            flock->_dtAcc -= deltaTimeFixed;
        }

        FlockParticles* fp = &flock->particles;

        const int nBoids = flock->particles.size;
        for( int iboid = 0; iboid < nBoids; ++iboid )
        {
            const Vector3& pos = fp->pos0[iboid];
            const Vector3& vel = fp->vel[iboid];
            const Matrix3 rot( fp->rot0[iboid] );

            bxGfxDebugDraw::addSphere( Vector4( pos, 0.1f ), 0xFFFF00FF, true );
            //bxGfxDebugDraw::addLine( pos, pos + vel, 0x0000FFFF, true );
            
            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol0(), 0x0F00FFFF, true );
            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol1(), 0xF000FFFF, true );
            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol2(), 0xFF00FFFF, true );
        }
    }

}///