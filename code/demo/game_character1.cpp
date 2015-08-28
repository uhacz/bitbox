#include "game_character1.h"
#include "physics.h"
#include "physics_pbd.h"

#include <util/memory.h>
#include <util/signal_filter.h>
#include <util/buffer_utils.h>
#include <util/common.h>

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
    memset( &ch->constraints, 0x00, sizeof( ConstraintData ) );
    memset( &ch->restPos, 0x00, sizeof( BodyRestPosition ) );
    memset( &ch->mainBody, 0x00, sizeof( Character1::Body ) );
    memset( &ch->wheelBody, 0x00, sizeof( Character1::Body ) );
    //memset( ch->wheelRestPos, 0x00, sizeof( ch->wheelRestPos ) );
    //memset( &ch->mainBodyConstraints, 0x00, sizeof( bxGame::Constraint ) );
    //memset( &ch->wheelBodyConstraints, 0x00, sizeof( bxGame::Constraint ) );
    memset( &ch->input, 0x00, sizeof( Character1::Input ) );

    return ch;
}
void character1_delete( Character1** character )
{
    if( !character[0] )
        return;

    BX_DELETE0( bxDefaultAllocator(), character[0] );
}

static inline int computeShapeParticleCount( int nIterations )
{
    int outer = ( int )::pow( 2 + nIterations, 3.0 );
    int inner = ( int )::pow( nIterations, 3.0 );
    return ( outer - inner ) + 1;
}

void character1_init( Character1* ch, bxResourceManager* resourceManager, const Matrix4& worldPose )
{
    const int BODY_SHAPE_ITERATIONS = 3;
    
    ch->mainBody.particleBegin = 0;
    ch->mainBody.particleEnd = 3;
    ch->mainBody.constraintBegin = 0;
    ch->mainBody.constraintEnd = 3;

    ch->wheelBody.particleBegin = 3;
    ch->wheelBody.particleEnd = ch->wheelBody.particleBegin + computeShapeParticleCount( BODY_SHAPE_ITERATIONS );
    ch->wheelBody.constraintBegin = 3;
    ch->wheelBody.constraintEnd = ch->wheelBody.constraintBegin + 3;

    int nConstraints = 0;
    nConstraints += ch->mainBody.constaintCount();
    nConstraints += ch->wheelBody.constaintCount();

    int nPoints = 0;
    nPoints += ch->mainBody.particleCount();
    nPoints += ch->wheelBody.particleCount();

    ParticleData::alloc( &ch->particles, nPoints );
    ch->particles.size = nPoints;

    ConstraintData::alloc( &ch->constraints, nConstraints );
    ch->constraints.size = nConstraints;

    BodyRestPosition::alloc( &ch->restPos, ch->wheelBody.particleCount() );

    CharacterInternal::initMainBody( ch, worldPose );
    CharacterInternal::initWheelBody( ch, BODY_SHAPE_ITERATIONS, worldPose );
    
    ch->contacts = bxPhx::contacts_new( nPoints );
}
void character1_deinit( Character1* character, bxResourceManager* resourceManager )
{
    bxPhx::contacts_delete( &character->contacts );
    BodyRestPosition::free( &character->restPos );
    ConstraintData::free( &character->constraints );
    ParticleData::free( &character->particles );
}

