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

    void flock_simulate( Flock* flock, float deltaTime )
    {
        const Vector3 com( 0.f );

        const float massInv = 1 / 1.f;
        const float boidMaxSpeed = 2.f;
        const float boidRadius = 0.5f;
        const float separation = 0.95f;
        const float alignment = 0.01f;
        const float cohesion = 0.01f;
        const float cellSize = 2.f;

        const float cellSizeSqr = cellSize * cellSize;
        const float boidRadiusSqr = boidRadius * boidRadius;

        FlockParticles* fp = &flock->particles;

        const int nBoids = flock->particles.size;
        //for( int iboid = 0; iboid < nBoids; ++iboid )
        //{
        //    Vector3 pos = fp->pos0[iboid];
        //    Vector3 vel = fp->vel[iboid];

        //    Vector3 cohesionVec( 0.f );
        //    { // cohesion
        //        Vector3 localCom( 0.f );
        //        int neighbourCount = 0;
        //        for( int iboid1 = 0; iboid1 < nBoids; ++iboid1 )
        //        {
        //            if( iboid1 == iboid )
        //                continue;

        //            const Vector3& posB = fp->pos0[iboid1];
        //            const float dd = lengthSqr( posB - pos ).getAsFloat();

        //            if( dd > cellSizeSqr )
        //                continue;

        //            localCom += posB;
        //            ++neighbourCount;
        //        }

        //        cohesionVec = ( localCom - pos ) * cohesion;
        //    }

        //    Vector3 separationVec( 0.f );
        //    { // separation
        //        for( int iboid1 = 0; iboid1 < nBoids; ++iboid1 )
        //        {
        //            if ( iboid1 == iboid )
        //                continue;

        //            const Vector3& posB = fp->pos0[iboid1];
        //            const float dd = lengthSqr( posB - pos ).getAsFloat();

        //            if( dd > cellSizeSqr )
        //                continue;

        //            if( dd > boidRadiusSqr )
        //                continue;

        //            const float d = ::sqrt( dd );
        //            separationVec -= (posB - pos) * (boidRadius - d) * separation;
        //        }
        //    }

        //    Vector3 alignmentVec( 0.f );
        //    { // alignment
        //        int neighbourCount = 0;
        //        for( int iboid1 = 0; iboid1 < nBoids; ++iboid1 )
        //        {
        //            if ( iboid1 == iboid )
        //                continue;

        //            const Vector3& posB = fp->pos0[iboid1];
        //            const float dd = lengthSqr( posB - pos ).getAsFloat();
        //            if( dd > cellSizeSqr )
        //                continue;

        //            alignmentVec += fp->vel[iboid1];
        //            ++neighbourCount;
        //        }

        //        if( neighbourCount )
        //        {
        //            alignmentVec *= ( 1.f / neighbourCount ) * alignment;
        //        }
        //    }

        //    const Vector3 toCom = com - pos;
        //    vel += ( toCom ) * massInv * separation;
        //    vel += alignmentVec;
        //    vel += cohesionVec;
        //    vel += separationVec / deltaTime;
        //    const float speed = length( vel ).getAsFloat();
        //    if( speed > boidMaxSpeed )
        //    {
        //        vel = normalize( vel ) * boidMaxSpeed;
        //    }

        //    pos += vel * deltaTime;

        //    fp->pos0[iboid] = pos;
        //    fp->vel[iboid] = vel;
        //}

        for ( int iboid = 0; iboid < nBoids; ++iboid )
        {
            Vector3 pos = fp->pos0[iboid];
            Vector3 vel = fp->vel[iboid];
            Quat rot = fp->rot0[iboid];
            Vector3 avel = fp->avel[iboid];

            rot += (deltaTime * 0.5f) * Quat( avel, 0.f ) * rot;
            rot = normalize( rot );

            vel = fastRotate( rot, Vector3::zAxis() ) * length( vel );
            //vel += (com - pos) * deltaTime * massInv;
            pos += vel * deltaTime;
            
            fp->pos1[iboid] = pos;
            fp->vel[iboid] = vel;
            fp->rot1[iboid] = rot;
        }

        for ( int iboid = 0; iboid < nBoids; ++iboid )
        {
            for ( int iboid1 = iboid + 1; iboid1 < nBoids; ++iboid1 )
            {
                Vector3 posA = fp->pos1[iboid];
                Vector3 posB = fp->pos1[iboid1];

                Vector3 dposA, dposB;
                bxPhysics::pbd_solveRepulsionConstraint( &dposA, &dposB, posA, posB, 1.f, 1.f, boidRadius, separation );

                posA += dposA;
                posB += dposB;

                fp->pos1[iboid] = posA;
                fp->pos1[iboid1] = posB;
            }
        }

        for ( int iboid = 0; iboid < nBoids; ++iboid )
        {
            Vector3 localCom( 0.f );
            Vector3 avgVel( 0.f );
            Vector3 posA = fp->pos1[iboid];
            Vector3 velA = fp->vel[iboid];
            const Vector3 dir0 = fastRotate( fp->rot1[iboid], Vector3::zAxis() );
            Vector3 dir = dir0;

            int neighbourCount = 0;
            for ( int iboid1 = 0; iboid1 < nBoids; ++iboid1 )
            {
                if ( iboid1 == iboid )
                    continue;

                const Vector3& posB = fp->pos1[iboid1];
                const float dd = lengthSqr( posB - posA ).getAsFloat();

                if ( dd > cellSizeSqr )
                    continue;

                localCom += posB;
                avgVel += fp->vel[iboid1];
                ++neighbourCount;
            }

            if ( neighbourCount )
            {
                const float denom = 1.f / neighbourCount;
                localCom *= denom;
                avgVel *= denom;

                {
                    Vector3 n = localCom - posA;
                    posA += n * cohesion;
                    dir += n * cohesion;
                    //velA += (n * cohesion);
                }
                {
                    dir = lerp( alignment, dir, avgVel );
                    posA += ( avgVel - velA ) * alignment;// lerp( alignment, velA, avgVel );
                }
                
            }

            //dir += (com - posA) * cohesion;
            dir = normalizeSafe( dir );
            Quat drot = shortestRotation( dir0, dir );
            fp->rot1[iboid] *= drot;

            posA += (com - posA) * deltaTime;
            fp->pos1[iboid] = posA;
        }

        const float deltaTimeInv = (deltaTime > FLT_EPSILON) ? 1.f / deltaTime : 0.f;
        for ( int iboid = 0; iboid < nBoids; ++iboid )
        {
            const Vector3& pos0 = fp->pos0[iboid];
            const Vector3& pos1 = fp->pos1[iboid];
            
            fp->vel[iboid] = (pos1 - pos0) * deltaTimeInv;
            fp->pos0[iboid] = pos1;

            const Quat& q0 = fp->rot0[iboid];
            const Quat& q1 = fp->rot1[iboid];
            const Quat drot = q1 * conj( q0 );
            const Vector4 axisAngle = toAxisAngle( drot );
            fp->avel[iboid] = ( axisAngle.getXYZ() * axisAngle.getW() ) * deltaTimeInv;
            fp->rot0[iboid] = q1;
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
            const Matrix3 rot( fp->rot0[iboid] );

            bxGfxDebugDraw::addSphere( Vector4( pos, 0.1f ), 0xFFFF00FF, true );
            bxGfxDebugDraw::addLine( pos, pos + rot.getCol2(), 0x0000FFFF, true );
        }
    }

}///