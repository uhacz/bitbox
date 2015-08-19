#include "game_character1.h"
#include "physics.h"
#include "physics_pbd.h"

#include <util/memory.h>
#include <util/signal_filter.h>
#include <util/buffer_utils.h>

#include <resource_manager/resource_manager.h>
#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_camera.h>

namespace bxGame{

Character1* character1_new()
{
    Character1* ch = BX_NEW( bxDefaultAllocator(), Character1 );

    ch->upVector = Vector3::yAxis();
    ch->contacts = 0;
    ch->_dtAcc = 0.f;
    ch->_jumpAcc = 0.f;
    memset( &ch->particles, 0x00, sizeof( ParticleData ) );
    memset( &ch->mainBodyConstraints, 0x00, sizeof( bxGame::Constraint ) );
    memset( &ch->input, 0x00, sizeof( Character1::Input ) );

    return ch;
}
void character1_delete( Character1** character )
{
    if( !character[0] )
        return;

    BX_DELETE0( bxDefaultAllocator(), character[0] );
}
void character1_init( Character1* character, bxResourceManager* resourceManager, const Matrix4& worldPose )
{
    particleDataAllocate( &character->particles, Character1::eTOTAL_PARTICLE_COUNT );        

    character->mainBody.begin = 0;
    character->mainBody.end = Character1::eMAIN_BODY_PARTICLE_COUNT;

    character->wheelBody.begin = character->mainBody.end;
    character->wheelBody.end = character->wheelBody.begin + Character1::eWHEEL_BODY_PARTICLE_COUNT;

    CharacterInternal::initMainBody( character, worldPose );
    character->particles.size = 3;

    character->contacts = bxPhx::contacts_new( Character1::eTOTAL_PARTICLE_COUNT );
}
void character1_deinit( Character1* character, bxResourceManager* resourceManager )
{
    bxPhx::contacts_delete( &character->contacts );
    particleDataFree( &character->particles );
}

void character1_tick( Character1* character, bxPhx_CollisionSpace* cspace, const bxGfxCamera& camera, const bxInput& input, float deltaTime )
{
    CharacterInternal::collectInputData( &character->input, input, deltaTime );
    Vector3 externalForces( 0.f );
    {
        externalForces += Vector3::xAxis() * character->input.analogX;
        externalForces -= Vector3::zAxis() * character->input.analogY;

        const float maxInputForce = 0.5f;
        const floatInVec externalForcesValue = minf4( length( externalForces ), floatInVec( maxInputForce ) );
        externalForces = projectVectorOnPlane( camera.matrix.world.getUpper3x3() * externalForces, Vector4( character->upVector, oneVec ) );
        externalForces = normalizeSafe( externalForces ) * externalForcesValue;
        character->_jumpAcc += character->input.jump;

        //bxGfxDebugDraw::addLine( character->bottomBody.com.pos, character->bottomBody.com.pos + externalForces + character->upVector*character->_jumpAcc, 0xFFFFFFFF, true );
    }

    const float staticFriction = 0.1f;
    const float dynamicFriction = 0.1f;

    const float fixedFreq = 60.f;
    const float fixedDt = 1.f / fixedFreq;

    character->_dtAcc += deltaTime;

    //const floatInVec dtv( fixedDt );
    //const floatInVec dtvInv = select( zeroVec, oneVec / dtv, dtv > fltEpsVec );

    while( character->_dtAcc >= fixedDt )
    {
        CharacterInternal::simulateMainBodyBegin( character, externalForces, fixedDt );

        bxPhx::contacts_clear( character->contacts );
        bxPhx::collisionSpace_collide( cspace, character->contacts, character->particles.pos1, Character1::eTOTAL_PARTICLE_COUNT );

        CharacterInternal::simulateFinalize( character, staticFriction, dynamicFriction, fixedDt );
        CharacterInternal::computeCharacterPose( character );

        character->_dtAcc -= fixedDt;
        character->_jumpAcc = 0.f;
    }

    CharacterInternal::debugDraw( character );
}

Matrix4 character1_pose( const Character1* ch )
{
    return Matrix4( Matrix3(  ch->sideVector, ch->upVector, ch->frontVector ), ch->footPos );
}
Vector3 character1_upVector( const Character1* ch )
{
    return ch->upVector;
}

//////////////////////////////////////////////////////////////////////////
void particleDataAllocate( ParticleData* data, int newcap )
{
    if( newcap <= data->capacity )
        return;

    int memSize = 0;
    memSize += newcap * sizeof( *data->pos0 );
    memSize += newcap * sizeof( *data->pos1 );
    memSize += newcap * sizeof( *data->vel );
    memSize += newcap * sizeof( *data->mass );
    memSize += newcap * sizeof( *data->massInv );

    void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
    memset( mem, 0x00, memSize );

    bxBufferChunker chunker( mem, memSize );

    ParticleData newdata;
    newdata.memoryHandle = mem;
    newdata.pos0 = chunker.add< Vector3 >( newcap );
    newdata.pos1 = chunker.add< Vector3 >( newcap );
    newdata.vel  = chunker.add< Vector3 >( newcap );
    newdata.mass = chunker.add< f32 >( newcap );
    newdata.massInv = chunker.add< f32 >( newcap );
    chunker.check();

    newdata.size = data->size;
    newdata.capacity = newcap;

    if( data->size )
    {
        BX_CONTAINER_COPY_DATA( &newdata, data, pos0 );
        BX_CONTAINER_COPY_DATA( &newdata, data, pos1 );
        BX_CONTAINER_COPY_DATA( &newdata, data, vel );
        BX_CONTAINER_COPY_DATA( &newdata, data, mass );
        BX_CONTAINER_COPY_DATA( &newdata, data, massInv );
    }

    BX_FREE( bxDefaultAllocator(), data->memoryHandle );
    *data = newdata;
}
void particleDataFree( ParticleData* data )
{
    BX_FREE( bxDefaultAllocator(), data->memoryHandle );
    memset( data, 0x00, sizeof( ParticleData ) );
}




}///