void character1_tick( Character1* character, bxPhx_CollisionSpace* cspace, const bxGfxCamera& camera, const bxInput& input, float deltaTime )
{
    CharacterInternal::collectInputData( &character->input, input, deltaTime );
    Vector3 externalForces( 0.f );
    {
        externalForces += Vector3::xAxis() * character->input.analogX;
        externalForces -= Vector3::zAxis() * character->input.analogY;

        const float maxInputForce = 0.25f;
        const floatInVec externalForcesValue = minf4( length( externalForces ), floatInVec( maxInputForce ) );
        externalForces = projectVectorOnPlane( camera.matrix.world.getUpper3x3() * externalForces, Vector4( character->upVector, oneVec ) );
        externalForces = normalizeSafe( externalForces ) * externalForcesValue;
        character->_jumpAcc += character->input.jump * 2;

        //bxGfxDebugDraw::addLine( character->bottomBody.com.pos, character->bottomBody.com.pos + externalForces + character->upVector*character->_jumpAcc, 0xFFFFFFFF, true );
    }

    const float staticFriction = 0.95f;
    const float dynamicFriction = 0.9f;
    const float shapeStiffness = 0.15f;

    const float fixedFreq = 60.f;
    const float fixedDt = 1.f / fixedFreq;

    character->_dtAcc += deltaTime;

    //const floatInVec dtv( fixedDt );
    //const floatInVec dtvInv = select( zeroVec, oneVec / dtv, dtv > fltEpsVec );

    while( character->_dtAcc >= fixedDt )
    {
        CharacterInternal::simulateMainBodyBegin( character, externalForces, fixedDt );
        CharacterInternal::simulateWheelBodyBegin( character, externalForces, fixedDt );

        bxPhx::contacts_clear( character->contacts );
        bxPhx::collisionSpace_collide( cspace, character->contacts, character->particles.pos1, character->particles.size );

        CharacterInternal::simulateWheelUpdatePose( character, 1.f, shapeStiffness );

        CharacterInternal::simulateFinalize( character, staticFriction, dynamicFriction, fixedDt );
        CharacterInternal::computeCharacterPose( character );

        character->_dtAcc -= fixedDt;
        character->_jumpAcc = 0.f;
    }

    CharacterInternal::debugDraw( character );
}

Matrix4 character1_pose( const Character1* ch )
{
    return Matrix4( Matrix3(  ch->sideVector, ch->upVector, ch->frontVector ), ch->feetCenterPos );
}
Vector3 character1_upVector( const Character1* ch )
{
    return ch->upVector;
}

//////////////////////////////////////////////////////////////////////////
void ParticleData::alloc( ParticleData* data, int newcap )
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

    ParticleData newdata;
    newdata.memoryHandle = mem;
    newdata.size = data->size;
    newdata.capacity = newcap;

    bxBufferChunker chunker( mem, memSize );
    newdata.pos0 = chunker.add< Vector3 >( newcap );
    newdata.pos1 = chunker.add< Vector3 >( newcap );
    newdata.vel  = chunker.add< Vector3 >( newcap );
    newdata.mass = chunker.add< f32 >( newcap );
    newdata.massInv = chunker.add< f32 >( newcap );
    chunker.check();

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
void ParticleData::free( ParticleData* data )
{
    BX_FREE( bxDefaultAllocator(), data->memoryHandle );
    memset( data, 0x00, sizeof( ParticleData ) );
}


void ConstraintData::alloc( ConstraintData* data, int newcap )
{
    if( newcap <= data->capacity )
        return;

    int memSize = 0;
    memSize += newcap * sizeof( *data->desc );

    void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 4 );
    memset( mem, 0x00, memSize );

    ConstraintData newdata;
    newdata.memoryHandle = mem;
    newdata.capacity = newcap;
    newdata.size = data->size;

    bxBufferChunker chunker( mem, memSize );
    newdata.desc = chunker.add< Constraint >( newcap );
    chunker.check();

    if( data->size )
    {
        BX_CONTAINER_COPY_DATA( &newdata, data, desc );
    }

    BX_FREE( bxDefaultAllocator(), data->memoryHandle );
    *data = newdata;
}

void ConstraintData::free( ConstraintData* data )
{
    BX_FREE( bxDefaultAllocator(), data->memoryHandle );
    memset( data, 0x00, sizeof( ConstraintData ) );
}


void BodyRestPosition::alloc( BodyRestPosition* data, int newcap )
{
    int memSize = 0;
    memSize += newcap * sizeof( *data->box );
    memSize += newcap * sizeof( *data->sphere );
    memSize += newcap * sizeof( *data->current );

    void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
    memset( mem, 0x00, memSize );

    BodyRestPosition newdata;
    newdata.memoryHandle = mem;
    newdata.size = newcap;

    bxBufferChunker chunker( mem, memSize );
    newdata.box = chunker.add< Vector3 >( newcap );
    newdata.sphere = chunker.add< Vector3 >( newcap );
    newdata.current = chunker.add< Vector3 >( newcap );
    chunker.check();

    if( data->size )
    {
        BX_CONTAINER_COPY_DATA( &newdata, data, box );
        BX_CONTAINER_COPY_DATA( &newdata, data, sphere );
        BX_CONTAINER_COPY_DATA( &newdata, data, current );
    }

    BX_FREE( bxDefaultAllocator(), data->memoryHandle );
    *data = newdata;
}

