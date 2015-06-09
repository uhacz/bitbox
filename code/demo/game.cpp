#include "game.h"
#include "physics.h"

#include <util/type.h>
#include <util/buffer_utils.h>
#include <util/array.h>
#include <util/common.h>

#include <gfx/gfx_debug_draw.h>
#include "util/poly/poly_shape.h"
#include "system/input.h"
#include "util/signal_filter.h"
namespace
{
    float abs_column_sum( const Matrix3& a, int i )
    {
        return sum( absPerElem( a[i] ) ).getAsFloat();
        //return abs( a[0][i] ) + abs( a[1][i] ) + abs( a[2][i] );
    }

    float abs_row_sum( const Matrix3& a, int i )
    {
        return sum( absPerElem( Vector3( a[0][i], a[1][i], a[2][i] ) ) ).getAsFloat();
        //return btFabs( a[i][0] ) + btFabs( a[i][1] ) + btFabs( a[i][2] );
    }

    float p1_norm( const Matrix3& a )
    {
        const float sum0 = abs_column_sum( a, 0 );
        const float sum1 = abs_column_sum( a, 1 );
        const float sum2 = abs_column_sum( a, 2 );
        return maxOfPair( maxOfPair( sum0, sum1 ), sum2 );
    }

    float pinf_norm( const Matrix3& a )
    {
        const float sum0 = abs_row_sum( a, 0 );
        const float sum1 = abs_row_sum( a, 1 );
        const float sum2 = abs_row_sum( a, 2 );
        return maxOfPair( maxOfPair( sum0, sum1 ), sum2 );
    }

    const float POLAR_DECOMPOSITION_DEFAULT_TOLERANCE = ( 0.0001f );
    const unsigned int POLAR_DECOMPOSITION_DEFAULT_MAX_ITERATIONS = 16;
}
unsigned int bxPolarDecomposition( const Matrix3& a, Matrix3& u, Matrix3& h, unsigned maxIterations = POLAR_DECOMPOSITION_DEFAULT_MAX_ITERATIONS )
{
    const float tolerance = POLAR_DECOMPOSITION_DEFAULT_TOLERANCE;
    // Use the 'u' and 'h' matrices for intermediate calculations
    u = a;
    h = inverse( a );

    for ( unsigned int i = 0; i < maxIterations; ++i )
    {
        const float h_1 = p1_norm( h );
        const float h_inf = pinf_norm( h );
        const float u_1 = p1_norm( u );
        const float u_inf = pinf_norm( u );

        const float h_norm = h_1 * h_inf;
        const float u_norm = u_1 * u_inf;

        // The matrix is effectively singular so we cannot invert it
        if ( ( h_norm < FLT_EPSILON) || ( u_norm < FLT_EPSILON) )
            break;

        const float gamma = pow( h_norm / u_norm, 0.25f );
        const float inv_gamma = float( 1.0 ) / gamma;

        // Determine the delta to 'u'
        const Matrix3 delta = (u * (gamma - float( 2.0 )) + transpose( h ) * inv_gamma) * float( 0.5 );

        // Update the matrices
        u += delta;
        h = inverse( u );

        // Check for convergence
        if ( p1_norm( delta ) <= tolerance * u_1 )
        {
            h = transpose( u ) * a;
            h = (h + transpose( h )) * 0.5;
            return i;
        }
    }

    // The algorithm has failed to converge to the specified tolerance, but we
    // want to make sure that the matrices returned are in the right form.
    h = transpose( u ) * a;
    h = (h + transpose( h )) * 0.5;

    return maxIterations;
}


namespace bxGame
{



#define BX_GAME_COPY_DATA( to, from, field ) memcpy( to.field, from->field, from->size * sizeof( *from->field ) )

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

    struct CharacterInput
    {
        f32 analogX;
        f32 analogY;
        f32 jump;
        f32 crouch;
    };
    void characterInput_collectData( CharacterInput* charInput, const bxInput& input, float deltaTime )
    {
        const bxInput_Pad& pad = bxInput_getPad( input );
        if( pad.currentState()->connected )
        {

        }
        else
        {
            const bxInput_Keyboard* kbd = &input.kbd;

            const int inLeft = bxInput_isKeyPressed( kbd, 'A' );
            const int inRight = bxInput_isKeyPressed( kbd, 'D' );
            const int inFwd = bxInput_isKeyPressed( kbd, 'W' );
            const int inBack = bxInput_isKeyPressed( kbd, 'S' );
            const int inJump = bxInput_isKeyPressedOnce( kbd, ' ' );
            const int inCrouch = bxInput_isKeyPressedOnce( kbd, bxInput::eKEY_LSHIFT );

            const float analogX = -(float)inLeft + (float)inRight;
            const float analogY = -(float)inBack + (float)inFwd;

            const float crouch = (f32)inCrouch;
            const float jump = (f32)inJump;

            const float RC = 0.1f;
            charInput->analogX = signalFilter_lowPass( analogX, charInput->analogX, RC, deltaTime );
            charInput->analogY = signalFilter_lowPass( analogY, charInput->analogY, RC, deltaTime );
            charInput->jump = signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
            charInput->crouch = signalFilter_lowPass( crouch, charInput->crouch, RC, deltaTime );

            bxLogInfo( "x: %f, y: %f", charInput->analogX, charInput->analogY );
        }
    }

