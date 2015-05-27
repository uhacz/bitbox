#include "game.h"

#include <util/type.h>
#include <util/buffer_utils.h>
#include <util/array.h>

#include <gfx/gfx_debug_draw.h>

namespace bxGame
{
#define BX_GAME_COPY_DATA( to, from, field ) memcpy( to.field, from->field, from->size * sizeof( *from->field ) )


    struct Constraint
    {
        u16 p0;
        u16 p1;
        f32 len;
    };

    typedef array_t< Constraint > CharacterConstraints;

    struct CharacterParticles
    {
        void* memoryHandle;

        Vector3* restPos;
      
        Vector3* pos0;
        Vector3* pos1;
        Vector3* velocity;
        f32*     mass;
        f32*     massInv;

        i32 size;
        i32 capacity;
    };
    void _CharacterParticles_allocateData( CharacterParticles* cp, int newCap )
    {
        if ( newCap <= cp->capacity )
            return;

        int memSize = 0;
        memSize += newCap * sizeof( *cp->restPos );
        memSize += newCap * sizeof( *cp->pos0 );
        memSize += newCap * sizeof( *cp->pos1 );
        memSize += newCap * sizeof( *cp->velocity );
        memSize += newCap * sizeof( *cp->mass );
        memSize += newCap * sizeof( *cp->massInv );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );
        
        CharacterParticles newCP;
        newCP.memoryHandle = mem;
        newCP.restPos = chunker.add< Vector3 >( newCap );
        newCP.pos0 = chunker.add< Vector3 >( newCap );
        newCP.pos1 = chunker.add< Vector3 >( newCap );
        newCP.velocity = chunker.add< Vector3 >( newCap );
        newCP.mass = chunker.add< f32 >( newCap );
        newCP.massInv = chunker.add< f32 >( newCap );
        chunker.check();

        newCP.size = cp->size;
        newCP.capacity = newCap;

        if( cp->size )
        {
            BX_GAME_COPY_DATA( newCP, cp, restPos );
            BX_GAME_COPY_DATA( newCP, cp, pos0 );
            BX_GAME_COPY_DATA( newCP, cp, pos0 );
            BX_GAME_COPY_DATA( newCP, cp, velocity );
            BX_GAME_COPY_DATA( newCP, cp, mass );
            BX_GAME_COPY_DATA( newCP, cp, massInv );
        }

        BX_FREE( bxDefaultAllocator(), cp->memoryHandle );
        *cp = newCP;
    }

    struct CharacterCenterOfMass
    {
        Vector3 pos;
        Quat rot;
    };

    struct Character
    {
        CharacterParticles particles;
        CharacterCenterOfMass centerOfMass;
    };

}///

namespace bxGame
{
    Character* character_new()
    {
        Character* character = BX_NEW( bxDefaultAllocator(), Character );
        memset( &character->particles, 0x00, sizeof( CharacterParticles ) );

        return character;
    }
    void character_delete( Character** c )
    {
        if ( !c[0] )
            return;

        BX_FREE0( bxDefaultAllocator(), c[0]->particles.memoryHandle );
        BX_DELETE0( bxDefaultAllocator(), c[0] );
    }

    void character_init( Character* character, const Matrix4& worldPose )
    {
        const float a = 0.5f;
        const int NUM_POINTS = 8;
        const Vector3 restPos[NUM_POINTS] =
        {
            Vector3( -a, -a, a ),
            Vector3( a, -a, a ),
            Vector3( a, a, a ),
            Vector3( -a, a, a ),

            Vector3( -a, -a, -a ),
            Vector3( a, -a, -a ),
            Vector3( a, a, -a ),
            Vector3( -a, a, -a ),
        };

        CharacterParticles& cp = character->particles;
        _CharacterParticles_allocateData( &cp, NUM_POINTS );

        for( int i = 0; i < NUM_POINTS; ++i )
        {
            cp.restPos[i] = restPos[i];
            
            const Vector3 pos = mulAsVec4( worldPose, restPos[i] );
            cp.pos0[i] = pos;
            cp.pos1[i] = pos;
            cp.velocity[i] = Vector3( 0.f );
            cp.mass[i] = 1.f;
            cp.massInv[i] = 1.f;
        }
        cp.size = NUM_POINTS;

        character->centerOfMass.pos = worldPose.getTranslation();
        character->centerOfMass.rot = Quat( worldPose.getUpper3x3() );
    }