void BodyRestPosition::free( BodyRestPosition* data )
{
    BX_FREE0( bxDefaultAllocator(), data->memoryHandle );
    memset( data, 0x00, sizeof( BodyRestPosition ) );
}

}///

namespace bxGame{ namespace CharacterInternal {

void debugDraw( Character1* character )
{
    ParticleData* cp = &character->particles;
    const int nPoint = cp->size;
    for( int ipoint = character->mainBody.particleBegin; ipoint < character->mainBody.particleEnd; ++ipoint )
    {
        bxGfxDebugDraw::addSphere( Vector4( cp->pos0[ipoint], 0.05f ), 0x00FF00FF, true );
    }

    const int wheelBegin = character->wheelBody.particleBegin;
    const int wheelEnd = character->wheelBody.particleEnd;
    for( int ipoint = wheelBegin; ipoint < wheelEnd; ++ipoint )
    {
        bxGfxDebugDraw::addSphere( Vector4( cp->pos0[ipoint], 0.05f ), 0x333333FF, true );
    }
    bxGfxDebugDraw::addSphere( Vector4( character->leftFootPos, 0.075f ), 0xFFFF00FF, true );
    bxGfxDebugDraw::addSphere( Vector4( character->rightFootPos, 0.075f ), 0xFFFF00FF, true );


    for( int iconstraint = character->mainBody.constraintBegin; iconstraint < character->mainBody.constraintEnd; ++iconstraint )
    {
        const Constraint& c = character->constraints.desc[iconstraint];
        bxGfxDebugDraw::addLine( cp->pos0[c.i0], cp->pos1[c.i1], 0xFF0000FF, true );
    }

    for ( int iconstraint = character->wheelBody.constraintBegin; iconstraint < character->wheelBody.constraintEnd; ++iconstraint )
    {
        const Constraint& c = character->constraints.desc[iconstraint];
        bxGfxDebugDraw::addLine( cp->pos0[c.i0], cp->pos1[c.i1], 0xFF0000FF, true );
    }

    bxGfxDebugDraw::addLine( character->feetCenterPos, character->feetCenterPos + character->sideVector, 0xFF0000FF, true );
    bxGfxDebugDraw::addLine( character->feetCenterPos, character->feetCenterPos + character->upVector, 0x00FF00FF, true );
    bxGfxDebugDraw::addLine( character->feetCenterPos, character->feetCenterPos + character->frontVector, 0x0000FFFF, true );

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
static inline void projectDistanceConstraints( ParticleData* pdata, const Constraint* constraints, int nConstraints, float stiffness, int iterationCount )
{
    for( int iter = 0; iter < iterationCount; ++iter )
    {
        for( int iconstraint = 0; iconstraint < nConstraints; ++iconstraint )
        {
            const Constraint& c = constraints[iconstraint];

            const Vector3& p0 = pdata->pos1[c.i0];
            const Vector3& p1 = pdata->pos1[c.i1];
            const float massInv0 = pdata->massInv[c.i0];
            const float massInv1 = pdata->massInv[c.i1];

            Vector3 dpos0, dpos1;
            bxPhx::pbd_solveDistanceConstraint( &dpos0, &dpos1, p0, p1, massInv0, massInv1, c.d, stiffness, stiffness );

            pdata->pos1[c.i0] += dpos0;
            pdata->pos1[c.i1] += dpos1;
        }
    }
}

void initMainBody( Character1* ch, const Matrix4& worldPose )
{
    ParticleData& p = ch->particles;
    Character1::Body& body = ch->mainBody;

    const float a = 0.5f;
    const float b = 0.4f;

    const Matrix3 worldRot = worldPose.getUpper3x3();
    const Vector3 axisX = worldRot.getCol0();
    const Vector3 axisY = worldRot.getCol1();
    const Vector3 axisZ = worldRot.getCol2();
    const Vector3 worldPos = worldPose.getTranslation();

    p.pos0[0] = worldPos + axisZ * a * 0.5f;
    p.pos0[1] = worldPos + axisX * b - axisZ * a * 0.5f;
    p.pos0[2] = worldPos - axisX * b - axisZ * a * 0.5f;


    float massSum = 0.f;
    body.com.pos = Vector3( 0.f );
    body.com.rot = Quat( worldPose.getUpper3x3() );

    for( int i = body.particleBegin; i < body.particleEnd; ++i )
    {
        p.pos1[i] = p.pos0[i];
        p.vel[i] = Vector3( 0.f );

        float mass = 2.f;
        if( i == 0 )
            mass += 4.f;

        p.mass[i] = mass;
        p.massInv[i] = 1.f / mass;

        body.com.pos += p.pos0[0];
        massSum += mass;
    }

    body.com.pos /= massSum;
    
    {
        int csBegin = body.constraintBegin;
        int pBegin = body.particleBegin;
        
        Constraint* cs = ch->constraints.desc + csBegin;
        Vector3* pos0 = ch->particles.pos0 + pBegin;

        initConstraint( &cs[0], pos0[0], pos0[1], pBegin + 0, pBegin + 1 );
        initConstraint( &cs[1], pos0[0], pos0[2], pBegin + 0, pBegin + 2 );
        initConstraint( &cs[2], pos0[1], pos0[2], pBegin + 1, pBegin + 2 );
    }
}


void initWheelBody( Character1* ch, int shapeIterations, const Matrix4& worldPose )
{
    const int nPoints = computeShapeParticleCount( shapeIterations );

    ParticleData& p = ch->particles;
    Character1::Body& body = ch->wheelBody;

    const float a = 0.5f;
    const float b = 0.5f;

    Vector3* localPointsBox = ch->restPos.box;
    Vector3* localPointsSphere = ch->restPos.sphere;

    const int nEdgePoints = shapeIterations + 2;
    const float step = 1.f / ( nEdgePoints - 1 );
    int pointCounter = 0;

    localPointsBox[0] = Vector3( 0.f );
    localPointsSphere[0] = Vector3( 0.f );
    ++pointCounter;

    for( int iz = 0; iz < nEdgePoints; ++iz )
    {
        const float z = -0.5f + iz * step;
        const bool edgez = iz == 0 || iz == ( nEdgePoints - 1 );
        
        for( int iy = 0; iy < nEdgePoints; ++iy )
        {
            const float y = -0.5f + iy * step;
            const bool edgey = iy == 0 || iy == ( nEdgePoints - 1 );
            
            for( int ix = 0; ix < nEdgePoints; ++ix )
            {
                const bool edgex = ix == 0 || ix == ( nEdgePoints - 1 );
                
                if( !edgex && !edgey && !edgez )
                    continue;
                
                const float x = -0.5f + ix * step;

                SYS_ASSERT( pointCounter < nPoints );

                Vector3 pos( x, y, z );

                localPointsBox[pointCounter] = pos;
                localPointsSphere[pointCounter] = normalize( pos );

                ++pointCounter;
            }
        }
    }


    //Vector3 localPoints[Character1::eWHEEL_BODY_PARTICLE_COUNT] =
    //{
    //    Vector3( -a*0.66f, 0.f, 0.f ),
    //    Vector3(  a*0.66f, 0.f, 0.f ),

    //    Vector3( 0.f, 1.f, 0.f ),
    //    Vector3( 0.f, 1.f, 1.f ),
    //    Vector3( 0.f, 0.f, 1.f ),
    //    Vector3( 0.f,-1.f, 1.f ),
    //    
    //    Vector3( 0.f,-1.f, 0.f ),
    //    Vector3( 0.f,-1.f,-1.f ),
    //    Vector3( 0.f, 0.f,-1.f ),
    //    Vector3( 0.f, 1.f,-1.f ),
    //};
    Vector3* localPointsCurrent = ch->restPos.current;
    for( int ipoint = 0; ipoint < nPoints; ++ipoint )
    {
        localPointsBox[ipoint] = mulPerElem( localPointsBox[ipoint], Vector3( a, b, a ) );
        localPointsSphere[ipoint] = mulPerElem( localPointsSphere[ipoint], Vector3( a, b, a ) );
        localPointsCurrent[ipoint] = localPointsSphere[ipoint];
    }


    for( int ipoint = body.particleBegin, irestpos = 0; ipoint < body.particleEnd; ++ipoint, ++irestpos )
    {
        SYS_ASSERT( irestpos < nPoints );

        const Vector3 restPos = localPointsCurrent[irestpos];
        p.pos0[ipoint] = restPos;
        p.pos1[ipoint] = restPos;
        p.vel[ipoint] = Vector3( 0.f );

        float mass = 0.01f;
        p.mass[ipoint] = mass;
        p.massInv[ipoint] = 1.f / mass;
    }

    f32 massSum = 0.f;
    Vector3 com( 0.f );
    for( int ipoint = body.particleBegin; ipoint < body.particleEnd; ++ipoint )
    {
        com += p.pos0[ipoint] * p.mass[ipoint];
        massSum += p.mass[ipoint];
    }
    com /= massSum;
    for( int ipoint = body.particleBegin, irestpos = 0; ipoint < body.particleEnd; ++ipoint, ++irestpos )
    {
        //ch->wheelRestPos[irestpos] = p.pos0[ipoint] - com;
        p.pos0[ipoint] = mulAsVec4( worldPose, p.pos0[ipoint] );
        p.pos1[ipoint] = p.pos0[ipoint];
    }

    body.com.pos = com;
    body.com.rot = Quat( worldPose.getUpper3x3() );

    int mainBegin = ch->mainBody.particleBegin;
    int shapeBegin = body.particleBegin;

    const Vector3* mainPos0 = ch->particles.pos0 + mainBegin;
    const Vector3* shapePos0 = ch->particles.pos0 + shapeBegin;

    Constraint* cs = ch->constraints.desc + body.constraintBegin;
    initConstraint( cs + 0, mainPos0[0], shapePos0[0], mainBegin+0, shapeBegin );
    initConstraint( cs + 1, mainPos0[1], shapePos0[0], mainBegin+1, shapeBegin );
    initConstraint( cs + 2, mainPos0[2], shapePos0[0], mainBegin+2, shapeBegin );

    //int left = body.particleBegin;
    //int right = body.particleBegin + 1;
    //
    //initConstraint( &ch->wheelBodyConstraints[0], p.pos0[left], p.pos0[right], left, right);

    //initConstraint( &ch->wheelBodyConstraints[1], p.pos0[0], p.pos0[left], 0, left );
    //initConstraint( &ch->wheelBodyConstraints[2], p.pos0[1], p.pos0[left], 1, left );
    //initConstraint( &ch->wheelBodyConstraints[3], p.pos0[2], p.pos0[left], 2, left );

    //initConstraint( &ch->wheelBodyConstraints[4], p.pos0[0], p.pos0[right], 0, right );
    //initConstraint( &ch->wheelBodyConstraints[5], p.pos0[1], p.pos0[right], 1, right );
    //initConstraint( &ch->wheelBodyConstraints[6], p.pos0[2], p.pos0[right], 2, right );
}

struct PBD_Simulate
{
    template< class TVelPlug >
    static void predictPosition( ParticleData* cp, int pointBegin, int pointEnd, const Vector3& gravityVec, float damping, float deltaTime, TVelPlug& velPlug )
    {
        const floatInVec dtv( deltaTime );
        const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( damping ), dtv );

        for( int ipoint = pointBegin; ipoint < pointEnd; ++ipoint )
        {
            Vector3 pos = cp->pos0[ipoint];
            Vector3 vel = cp->vel[ipoint];

            vel += gravityVec * dtv;
            vel *= dampingCoeff;

            velPlug( vel, ipoint, deltaTime );
            
            pos += vel * dtv;

            cp->pos1[ipoint] = pos;
            cp->vel[ipoint] = vel;
        }
    }

    static void applyFriction( ParticleData* cp, bxPhx_Contacts* contacts, float staticFriction, float dynamicFriction )
    {
        const int nContacts = bxPhx::contacts_size( contacts );

        Vector3 normal( 0.f );
        float depth = 0.f;
        u16 index0 = 0xFFFF;
        u16 index1 = 0xFFFF;

        for( int icontact = 0; icontact < nContacts; ++icontact )
        {
            bxPhx::contacts_get( contacts, &normal, &depth, &index0, &index1, icontact );

            //float fd = ( index0 < Character1::eMAIN_BODY_PARTICLE_COUNT ) ? dynamicFriction : 0.6f;

            Vector3 dpos( 0.f );
            bxPhx::pbd_computeFriction( &dpos, cp->pos0[index0], cp->pos1[index0], normal, depth, staticFriction, dynamicFriction );
            cp->pos1[index0] += dpos;
        }
    }

    static void softBodyUpdatePose( Vector3* comPos, Quat* comRot, ParticleData* cp, int pointBegin, int pointEnd, const Vector3* restPositions, float shapeStiffness )
    {
        const int pointCount = pointEnd - pointBegin;
        SYS_ASSERT( pointCount > 0 );

        Matrix3 R;
        Vector3 com;
        bxPhx::pbd_softBodyUpdatePose( &R, &com, cp->pos1 + pointBegin, restPositions, cp->mass + pointBegin, pointCount );
        comPos[0] = com;
        comRot[0] = normalize( Quat( R ) );

        //const Matrix3 R1 = appendScale( R, Vector3( shapeScale ) );
        for( int ipoint = pointBegin, irestpos = 0; ipoint < pointEnd; ++ipoint, ++irestpos )
        {
            SYS_ASSERT( irestpos < pointCount );
            Vector3 dpos( 0.f );
            bxPhx::pbd_solveShapeMatchingConstraint( &dpos, R, com, restPositions[irestpos], cp->pos1[ipoint], shapeStiffness );
            cp->pos1[ipoint] += dpos;
        }
    }

    static void updateVelocity( ParticleData* cp, int pointBegin, int pointEnd, float deltaTimeInv )
    {
        for( int ipoint = pointBegin; ipoint < pointEnd; ++ipoint )
        {
            cp->vel[ipoint] = ( cp->pos1[ipoint] - cp->pos0[ipoint] ) * deltaTimeInv;
            cp->pos0[ipoint] = cp->pos1[ipoint];
        }
    }

    template< class TParamEval >
    static void projectDistanceConstraint( ParticleData* cp, const Constraint* constraints, int nConstraints, float stiffness, TParamEval paramEval )
    {
        for( int iconstraint = 0; iconstraint < nConstraints; ++iconstraint )
        {
            const Constraint& c = constraints[iconstraint];

            const Vector3& p0 = cp->pos1[c.i0];
            const Vector3& p1 = cp->pos1[c.i1];

            float massInv0 = cp->massInv[c.i0];
            float massInv1 = cp->massInv[c.i1];
            float len = c.d;

            paramEval( massInv0, massInv1, len, c, iconstraint );

            Vector3 dpos0, dpos1;
            bxPhx::pbd_solveDistanceConstraint( &dpos0, &dpos1, p0, p1, massInv0, massInv1, c.d, 1.f, 1.f );

            cp->pos1[c.i0] += dpos0;
            cp->pos1[c.i1] += dpos1;
        }
    }
};

struct PBD_VelPlug_Null
{
    void operator() ( Vector3& vel, int ipoint, float deltaTime ) { (void)vel; (void)ipoint; }
};

struct PBD_VelPlug_MainBody
{
    Vector3 inputVec;
    Vector3 jumpVec;
    Character1::Body* body;
    
