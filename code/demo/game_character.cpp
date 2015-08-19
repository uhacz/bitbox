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

    struct CharacterCenterOfMass
    {
        Vector3 pos;
        Quat rot;

        CharacterCenterOfMass()
            : pos( 0.f )
            , rot( Quat::identity() )
        {}
    };

    struct CharacterBody
    {
        CharacterParticles* particles;
        CharacterCenterOfMass com;

        i16 particleBegin;
        i16 particleCount;

        CharacterBody()
            : particles(0)
            , particleBegin(0)
            , particleCount(0)
        {}
    };

    struct CharacterInput
    {
        f32 analogX;
        f32 analogY;
        f32 jump;
        f32 crouch;
    };
    
    enum ECharacterAnimation_Pose
    {
        eANIM_POSE_WALK0,
        eANIM_POSE_WALK1,
        eANIM_POSE_WALK2,

        eANIM_POSE_COUNT,
    };
    struct CharacterAnimation
    {
        bxAnim_Skel* skel;
        bxAnim_Clip* clip;
        bxAnim_Context* ctx;

        void* poseMemoryHandle_;
        bxAnim_Joint* pose[eANIM_POSE_COUNT];

    };
    
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
            : maxInputForce( 1.15f )
            , jumpStrength( 15.f )
            , staticFriction( 0.1f )
            , dynamicFriction( 0.1f )
            , velocityDamping( 0.7f )
            , shapeStiffness( 0.1f )
            , shapeScale( 1.f )
            , gravity( 9.1f )
        {}
    };

    struct Character
    {
        CharacterAnimation anim;
        CharacterParticles particles;
        //CharacterCenterOfMass centerOfMass;
        CharacterBody bottomBody;
        CharacterBody spineBody;

        Vector3 upVector;

        CharacterInput input;
        CharacterParams params;
        bxPhx_Contacts* contacts;

        f32 _dtAcc;
        f32 _jumpAcc;
        u64 _timeMS;

        Character()
            : upVector( Vector3::yAxis() )
            , contacts( 0 )
            , _dtAcc( 0.f )
            , _jumpAcc( 0.f )
            , _timeMS( 0 )
        {}
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
            BX_CONTAINER_COPY_DATA( &newCP, cp, pos1 );
            BX_CONTAINER_COPY_DATA( &newCP, cp, velocity );
            BX_CONTAINER_COPY_DATA( &newCP, cp, mass );
            BX_CONTAINER_COPY_DATA( &newCP, cp, massInv );
        }

        BX_FREE( bxDefaultAllocator(), cp->memoryHandle );
        *cp = newCP;
    }


    
    void _CharacterAnimation_load( CharacterAnimation* ca, bxResourceManager* resourceManager )
    {
        ca->skel = bxAnimExt::loadSkelFromFile( resourceManager, "anim/human.skel" );
        ca->clip = bxAnimExt::loadAnimFromFile( resourceManager, "anim/run.anim" );

        if( ca->skel )
        {
            ca->ctx = bxAnim::contextInit( *ca->skel );
        }

        {
            const int numJoints = ca->skel->numJoints;

            int memSize = eANIM_POSE_COUNT * sizeof( bxAnim_Joint ) * numJoints;
            void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
            memset( mem, 0x00, memSize );

            bxBufferChunker chunker( mem, memSize );
            for( int ipose = 0; ipose < eANIM_POSE_COUNT; ++ipose )
            {
                ca->pose[ipose] = chunker.add< bxAnim_Joint >( numJoints );
            }
            chunker.check();

            bxAnim::evaluateClip( ca->pose[eANIM_POSE_WALK0], ca->clip, 26, 0 );
            bxAnim::evaluateClip( ca->pose[eANIM_POSE_WALK1], ca->clip, 30, 0 );
            bxAnim::evaluateClip( ca->pose[eANIM_POSE_WALK2], ca->clip, 47, 0 );

        }
    }
    void _CharacterAnimation_unload( CharacterAnimation* ca, bxResourceManager* resourceManager )
    {
        BX_FREE0( bxDefaultAllocator(), ca->poseMemoryHandle_ );
        memset( ca->pose, 0x00, sizeof( ca->pose ) );

        bxAnim::contextDeinit( &ca->ctx );
        bxAnimExt::unloadAnimFromFile( resourceManager, &ca->clip );
        bxAnimExt::unloadSkelFromFile( resourceManager, &ca->skel );
    }

    
    void _CharacterInput_collectData( CharacterInput* charInput, const bxInput& input, float deltaTime )
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
            charInput->jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
            charInput->crouch = signalFilter_lowPass( crouch, charInput->crouch, RC, deltaTime );

            //bxLogInfo( "x: %f, y: %f", charInput->analogX, charInput->jump );
        }
    }

    

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

        BX_DELETE0( bxDefaultAllocator(), c[0] );
    }

    void _CharacterBody_initCenterOfMass( CharacterBody* body, const Matrix4& worldPose )
    {
        CharacterParticles& cp = body->particles[0];

        int begin = body->particleBegin;
        int end = begin + body->particleCount;
        f32 massSum = 0.f;
        Vector3 com( 0.f );
        for ( int i = begin; i < end; ++i )
        {
            com += cp.pos0[i] * cp.mass[i];
            massSum += cp.mass[i];
        }
        com /= massSum;
        for ( int i = begin; i < end; ++i )
        {
            cp.restPos[i] = cp.pos0[i] - com;
            cp.pos0[i] = mulAsVec4( worldPose, cp.pos0[i] );
            cp.pos1[i] = cp.pos0[i];
        }

        body->com.pos = com;
        body->com.rot = Quat( worldPose.getUpper3x3() );
    }

    void character_init( Character* character, bxResourceManager* resourceManager, const Matrix4& worldPose )
    {
        const float a = 0.5f;
        const float b = 0.5f;
        const float c = 0.5f;
        const float d = 3.f;
        
        const int NUM_POINTS_BOTTOM = 8;
        const int NUM_POINTS_SPINE = 4;
        const int NUM_POINTS = NUM_POINTS_BOTTOM + NUM_POINTS_SPINE;
        const Vector3 localPoints[NUM_POINTS] =
        {
            Vector3( -b, 0.f, b ),
            Vector3(  b, 0.f, b ),
            Vector3(  b, 0.f,-b ),
            Vector3( -b, 0.f,-b ),

            Vector3( -a, c, a ),
            Vector3(  a, c, a ),
            Vector3(  a, c,-a ),
            Vector3( -a, c,-a ),

            Vector3( 0.f, lerp( 0.00f, 0.f, d ), 0.f ),
            Vector3( 0.f, lerp( 0.33f, 0.f, d ), 0.f ),
            Vector3( 0.f, lerp( 0.66f, 0.f, d ), 0.f ),
            Vector3( 0.f, lerp( 1.00f, 0.f, d ), 0.f ),
        };

        CharacterParticles& cp = character->particles;
        _CharacterParticles_allocateData( &cp, NUM_POINTS );
        cp.size = NUM_POINTS;

        //bxRandomGen rnd( 0xBAADC0DE );

        for ( int i = 0; i < NUM_POINTS; ++i )
        {
            //const Vector3 pos = mulAsVec4( worldPose, restPos[i] );
            //const Vector3 restPos( xyz_to_m128( shape.position( i ) ) );
            const Vector3 restPos = localPoints[i];
            const Vector3 pos = restPos;
            cp.pos0[i] = pos;
            cp.pos1[i] = pos;
            cp.velocity[i] = Vector3( 0.f );
            
            float mass = 1.f; // maxOfPair( 0.5f, c - restPos.getY().getAsFloat() ); // +maxOfPair( 0.f, dot( Vector3::zAxis() - Vector3::yAxis(), restPos ).getAsFloat() ) * 7;
            if ( i < 4 )
                mass += 2.f;

            if ( i == NUM_POINTS - 1 )
                mass = 2.f;

            cp.mass[i] = mass; // bxRand::randomFloat( rnd, 1.f, 0.5f );

            cp.massInv[i] = 1.f / mass;
        }

        character->bottomBody.particles = &character->particles;
        character->bottomBody.particleBegin = 0;
        character->bottomBody.particleCount = NUM_POINTS_BOTTOM;

        character->spineBody.particles = &character->particles;
        character->spineBody.particleBegin = NUM_POINTS_BOTTOM;
        character->spineBody.particleCount = NUM_POINTS_SPINE;

        _CharacterBody_initCenterOfMass( &character->bottomBody, worldPose );
        _CharacterBody_initCenterOfMass( &character->spineBody, worldPose );

        character->contacts = bxPhx::contacts_new( NUM_POINTS );
        _CharacterAnimation_load( &character->anim, resourceManager );
    }

    void character_deinit( Character* character, bxResourceManager* resourceManager )
    {
        bxPhx::contacts_delete( &character->contacts );
        BX_FREE0( bxDefaultAllocator(), character->particles.memoryHandle );
        _CharacterAnimation_unload( &character->anim, resourceManager );
    }


    namespace
    { 
        void _UpdateSoftBody( CharacterBody* body, float shapeScale, float shapeStiffness )
        {
            CharacterParticles* cp = body->particles;
            const int begin = body->particleBegin;
            const int count = body->particleCount;
            const int end = begin + count;
            
            Matrix3 R;
            Vector3 com;
            bxPhx::pbd_softBodyUpdatePose( &R, &com, cp->pos1 + begin, cp->restPos + begin, cp->mass + begin, count );
            body->com.pos = com;
            body->com.rot = Quat( R );

            const Matrix3 R1 = appendScale( R, Vector3( shapeScale ) );
            for ( int ipoint = begin; ipoint < end; ++ipoint )
            {
                Vector3 dpos( 0.f );
                bxPhx::pbd_solveShapeMatchingConstraint( &dpos, R1, com, cp->restPos[ipoint], cp->pos1[ipoint], shapeStiffness );
                cp->pos1[ipoint] += dpos;
            }
        }

        void _FinalizeSimulation( CharacterParticles* cp, bxPhx_Contacts* contacts, float staticFriction, float dynamicFriction, float deltaTime )
        {
            const int nContacts = bxPhx::contacts_size( contacts );
            
            Vector3 normal( 0.f );
            float depth = 0.f;
            u16 index0 = 0xFFFF;
            u16 index1 = 0xFFFF;

            for ( int icontact = 0; icontact < nContacts; ++icontact )
            {
                bxPhx::contacts_get( contacts, &normal, &depth, &index0, &index1, icontact );

                Vector3 dpos( 0.f );
                bxPhx::pbd_computeFriction( &dpos, cp->pos0[index0], cp->pos1[index0], normal, staticFriction, dynamicFriction );
                cp->pos1[index0] += dpos;
            }

            const floatInVec dtv( deltaTime );
            const floatInVec dtInv = select( zeroVec, oneVec / dtv, dtv > fltEpsVec );
            for ( int ipoint = 0; ipoint < cp->size; ++ipoint )
            {
                cp->velocity[ipoint] = (cp->pos1[ipoint] - cp->pos0[ipoint]) * dtInv;
                cp->pos0[ipoint] = cp->pos1[ipoint];
            }
        }

        void _BeginSimulation_Spine( Character* character, const Vector3& steeringForce, float deltaTime )
        {
            const CharacterParams& params = character->params;

            CharacterBody& body = character->spineBody;
            CharacterParticles& cp = body.particles[0];
            CharacterCenterOfMass& centerOfMass = body.com;

            const Vector3& upVector = character->upVector;
            const Vector3  dirVector = fastRotate( centerOfMass.rot, Vector3::zAxis() );
            const Vector3 currentCom = centerOfMass.pos;

            const Vector3 gravity = character->upVector * params.gravity;
            const floatInVec dtv( deltaTime );
            const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( params.velocityDamping ), dtv );
            //const int nPoints = cp.size;
            const int beginPoints = body.particleBegin;
            const int countPoints = body.particleCount;
            const int endPoints = beginPoints + countPoints;

            for ( int ipoint = beginPoints; ipoint < endPoints; ++ipoint )
            {
                Vector3 pos = cp.pos0[ipoint];
                Vector3 vel = cp.velocity[ipoint];

                vel += (gravity)* dtv;
                vel *= dampingCoeff;
                if ( ipoint == (endPoints-1) )
                {
                    vel += steeringForce;
                }
                pos += vel * dtv;
                
                cp.pos1[ipoint] = pos;
                cp.velocity[ipoint] = vel;
            }


            const Vector3 btmPoint = cp.pos1[beginPoints];
            const Vector3 topPoint = cp.pos1[endPoints - 1];
            const Vector3 up = topPoint - btmPoint;
            const floatInVec len = length( up );
            const Vector3 upDir = up / len;
            const float cosine = dot( upDir, upVector ).getAsFloat();
            const float angle = ::acos( clamp( cosine, -1.f, 1.f ) );
            const float maxAngle = PI / 16.f;
            if( angle > maxAngle )
            {
                const Vector3 axis = normalize( cross( upVector, upDir ) );
                const Vector3 newUp = fastRotate( Quat::rotation( floatInVec( maxAngle ), axis ), upVector );
                const Vector3 newTop = btmPoint + normalize( newUp ) * len;
                bxGfxDebugDraw::addSphere( Vector4( newTop, 0.1f ), 0xFF0000FF, true );
                cp.pos1[endPoints - 1] += ( newTop - topPoint ) * cp.massInv[endPoints-1];
            }
        

        }

        void _BeginSimulation_Bottom( Character* character, const Vector3& steeringForce, float deltaTime )
        {
            const CharacterParams& params = character->params;
            
            CharacterBody& body = character->bottomBody;
            CharacterParticles& cp = body.particles[0];
            CharacterCenterOfMass& centerOfMass = body.com;
            
            const Vector3& upVector = character->upVector;
            const Vector3  dirVector = fastRotate( centerOfMass.rot, Vector3::zAxis() );

            const Vector3 steeringForceXZ = projectVectorOnPlane( steeringForce, Vector4( upVector, 0.f ) );
            const Vector3 steeringForceY = upVector * dot( upVector, steeringForce );
            const Vector3 currentCom = centerOfMass.pos;

            const Vector3 gravity = -character->upVector * params.gravity;
            const floatInVec dtv( deltaTime );
            const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( params.velocityDamping ), dtv );
            //const int nPoints = cp.size;
            const int beginPoints = body.particleBegin;
            const int countPoints = body.particleCount;
            const int endPoints = beginPoints + countPoints;

            for ( int ipoint = beginPoints; ipoint < endPoints; ++ipoint )
            {
                Vector3 pos = cp.pos0[ipoint];
                Vector3 vel = cp.velocity[ipoint];

                vel += (gravity) * dtv;

                if ( ipoint < 4 )
                {
                    Vector3 steering = cross( cross( dirVector, steeringForceXZ ), pos - currentCom );
                    steering += steeringForceY;
                    steering += steeringForceXZ;

                    vel += steering * floatInVec( cp.massInv[ipoint] );
                }
                vel *= dampingCoeff;
                pos += vel * dtv;

                cp.pos1[ipoint] = pos;
                cp.velocity[ipoint] = vel;
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
        _CharacterInput_collectData( &character->input, input, deltaTime );
        Vector3 externalForces( 0.f );
        {
            externalForces += Vector3::xAxis() * character->input.analogX;
            externalForces -= Vector3::zAxis() * character->input.analogY;

            const floatInVec externalForcesValue = minf4( length( externalForces ), floatInVec( params.maxInputForce ) );
            externalForces = projectVectorOnPlane( camera.matrix.world.getUpper3x3() * externalForces, Vector4( character->upVector, oneVec ) );
            externalForces = normalizeSafe( externalForces ) * externalForcesValue;

            if ( character->_jumpAcc < params.jumpStrength*2 )
            {
                character->_jumpAcc += character->input.jump * params.jumpStrength;
            }

            bxGfxDebugDraw::addLine( character->bottomBody.com.pos, character->bottomBody.com.pos + externalForces + character->upVector*character->_jumpAcc, 0xFFFFFFFF, true );
        }

        const float fixedFreq = 60.f;
        const float fixedDt = 1.f / fixedFreq;

        character->_dtAcc += deltaTime;

        int iteration = 0;
        while ( character->_dtAcc >= fixedDt )
        {
            if( iteration == 0 )
            {
                externalForces += character->upVector * character->_jumpAcc;
            }
            _BeginSimulation_Spine( character, externalForces, fixedDt );
            
            Vector3 bottomSteeringForce( 0.f );
            {
                int spineFirst = character->spineBody.particleBegin;
                int spineLast = spineFirst + character->spineBody.particleCount - 1;
                Vector3 spineUpDir = normalize( character->particles.pos1[spineLast] - character->particles.pos1[spineFirst] );
                bottomSteeringForce = externalForces + ( spineUpDir - character->upVector );
            }
            _BeginSimulation_Bottom( character, bottomSteeringForce, fixedDt );

            {
                Vector3 pos0 = character->bottomBody.com.pos;
                Vector3 pos1 = character->particles.pos1[character->spineBody.particleBegin];
                float massInv1 = character->particles.massInv[character->spineBody.particleBegin];

                Vector3 dpos0, dpos1;
                bxPhx::pbd_solveDistanceConstraint( &dpos0, &dpos1, pos0, pos1, 0.f, massInv1, 0.f, 1.f, 1.f );

                character->particles.pos1[character->spineBody.particleBegin] = pos1 + dpos1;
            }

            {
                bxPhx::contacts_clear( character->contacts );
                bxPhx::collisionSpace_collide( cspace, character->contacts, character->particles.pos1, character->particles.size );
            }
            _UpdateSoftBody( &character->spineBody, params.shapeScale, 0.5f );
            _UpdateSoftBody( &character->bottomBody, params.shapeScale, params.shapeStiffness );
            _FinalizeSimulation( &character->particles, character->contacts, params.staticFriction, params.dynamicFriction, fixedDt );

            character->_dtAcc -= fixedDt;
            character->_jumpAcc = 0.f;
            ++iteration;
        }


        Matrix4 animationRoot = Matrix4::identity();
        {
            int spineFirst = character->spineBody.particleBegin;
            int spineLast = spineFirst + character->spineBody.particleCount - 1;
            Vector3 spineUpDir = normalize( character->particles.pos1[spineLast] - character->particles.pos1[spineFirst] );
            Vector3 sideDir = fastRotate( character->bottomBody.com.rot, Vector3::xAxis() );
            Vector3 frontDir = normalize( cross( sideDir, spineUpDir ) );

            animationRoot = Matrix4( Matrix3( sideDir, spineUpDir, frontDir ), character->bottomBody.com.pos );
        }

        {
            CharacterAnimation& anim = character->anim;

            const float time = (float)( (double)character->_timeMS * 0.001 );
            bxAnim_Joint* localJoints = bxAnim::poseFromStack( anim.ctx, 0 );
            bxAnim_Joint* worldJoints = bxAnim::poseFromStack( anim.ctx, 1 );
            bxAnim::evaluateClip( localJoints, anim.clip, time * 0.5f );

            //float blendFactor = (::sin( time ));// *0.5f ) + 0.5f;
            //bxAnim_Joint* leftJoints = anim.pose[eANIM_POSE_WALK0];
            //bxAnim_Joint* rightJoints = (blendFactor > 0.f) ? anim.pose[eANIM_POSE_WALK1] : anim.pose[eANIM_POSE_WALK2];
            //blendFactor = smoothstep( 0.f, 1.f, ::abs( blendFactor ) );
            //bxAnim::blendJointsLinear( localJoints, leftJoints, rightJoints, blendFactor, anim.skel->numJoints );

            bxAnim_Joint rootJoint = bxAnim_Joint::identity();
            rootJoint.position = animationRoot.getTranslation();
            rootJoint.rotation = Quat( animationRoot.getUpper3x3() );

            bxAnimExt::localJointsToWorldJoints( worldJoints, localJoints, anim.skel, rootJoint );

            const i16* parentIndices = TYPE_OFFSET_GET_POINTER( const i16, anim.skel->offsetParentIndices );
            for( int ijoint = 0; ijoint < anim.skel->numJoints; ++ijoint )
            {
                const bxAnim_Joint& joint = worldJoints[ijoint];
                bxGfxDebugDraw::addSphere( Vector4( joint.position, 0.05f ), 0xFF0000FF, true );
                if( parentIndices[ijoint] != -1 )
                {
                    const bxAnim_Joint& parentJoint = worldJoints[parentIndices[ijoint]];
                    bxGfxDebugDraw::addLine( joint.position, parentJoint.position, 0xFF0000FF, true );
                }
            }
        }

        character->_timeMS += (u64)( (double)deltaTime * 1000.0 );

        CharacterParticles& cp = character->particles;
        for ( int i = 0; i < cp.size; ++i )
        {
            bxGfxDebugDraw::addSphere( Vector4( cp.pos0[i], 0.01f * cp.mass[i] ), 0x002200FF, true );
        }
        const Vector3& com = character->bottomBody.com.pos;
        const Matrix3 R( character->bottomBody.com.rot );
        bxGfxDebugDraw::addLine( com, com + R[0], 0x990000FF, true );
        bxGfxDebugDraw::addLine( com, com + R[1], 0x009900FF, true );
        bxGfxDebugDraw::addLine( com, com + R[2], 0x000099FF, true );

        _CharacterParams_show( &character->params );



    }

    Matrix4 character_pose( const Character* character )
    {
        return Matrix4( character->bottomBody.com.rot, character->bottomBody.com.pos );
    }

    Vector3 character_upVector( const Character* character )
    {
        return character->upVector;
    }



}///