namespace bxGame{ namespace CharacterInternal {

void debugDraw( Character1* character )
{
    ParticleData* cp = &character->particles;
    const int nPoint = cp->size;
    for( int ipoint = 0; ipoint < nPoint; ++ipoint )
    {
        bxGfxDebugDraw::addSphere( Vector4( cp->pos0[ipoint], 0.05f ), 0x00FF00FF, true );
    }

    const int nConstraint = Character1::eMAIN_BODY_CONSTRAINT_COUNT;
    for( int iconstraint = 0; iconstraint < nConstraint; ++iconstraint )
    {
        const Constraint& c = character->mainBodyConstraints[iconstraint];
        bxGfxDebugDraw::addLine( cp->pos0[c.i0], cp->pos1[c.i1], 0xFF0000FF, true );
    }

    bxGfxDebugDraw::addLine( character->footPos, character->footPos + character->sideVector, 0xFF0000FF, true );
    bxGfxDebugDraw::addLine( character->footPos, character->footPos + character->upVector, 0x00FF00FF, true );
    bxGfxDebugDraw::addLine( character->footPos, character->footPos + character->frontVector, 0x0000FFFF, true );

}
void collectInputData( Character1::Input* charInput, const bxInput& input, float deltaTime )
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
        charInput->jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
        charInput->crouch = signalFilter_lowPass( crouch, charInput->crouch, RC, deltaTime );

        //bxLogInfo( "x: %f, y: %f", charInput->analogX, charInput->jump );
    }
}


static inline void initConstraint( Constraint* c, const Vector3& p0, const Vector3& p1, int i0, int i1 )
{
    c->i0 = (i16)i0;
    c->i1 = (i16)i1;
    c->d = length( p0 - p1 ).getAsFloat();
}

void initMainBody( Character1* ch, const Matrix4& worldPose )
{
    ParticleData& p = ch->particles;
    Character1::Body body = ch->mainBody;

    const float a = 0.5f;
    const float b = 0.4f;

    const Matrix3 worldRot = worldPose.getUpper3x3();
    const Vector3 axisX = worldRot.getCol0();
    const Vector3 axisY = worldRot.getCol1();
    const Vector3 axisZ = worldRot.getCol2();
    const Vector3 worldPos = worldPose.getTranslation();

    p.pos0[body.begin  ] = worldPos + axisZ * a * 0.5f;
    p.pos0[body.begin+1] = worldPos + axisX * b - axisZ * a * 0.5f;
    p.pos0[body.begin+2] = worldPos - axisX * b - axisZ * a * 0.5f;

    for( int i = body.begin; i < body.end; ++i )
    {
        p.pos1[i] = p.pos0[i];
        p.vel[i] = Vector3( 0.f );

        float mass = 1.f;
        if( i == 0 )
            mass += 2.f;

        p.mass[i] = mass;
        p.massInv[i] = 1.f / mass;
    }
    
{
        initConstraint( &ch->mainBodyConstraints[0], p.pos0[body.begin + 0], p.pos0[body.begin + 1], body.begin + 0, body.begin + 1 );
        initConstraint( &ch->mainBodyConstraints[1], p.pos0[body.begin + 0], p.pos0[body.begin + 2], body.begin + 0, body.begin + 2 );
        initConstraint( &ch->mainBodyConstraints[2], p.pos0[body.begin + 1], p.pos0[body.begin + 2], body.begin + 1, body.begin + 2 );
    }
}

void simulateMainBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime )
{
    ParticleData* cp = &ch->particles;
    Character1::Body body = ch->mainBody;

    const floatInVec dtv( deltaTime );
    const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( 0.1f ), dtv );
    const Vector3 gravity = -ch->upVector * 9.1f;
    const Vector3 jumpVector = ch->upVector * ch->_jumpAcc * 5.f;


    const int nPoint = body.count();
    for( int ipoint = body.begin; ipoint < body.end; ++ipoint )
    {
        Vector3 pos = cp->pos0[ipoint];
        Vector3 vel = cp->vel[ipoint];

        vel += (gravity)* dtv;
        vel *= dampingCoeff;

        if( ipoint == body.begin )
        {
            vel += extForce;
        }
        vel += jumpVector;
        pos += vel * dtv;

        cp->pos1[ipoint] = pos;
        cp->vel[ipoint] = vel;
    }

    const int nConstraint = Character1::eMAIN_BODY_CONSTRAINT_COUNT;
    for( int iter = 0; iter < 4; ++iter )
    {
        for( int iconstraint = 0; iconstraint < nConstraint; ++iconstraint )
        {
            const Constraint& c = ch->mainBodyConstraints[iconstraint];

            const Vector3& p0 = cp->pos1[c.i0];
            const Vector3& p1 = cp->pos1[c.i1];
            const float massInv0 = cp->massInv[c.i0];
            const float massInv1 = cp->massInv[c.i1];

            Vector3 dpos0, dpos1;
            bxPhx::pbd_solveDistanceConstraint( &dpos0, &dpos1, p0, p1, massInv0, massInv1, c.d, 1.f, 1.f );

            cp->pos1[c.i0] += dpos0;
            cp->pos1[c.i1] += dpos1;
        }
    }
}

void simulateFinalize( Character1* ch, float staticFriction, float dynamicFriction, float deltaTime )
{
    bxPhx_Contacts* contacts = ch->contacts;
    const int nContacts = bxPhx::contacts_size( contacts );

    Vector3 normal( 0.f );
    float depth = 0.f;
    u16 index0 = 0xFFFF;
    u16 index1 = 0xFFFF;

    ParticleData* cp = &ch->particles;

    for( int icontact = 0; icontact < nContacts; ++icontact )
    {
        bxPhx::contacts_get( contacts, &normal, &depth, &index0, &index1, icontact );

        Vector3 dpos( 0.f );
        bxPhx::pbd_computeFriction( &dpos, cp->pos0[index0], cp->pos1[index0], normal, staticFriction, dynamicFriction );
        cp->pos1[index0] += dpos;
    }

    const floatInVec dtvInv( ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f );

    for( int ipoint = 0; ipoint < cp->size; ++ipoint )
    {
        cp->vel[ipoint] = ( cp->pos1[ipoint] - cp->pos0[ipoint] ) * dtvInv;
        cp->pos0[ipoint] = cp->pos1[ipoint];
    }
}

void computeCharacterPose( Character1* ch )
{
    Vector3 com( 0.f );
    float massSum = 0.f;

    ParticleData* cp = &ch->particles;
    Character1::Body body = ch->mainBody;

    for( int ipoint = body.begin; ipoint < body.end; ++ipoint )
    {
        com += cp->pos0[ipoint] * cp->mass[ipoint];
        massSum += cp->mass[ipoint];
    }
    com /= massSum;

    Vector3 dir = normalize( cp->pos0[body.begin] - com );
    Vector3 side = normalize( cross( ch->upVector, dir ) );

    ch->frontVector = dir;
    ch->sideVector = side;
    ch->footPos = com;
}

}}///