    void operator() ( Vector3& vel, int ipoint, float deltaTime )
    {
        if( ipoint == body->particleBegin )
        {
            vel += inputVec;
        }

        vel += jumpVec;
    }
};

struct PBD_ConstraintParamEval_Null
{
    void operator() ( float& massInv0, float& massInv1, float& restLength, const Constraint& c, int iconstrait )
    {
        (void)massInv0;
        (void)massInv1;
        (void)restLength;
        (void)c;
        (void)iconstrait;
    }
};

struct PBD_ConstraintParamEval_Wheel
{
    void operator() ( float& massInv0, float& massInv1, float& restLength, const Constraint& c, int iconstrait )
    {
        if ( iconstrait != 0 )
            massInv0 = 0.f;
    }
};

void simulateMainBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime )
{
    ParticleData* cp = &ch->particles;
    const Character1::Body& body = ch->mainBody;

    //const floatInVec dtv( deltaTime );
    //const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( 0.1f ), dtv );
    
    const Vector3 gravity = -ch->upVector * 9.1f;
    const Vector3 jumpVector = ch->upVector * ch->_jumpAcc * 5.f;

    PBD_VelPlug_MainBody velPlug;
    velPlug.body = &ch->mainBody;
    velPlug.inputVec = extForce;
    velPlug.jumpVec = jumpVector;

    PBD_Simulate::predictPosition( cp, body.particleBegin, body.particleEnd, gravity, 0.5f, deltaTime, velPlug );
    
    const Constraint* cs = ch->constraints.desc + body.constraintBegin;
    for( int iiter = 0; iiter < 4; ++iiter )
        PBD_Simulate::projectDistanceConstraint( cp, cs, body.constaintCount(), 1.f, PBD_ConstraintParamEval_Null() );

    //const int nPoint = body.count();
    //for( int ipoint = body.begin; ipoint < body.end; ++ipoint )
    //{
    //    Vector3 pos = cp->pos0[ipoint];
    //    Vector3 vel = cp->vel[ipoint];

    //    vel += (gravity)* dtv;
    //    vel *= dampingCoeff;

    //    if( ipoint == body.begin )
    //    {
    //        vel += extForce;
    //    }
    //    vel += jumpVector;
    //    pos += vel * dtv;

    //    cp->pos1[ipoint] = pos;
    //    cp->vel[ipoint] = vel;
    //}

    //projectDistanceConstraints( cp, ch->mainBodyConstraints, Character1::eMAIN_BODY_CONSTRAINT_COUNT, 1.f, 4 );
}

void simulateWheelBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime )
{
    ParticleData* cp = &ch->particles;
    const Character1::Body& body = ch->wheelBody;

    //const floatInVec dtv( deltaTime );
    //const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( 0.9f ), dtv );
    const Vector3 gravity = -ch->upVector * 9.1f;
    //const Vector3 jumpVector = ch->upVector * ch->_jumpAcc * 5.f;

    PBD_Simulate::predictPosition( cp, body.particleBegin, body.particleEnd, gravity, 0.9f, deltaTime, PBD_VelPlug_Null() );
    
    const Constraint* cs = ch->constraints.desc + body.constaintCount();
    for( int iiter = 0; iiter < 4; ++iiter )
        PBD_Simulate::projectDistanceConstraint( cp, cs, body.constaintCount(), 1.f, PBD_ConstraintParamEval_Wheel() );

    //const int nPoint = body.count();
    //for( int ipoint = body.begin; ipoint < body.end; ++ipoint )
    //{
    //    Vector3 pos = cp->pos0[ipoint];
    //    Vector3 vel = cp->vel[ipoint];

    //    vel += (gravity)* dtv;
    //    vel *= dampingCoeff;

    //    //vel += jumpVector;
    //    pos += vel * dtv;

    //    cp->pos1[ipoint] = pos;
    //    cp->vel[ipoint] = vel;
    //}

    //for( int iter = 0; iter < 4; ++iter )
    //{
    //    for( int iconstraint = 0; iconstraint < Character1::eWHEEL_BODY_CONSTRAINT_COUNT; ++iconstraint )
    //    {
    //        const Constraint& c = ch->wheelBodyConstraints[iconstraint];

    //        const Vector3& p0 = cp->pos1[c.i0];
    //        const Vector3& p1 = cp->pos1[c.i1];
    //        const float massInv0 = (iconstraint == 0) ? cp->massInv[c.i0] : 0.f;
    //        const float massInv1 = cp->massInv[c.i1];

    //        Vector3 dpos0, dpos1;
    //        bxPhx::pbd_solveDistanceConstraint( &dpos0, &dpos1, p0, p1, massInv0, massInv1, c.d, 1.f, 1.f );

    //        cp->pos1[c.i0] += dpos0;
    //        cp->pos1[c.i1] += dpos1;
    //    }
    //}

    //projectDistanceConstraints( cp, ch->wheelBodyConstraints, Character1::eWHEEL_BODY_CONSTRAINT_COUNT, 0.1f, 4 );
}