    void character_tick( Character* character, float deltaTime )
    {
        CharacterParticles& cp = character->particles;
        for( int i = 0; i < cp.size; ++i )
        {
            bxGfxDebugDraw::addSphere( Vector4( cp.pos0[i], 0.1f ), 0x00FF00FF, true );
        }

        const Vector3 groundPlaneOffset = Vector3( 0.f, -2.f, 0.f );
        const Vector4 groundPlane = makePlane( Vector3::yAxis(), groundPlaneOffset );
        bxGfxDebugDraw::addBox( Matrix4::translation( groundPlaneOffset ), Vector3( 10.f, 0.1f, 10.f ) , 0xFF0000FF, true );

        const Vector3 gravity = Vector3( 0.f, -1.f, 0.f );
        const floatInVec dtv( deltaTime );
        const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( 0.2f ), dtv );
        const int nPoints = cp.size;

        for( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            Vector3 pos = cp.pos0[ipoint];
            Vector3 vel = cp.velocity[ipoint];
            
            vel += gravity * dtv * floatInVec( cp.massInv[ipoint] );
            vel *= dampingCoeff;

            pos += vel * dtv;

            cp.pos1[ipoint] = pos;
            cp.velocity[ipoint] = vel;
        }

        for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            Vector3 pos = cp.pos1[ipoint];

            const floatInVec d = dot( groundPlane, Vector4( pos, oneVec ) );
            const Vector3 dpos = -groundPlane.getXYZ() * minf4( d, zeroVec );
            pos += dpos;

            cp.pos1[ipoint] = pos;
        }

        Vector3 com( 0.f );
        floatInVec totalMass( 0.f );
        for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            const floatInVec mass( cp.mass[ipoint] );
            com += cp.pos1[ipoint] * mass;
            totalMass += mass;
        }
        com /= totalMass;

        Vector3 col0( 0.f );
        Vector3 col1( 0.f );
        Vector3 col2( 0.f );
        for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            const Vector3& q = cp.restPos[ipoint];
            const Vector3 p = cp.pos1[ipoint] - com;
            const floatInVec mass( cp.mass[ipoint] );
            col0 += p * q.getX() * mass;
            col1 += p * q.getY() * mass;
            col2 += p * q.getZ() * mass;
        }
        col0 = normalize( col0 );
        col1 = normalize( col1 );
        col2 = normalize( col2 );
        Matrix3 R( col0, col1, col2 );

        bxGfxDebugDraw::addLine( com, com + col0, 0x990000FF, true );
        bxGfxDebugDraw::addLine( com, com + col1, 0x009900FF, true );
        bxGfxDebugDraw::addLine( com, com + col2, 0x000099FF, true );

        const floatInVec shapeStiffness( 1.f );
        for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            const Vector3 goalPos = com + R * cp.restPos[ipoint];
            
            Vector3 pos = cp.pos1[ipoint];
            const Vector3 dpos = goalPos - pos;
            
            pos += dpos * shapeStiffness;

            cp.pos1[ipoint] = pos;
        }

        const floatInVec dtInv = select( zeroVec, oneVec / dtv, dtv > fltEpsVec );
        for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
        {
            cp.velocity[ipoint] = (cp.pos1[ipoint] - cp.pos0[ipoint]) * dtInv;
            cp.pos0[ipoint] = cp.pos1[ipoint];
        }
    }
}///

