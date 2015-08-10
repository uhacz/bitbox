#include "game.h"

#include "physics.h"
#include "physics_pbd.h"

#include <system/input.h>

#include <util/type.h>
#include <util/buffer_utils.h>
#include <util/array.h>
#include <util/common.h>
#include <util/poly/poly_shape.h>
#include <util/signal_filter.h>
#include <util/random.h>

#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_camera.h>
#include <gfx/gfx_gui.h>
#include <anim/anim.h>

namespace bxGame
{
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

        if ( cp->size )
        {
            BX_CONTAINER_COPY_DATA( &newCP, cp, restPos );
            BX_CONTAINER_COPY_DATA( &newCP, cp, pos0 );
            BX_CONTAINER_COPY_DATA( &newCP, cp, pos0 );
            BX_CONTAINER_COPY_DATA( &newCP, cp, velocity );
            BX_CONTAINER_COPY_DATA( &newCP, cp, mass );
            BX_CONTAINER_COPY_DATA( &newCP, cp, massInv );
        }

        BX_FREE( bxDefaultAllocator(), cp->memoryHandle );
        *cp = newCP;
    }

    struct CharacterAnimation
    {
        bxAnim_Skel* skel;
        bxAnim_Clip* clip;
        bxAnim_Context* ctx;
    };
    void _CharacterAnimation_load( CharacterAnimation* ca, bxResourceManager* resourceManager )
    {
        ca->skel = bxAnimExt::loadSkelFromFile( resourceManager, "anim/human.skel" );
        ca->clip = bxAnimExt::loadAnimFromFile( resourceManager, "anim/walk.anim" );

        if( ca->skel )
        {
            ca->ctx = bxAnim::contextInit( *ca->skel );
        }

    }
    void _CharacterAnimation_unload( CharacterAnimation* ca, bxResourceManager* resourceManager )
    {
        bxAnim::contextDeinit( &ca->ctx );
        bxAnimExt::unloadAnimFromFile( resourceManager, &ca->clip );
        bxAnimExt::unloadSkelFromFile( resourceManager, &ca->skel );
    }

    struct CharacterCenterOfMass
    {
        Vector3 prevPos;
        Quat prevRot;
        
        Vector3 pos;
        Quat rot;

        CharacterCenterOfMass()
            : prevPos( 0.f )
            , prevRot( Quat::identity() )
            , pos( 0.f )
            , rot( Quat::identity() )
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
        if ( pad.currentState()->connected )
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
            charInput->jump = signalFilter_lowPass( jump, charInput->jump, RC, deltaTime );
            charInput->crouch = signalFilter_lowPass( crouch, charInput->crouch, RC, deltaTime );

            //bxLogInfo( "x: %f, y: %f", charInput->analogX, charInput->jump );
        }
    }

    struct CharacterParams
    {
        f32 maxInputForce;
        f32 jumpStrength;
        f32 staticFriction;
        f32 dynamicFriction;
        f32 velocityDamping;
        f32 shapeStiffness;
        f32 shapeScale;
        f32 gravity;

        CharacterParams()
            : maxInputForce( 0.15f )
            , jumpStrength( 15.f )
            , staticFriction( 0.2f )
            , dynamicFriction( 0.8f )
            , velocityDamping( 0.7f )
            , shapeStiffness( 0.1f )
            , shapeScale( 1.f )
            , gravity( 9.1f )
        {}
    };

    void _CharacterParams_show( CharacterParams* params )
    {
        if ( ImGui::Begin( "Character" ) )
        {
            ImGui::InputFloat( "maxInputForce", &params->maxInputForce );
            ImGui::InputFloat( "jumpStrength", &params->jumpStrength );
            ImGui::SliderFloat( "staticFriction", &params->staticFriction, 0.f, 1.f );
            ImGui::SliderFloat( "dynamicFriction", &params->dynamicFriction, 0.f, 1.f );
            ImGui::SliderFloat( "velocityDamping", &params->velocityDamping, 0.f, 1.f );
            ImGui::SliderFloat( "shapeStiffness", &params->shapeStiffness, 0.f, 1.f );
            ImGui::SliderFloat( "shapeScale", &params->shapeScale, 0.f, 5.f );
            ImGui::InputFloat( "gravity", &params->gravity );
        }
        ImGui::End();
    }

    struct Character
    {
        CharacterAnimation anim;
        CharacterParticles particles;
        CharacterCenterOfMass centerOfMass;
        
        Vector3 upVector;
        Vector3 dirVector;

        CharacterInput input;
        CharacterParams params;
        bxPhx_Contacts* contacts;

        f32 _dtAcc;
        f32 _jumpAcc;
        u64 _timeMS;

        Character()
            : upVector( Vector3::yAxis() )
            , dirVector( Vector3::zAxis() )
            , contacts( 0 )
            , _dtAcc( 0.f )
            , _jumpAcc( 0.f )
            , _timeMS(0)
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
        memset( &character->anim, 0x00, sizeof( CharacterAnimation ) );
        return character;
    }
    void character_delete( Character** c )
    {
        if ( !c[0] )
            return;

        bxPhx::contacts_delete( &c[0]->contacts );
        BX_FREE0( bxDefaultAllocator(), c[0]->particles.memoryHandle );
        BX_DELETE0( bxDefaultAllocator(), c[0] );
    }

    void character_init( Character* character, bxResourceManager* resourceManager, const Matrix4& worldPose )
    {
        const float a = 0.5f;

        bxPolyShape shape;
        bxPolyShape_createShpere( &shape, 2 );

        const int NUM_POINTS = shape.num_vertices;
        CharacterParticles& cp = character->particles;
        _CharacterParticles_allocateData( &cp, NUM_POINTS );

        //bxRandomGen rnd( 0xBAADC0DE );

        for ( int i = 0; i < NUM_POINTS; ++i )
        {
            //const Vector3 pos = mulAsVec4( worldPose, restPos[i] );
            const Vector3 restPos( xyz_to_m128( shape.position( i ) ) );
            const Vector3 pos = mulAsVec4( worldPose, restPos );
            cp.pos0[i] = pos;
            cp.pos1[i] = pos;
            cp.velocity[i] = Vector3( 0.f );
            cp.mass[i] = 1.f; // bxRand::randomFloat( rnd, 1.f, 0.5f );
            cp.massInv[i] = 1.f / cp.mass[i];
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
        character->centerOfMass.prevPos = character->centerOfMass.pos;
        character->centerOfMass.prevRot = character->centerOfMass.rot;
        
        character->contacts = bxPhx::contacts_new( NUM_POINTS );

        bxPolyShape_deallocateShape( &shape );

        _CharacterAnimation_load( &character->anim, resourceManager );
    }

    void character_deinit( Character* character, bxResourceManager* resourceManager )
    {
        _CharacterAnimation_unload( &character->anim, resourceManager );
    }


    namespace
    {
        void character_simulate( Character* character, bxPhx_CollisionSpace* cspace, const Vector3& externalForces, float deltaTime )
        {
            CharacterParticles& cp = character->particles;
            const CharacterParams& params = character->params;

            const Vector3 gravity = -character->upVector * params.gravity;
            const floatInVec dtv( deltaTime );
            const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( params.velocityDamping ), dtv );
            const int nPoints = cp.size;

            for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
            {
                Vector3 pos = cp.pos0[ipoint];
                Vector3 vel = cp.velocity[ipoint];

                vel += (gravity)* dtv * floatInVec( cp.massInv[ipoint] );
                vel *= dampingCoeff;
                vel += externalForces;

                pos += vel * dtv;

                cp.pos1[ipoint] = pos;
                cp.velocity[ipoint] = vel;
            }

            {
                bxPhx::contacts_clear( character->contacts );
                bxPhx::collisionSpace_collide( cspace, character->contacts, cp.pos1, nPoints );
            }

            Matrix3 R;
            Vector3 com;
            bxPhx::pbd_softBodyUpdatePose( &R, &com, cp.pos1, cp.restPos, cp.mass, nPoints );
            character->centerOfMass.pos = com;
            character->centerOfMass.rot = Quat( R );

            const Matrix3 R1 = appendScale( R, Vector3( params.shapeScale ) );
            for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
            {
                Vector3 dpos( 0.f );
                bxPhx::pbd_solveShapeMatchingConstraint( &dpos, R1, com, cp.restPos[ipoint], cp.pos1[ipoint], params.shapeStiffness );
                cp.pos1[ipoint] += dpos;
            }

            {
                bxPhx_Contacts* contacts = character->contacts;
                const int nContacts = bxPhx::contacts_size( contacts );

                Vector3 normal( 0.f );
                float depth = 0.f;
                u16 index0 = 0xFFFF;
                u16 index1 = 0xFFFF;

                for ( int icontact = 0; icontact < nContacts; ++icontact )
                {
                    bxPhx::contacts_get( contacts, &normal, &depth, &index0, &index1, icontact );

                    Vector3 dpos( 0.f );
                    bxPhx::pbd_computeFriction( &dpos, cp.pos0[index0], cp.pos1[index0], normal, params.staticFriction, params.dynamicFriction );
                    cp.pos1[index0] += dpos;
                }
            }

            const floatInVec dtInv = select( zeroVec, oneVec / dtv, dtv > fltEpsVec );
            for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
            {
                cp.velocity[ipoint] = (cp.pos1[ipoint] - cp.pos0[ipoint]) * dtInv;
                cp.pos0[ipoint] = cp.pos1[ipoint];
            }
        }
    
        inline Vector3 angularVelocityFromOrientations( const Quat& q0, const Quat& q1, float deltaTimeInv )
        {
            const Quat diffQ = q1 - q0;
            //Quat W( -diffQ.getXYZ(), diffQ.getW() );
            const Quat velQ = ( ( diffQ * 2.f ) * deltaTimeInv ) * conj( q0 );
            return velQ.getXYZ();
        }
    }///



    void character_tick( Character* character, bxPhx_CollisionSpace* cspace, const bxGfxCamera& camera, const bxInput& input, float deltaTime )
    {
        const CharacterParams& params = character->params;
        characterInput_collectData( &character->input, input, deltaTime );
        Vector3 externalForces( 0.f );
        {
            externalForces += Vector3::xAxis() * character->input.analogX;
            externalForces -= Vector3::zAxis() * character->input.analogY;

            const floatInVec externalForcesValue = minf4( length( externalForces ), floatInVec( params.maxInputForce ) );
            externalForces = projectVectorOnPlane( camera.matrix.world.getUpper3x3() * externalForces, Vector4( character->upVector, oneVec ) );
            externalForces = normalizeSafe( externalForces ) * externalForcesValue;

            character->_jumpAcc += character->input.jump * params.jumpStrength;
        }

        const float fixedFreq = 60.f;
        const float fixedDt = 1.f / fixedFreq;

        character->_dtAcc += deltaTime;

        while ( character->_dtAcc >= fixedDt )
        {
            externalForces += character->upVector * character->_jumpAcc;

            character_simulate( character, cspace, externalForces, fixedDt );
            character->_dtAcc -= fixedDt;

            character->_jumpAcc = 0.f;
            
            {
                CharacterCenterOfMass& com = character->centerOfMass;
                const Vector3 velocity = ( com.pos - com.prevPos ) * fixedFreq;
                const Vector3 spin = angularVelocityFromOrientations( com.prevRot, com.rot, fixedFreq );




            }
            
            character->centerOfMass.prevPos = character->centerOfMass.pos;
            character->centerOfMass.prevRot = character->centerOfMass.rot;
        }

        {
            CharacterAnimation& anim = character->anim;

            const float time = (float)( (double)character->_timeMS * 0.001 );
            bxAnim_Joint* localJoints = bxAnim::poseFromStack( anim.ctx, 0 );
            bxAnim_Joint* worldJoints = bxAnim::poseFromStack( anim.ctx, 1 );
            bxAnim::evaluate( localJoints, anim.clip, time );

            bxAnimExt::localJointsToWorldJoints( worldJoints, localJoints, anim.skel, bxAnim_Joint::identity() );

            for( int ijoint = 0; ijoint < anim.skel->numJoints; ++ijoint )
            {
                const bxAnim_Joint& joint = worldJoints[ijoint];
                bxGfxDebugDraw::addSphere( Vector4( joint.position, 0.05f ), 0xFF00FF00, true );
            }
        }

        character->_timeMS += (u64)( (double)deltaTime * 1000.0 );

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

        _CharacterParams_show( &character->params );



    }

    Matrix4 character_pose( const Character* character )
    {
        return Matrix4( character->centerOfMass.rot, character->centerOfMass.pos );
    }

    Vector3 character_upVector( const Character* character )
    {
        return character->upVector;
    }



}///