void simulateWheelUpdatePose( Character1* ch, float shapeScale, float shapeStiffness )
{
    ParticleData* cp = &ch->particles;
    Character1::Body& body = ch->wheelBody;

    Matrix3 R;
    Vector3 com;
    bxPhx::pbd_softBodyUpdatePose( &R, &com, cp->pos1 + body.particleBegin, ch->restPos.current, cp->mass + body.particleBegin, body.particleCount() );
    body.com.pos = com;
    body.com.rot = normalize( Quat( R ) );

    const Matrix3 R1 = appendScale( R, Vector3( shapeScale ) );
    for( int ipoint = body.particleBegin, irestpos = 0; ipoint < body.particleEnd; ++ipoint, ++irestpos )
    {
        SYS_ASSERT( irestpos < body.particleCount() );
        Vector3 dpos( 0.f );
        bxPhx::pbd_solveShapeMatchingConstraint( &dpos, R1, com, ch->restPos.current[irestpos], cp->pos1[ipoint], shapeStiffness );
        cp->pos1[ipoint] += dpos;
    }
}

void simulateFinalize( Character1* ch, float staticFriction, float dynamicFriction, float deltaTime )
{
    const float deltaTimeInv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
    
    PBD_Simulate::applyFriction( &ch->particles, ch->contacts, staticFriction, dynamicFriction );
    
    //bxPhx_Contacts* contacts = ch->contacts;
    //const int nContacts = bxPhx::contacts_size( contacts );

    //Vector3 normal( 0.f );
    //float depth = 0.f;
    //u16 index0 = 0xFFFF;
    //u16 index1 = 0xFFFF;

    //ParticleData* cp = &ch->particles;

    //for( int icontact = 0; icontact < nContacts; ++icontact )
    //{
    //    bxPhx::contacts_get( contacts, &normal, &depth, &index0, &index1, icontact );

    //    float fd = ( index0 < Character1::eMAIN_BODY_PARTICLE_COUNT ) ? dynamicFriction : 0.6f;

    //    Vector3 dpos( 0.f );
    //    bxPhx::pbd_computeFriction( &dpos, cp->pos0[index0], cp->pos1[index0], normal, depth, staticFriction, fd );
    //    cp->pos1[index0] += dpos;
    //}

    PBD_Simulate::updateVelocity( &ch->particles, 0, ch->particles.size, deltaTimeInv );

    //const floatInVec dtvInv( ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f );

    //for( int ipoint = 0; ipoint < cp->size; ++ipoint )
    //{
    //    cp->vel[ipoint] = ( cp->pos1[ipoint] - cp->pos0[ipoint] ) * dtvInv;
    //    cp->pos0[ipoint] = cp->pos1[ipoint];
    //}
}