    struct Character
    {
        CharacterParticles particles;
        CharacterCenterOfMass centerOfMass;
        CharacterInput input;

        f32 _dtAcc;

        Character()
            : _dtAcc(0.f)
        {}
    };

}///

namespace bxGame
{
    Character* character_new()
    {
        Character* character = BX_NEW( bxDefaultAllocator(), Character );
        memset( &character->particles, 0x00, sizeof( CharacterParticles ) );
        memset( &character->input, 0x00, sizeof( CharacterInput ) );

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

        bxPolyShape shape;
        bxPolyShape_createShpere( &shape, 2 );

        const int NUM_POINTS = shape.num_vertices;
        //const Vector3* restPos = (Vector3*)shape.positions;


        //const Vector3 restPos[NUM_POINTS] =
        //{
        //    Vector3( -a, -a, a ),
        //    Vector3( a, -a, a ),
        //    Vector3( a, a, a ),
        //    Vector3( -a, a, a ),

        //    Vector3( -a, -a, -a ),
        //    Vector3( a, -a, -a ),
        //    Vector3( a, a, -a ),
        //    Vector3( -a, a, -a ),
        //};

        CharacterParticles& cp = character->particles;
        _CharacterParticles_allocateData( &cp, NUM_POINTS );

        for( int i = 0; i < NUM_POINTS; ++i )
        {
            //const Vector3 pos = mulAsVec4( worldPose, restPos[i] );
            const Vector3 restPos( xyz_to_m128( shape.position( i ) ) );
            const Vector3 pos = mulAsVec4( worldPose, restPos );
            cp.pos0[i] = pos;
            cp.pos1[i] = pos;
            cp.velocity[i] = Vector3( 0.f );
            cp.mass[i] = 1.f;
            cp.massInv[i] = 1.f;
        }

        f32 massSum = 0.f;
        Vector3 com( 0.f );
        for ( int i = 0; i < NUM_POINTS; ++i )
        {
            com += cp.pos0[i];
            massSum += cp.mass[i];
        }
        com /= massSum;
        for ( int i = 0; i < NUM_POINTS; ++i )
        {
            cp.restPos[i] = cp.pos0[i] - com;
        }

        cp.size = NUM_POINTS;

        character->centerOfMass.pos = worldPose.getTranslation();
        character->centerOfMass.rot = Quat( worldPose.getUpper3x3() );

        bxPolyShape_deallocateShape( &shape );
    }

    namespace
    {
        void character_simulate( Character* character, const Vector3& externalForces, float deltaTime )
        {
            CharacterParticles& cp = character->particles;

            const Vector3 gravity = Vector3( 0.f, -9.1f, 0.f );
            const floatInVec dtv( deltaTime );
            const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( 0.4f ), dtv );
            const int nPoints = cp.size;

            for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
            {
                Vector3 pos = cp.pos0[ipoint];
                Vector3 vel = cp.velocity[ipoint];

                vel += ( gravity ) * dtv * floatInVec( cp.massInv[ipoint] );
                vel *= dampingCoeff;
                vel += externalForces;

                pos += vel * dtv;

                cp.pos1[ipoint] = pos;
                cp.velocity[ipoint] = vel;
            }

            {
                bxPhysics::collisionSpace_collide( bxPhysics::__cspace, cp.pos1, nPoints );
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

            Vector3 col0( FLT_EPSILON, 0.f, 0.f );
            Vector3 col1( 0.f, FLT_EPSILON * 2.f, 0.f );
            Vector3 col2( 0.f, 0.f, FLT_EPSILON * 4.f );
            for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
            {
                const floatInVec mass( cp.mass[ipoint] );
                const Vector3& q = cp.restPos[ipoint];
                const Vector3 p = (cp.pos1[ipoint] - com) * mass;
                col0 += p * q.getX();
                col1 += p * q.getY();
                col2 += p * q.getZ();
            }
            Matrix3 Apq( col0, col1, col2 );
            Matrix3 R, S;
            bxPolarDecomposition( Apq, R, S );

            character->centerOfMass.pos = com;
            character->centerOfMass.rot = Quat( R );

            const floatInVec shapeStiffness( 0.1f );
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



    void character_tick( Character* character, const bxGfxCamera& camera, const bxInput& input, float deltaTime )
    {
        characterInput_collectData( &character->input, input, deltaTime );
        Vector3 externalForces( 0.f );
        {
            externalForces += Vector3::xAxis() * character->input.analogX;
            externalForces += Vector3::zAxis() * character->input.analogY;
        }
                
        const float fixedDt = 1.f / 60.f;
        character->_dtAcc += deltaTime;

        while( character->_dtAcc >= fixedDt )
        {
            character_simulate( character, externalForces, fixedDt );
            character->_dtAcc -= fixedDt;
        }

        CharacterParticles& cp = character->particles;
        for ( int i = 0; i < cp.size; ++i )
        {
            bxGfxDebugDraw::addSphere( Vector4( cp.pos0[i], 0.05f ), 0x00FF00FF, true );
        }
        const Vector3& com = character->centerOfMass.pos;
        const Matrix3 R( character->centerOfMass.rot );
        bxGfxDebugDraw::addLine( com, com + R[0], 0x990000FF, true );
        bxGfxDebugDraw::addLine( com, com + R[1], 0x009900FF, true );
        bxGfxDebugDraw::addLine( com, com + R[2], 0x000099FF, true );
    }
}///