void computeCharacterPose( Character1* ch )
{
    Vector3 com( 0.f );
    float massSum = 0.f;

    ParticleData* cp = &ch->particles;
    Character1::Body body = ch->mainBody;

    for( int ipoint = body.particleBegin; ipoint < body.particleEnd; ++ipoint )
    {
        com += cp->pos0[ipoint] * cp->mass[ipoint];
        massSum += cp->mass[ipoint];
    }
    com /= massSum;

    Vector3 dir = normalize( cp->pos0[body.particleBegin] - com );
    Vector3 side = normalize( cross( ch->upVector, dir ) );

    ch->frontVector = dir;
    ch->sideVector = side;
    ch->feetCenterPos = com;

    body.com.pos = com;
    body.com.rot = Quat( Matrix3( dir, ch->upVector, side ) );


    const int leftFootParticleIndex = ch->wheelBody.particleBegin + 2;
    const int rightFootParticleIndex = ch->wheelBody.particleBegin + 6;
    const float sideLen = 0.35f;

    const Vector3& leftVelocity = ch->particles.vel[leftFootParticleIndex];
    const Vector3& rightVelocity = ch->particles.vel[rightFootParticleIndex];

    const float leftSpeed = length( leftVelocity ).getAsFloat();
    const float rightSpeed = length( rightVelocity ).getAsFloat();

    const Vector3 footOffsetX = side * sideLen;
    
    Vector3 leftWalkPos = ch->particles.pos0[leftFootParticleIndex] - footOffsetX;
    Vector3 rightWalkPos = ch->particles.pos0[rightFootParticleIndex] + footOffsetX;

    const Vector4 footPlane = makePlane( ch->upVector, com );
    Vector3 leftRestPos = projectPointOnPlane( leftWalkPos, footPlane );
    Vector3 rightRestPos = projectPointOnPlane( rightWalkPos, footPlane );

    

    const float leftAlpha = smoothstep( 0.f, 2.f, leftSpeed );
    const float rightAlpha = smoothstep( 0.f, 2.f, rightSpeed );

    ch->leftFootPos = lerp( leftAlpha, leftRestPos, leftWalkPos );
    ch->rightFootPos = lerp( rightAlpha, rightRestPos, rightWalkPos );
}

}}///




