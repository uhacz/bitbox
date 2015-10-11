#include "game_character.h"
#include "physics.h"
#include "physics_pbd.h"

#include <util/memory.h>
#include <util/signal_filter.h>
#include <util/buffer_utils.h>
#include <util/common.h>
#include <util/poly/poly_shape.h>

#include <gdi/gdi_render_source.h>

#include <resource_manager/resource_manager.h>
#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_camera.h>
#include "gfx/gfx_material.h"

namespace bxGame{

Character* character_new()
{
    Character* ch = BX_NEW( bxDefaultAllocator(), Character );

    ch->upVector = Vector3::yAxis();
    ch->contacts = 0;
    ch->_dtAcc = 0.f;
    ch->_jumpAcc = 0.f;
    ch->_shapeBlendAlpha = 0.f;

    memset( &ch->particles, 0x00, sizeof( ParticleData ) );
    memset( &ch->constraints, 0x00, sizeof( ConstraintData ) );
    memset( &ch->restPos, 0x00, sizeof( BodyRestPosition ) );
    //memset( &ch->mainBody, 0x00, sizeof( Character1::Body ) );
    memset( &ch->shapeMeshData, 0x00, sizeof( MeshData ) );
    memset( &ch->shapeBody, 0x00, sizeof( Character::Body ) );
    //memset( ch->wheelRestPos, 0x00, sizeof( ch->wheelRestPos ) );
    //memset( &ch->mainBodyConstraints, 0x00, sizeof( bxGame::Constraint ) );
    //memset( &ch->wheelBodyConstraints, 0x00, sizeof( bxGame::Constraint ) );
    memset( &ch->input, 0x00, sizeof( Character::Input ) );

    return ch;
}
void character_delete( Character** character )
{
    if( !character[0] )
        return;

    BX_DELETE0( bxDefaultAllocator(), character[0] );
}

static inline int computeShapeParticleCount( int nIterations )
{
    //int outer = ( int )::pow( 2 + nIterations, 3.0 );
    //int inner = ( int )::pow( nIterations, 3.0 );
    //return ( outer - inner ) + 1;

    int iter[6] = { nIterations, nIterations, nIterations, nIterations, nIterations, nIterations };
    return bxPolyShape_computeNumPoints( iter );
}
static inline int computeShapeTriangleCount( int nIterations )
{
    int iter[6] = { nIterations, nIterations, nIterations, nIterations, nIterations, nIterations };
    return bxPolyShape_computeNumTriangles( iter );
}

void character_init( Character* ch, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGfx_World* gfxWorld, const Matrix4& worldPose )
{
    const int BODY_SHAPE_ITERATIONS = 3;
    
    //ch->mainBody.particleBegin = 0;
    //ch->mainBody.particleEnd = 3;
    //ch->mainBody.constraintBegin = 0;
    //ch->mainBody.constraintEnd = 3;

    ch->shapeBody.particleBegin = 0;
    ch->shapeBody.particleEnd = ch->shapeBody.particleBegin + computeShapeParticleCount( BODY_SHAPE_ITERATIONS );
    //ch->wheelBody.constraintBegin = 3;
    //ch->wheelBody.constraintEnd = ch->wheelBody.constraintBegin + 3;

    int nConstraints = 0;
    //nConstraints += ch->mainBody.constaintCount();
    //nConstraints += ch->wheelBody.constaintCount();

    int nPoints = 0;
    //nPoints += ch->mainBody.particleCount();
    nPoints += ch->shapeBody.particleCount();

    int nIndices = 0;
    nIndices += computeShapeTriangleCount( BODY_SHAPE_ITERATIONS ) * 3;

    MeshData::alloc( &ch->shapeMeshData, nIndices );


    ParticleData::alloc( &ch->particles, nPoints );
    ch->particles.size = nPoints;

    //ConstraintData::alloc( &ch->constraints, nConstraints );
    //ch->constraints.size = nConstraints;

    BodyRestPosition::alloc( &ch->restPos, ch->shapeBody.particleCount() );

    //CharacterInternal::initMainBody( ch, worldPose );
    CharacterInternal::initShapeBody( ch, BODY_SHAPE_ITERATIONS, worldPose );
    CharacterInternal::initShapeMesh( ch, dev, resourceManager, gfxWorld );
    ch->contacts = bxPhx::contacts_new( nPoints );
}
void character_deinit( Character* character, bxResourceManager* resourceManager )
{
    bxPhx::contacts_delete( &character->contacts );
    CharacterInternal::deinitShapeMesh( character );
    BodyRestPosition::free( &character->restPos );
    ConstraintData::free( &character->constraints );
    ParticleData::free( &character->particles );
    MeshData::free( &character->shapeMeshData );
}

void character_tick( Character* character, bxPhx_CollisionSpace* cspace, bxGdiContextBackend* ctx, const bxGfxCamera& camera, const bxInput& input, float deltaTime )
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
        character->_jumpAcc += character->input.jump * 10;

        //bxGfxDebugDraw::addLine( character->bottomBody.com.pos, character->bottomBody.com.pos + externalForces + character->upVector*character->_jumpAcc, 0xFFFFFFFF, true );
    }

    {
        float dAlpha = 0.f;
        dAlpha -= character->input.L2 * deltaTime;
        dAlpha += character->input.R2 * deltaTime;

        if( ::abs( dAlpha ) > FLT_EPSILON )
        {
            character->_shapeBlendAlpha += dAlpha;
            character->_shapeBlendAlpha = clamp( character->_shapeBlendAlpha, 0.f, 1.f );

            Character::Body& body = character->shapeBody;
            for( int ipoint = body.particleBegin; ipoint < body.particleEnd; ++ipoint )
            {
                const Vector3& a = character->restPos.sphere[ipoint];
                const Vector3& b = character->restPos.box[ipoint];

                character->restPos.current[ipoint] = lerp( character->_shapeBlendAlpha, a, b );
            }
        }

    }

    const float staticFriction = 0.95f;
    const float dynamicFriction = 0.6f;
    const float shapeStiffness = 0.4f;

    const float fixedFreq = 60.f;
    const float fixedDt = 1.f / fixedFreq;

    character->_dtAcc += deltaTime;

    //const floatInVec dtv( fixedDt );
    //const floatInVec dtvInv = select( zeroVec, oneVec / dtv, dtv > fltEpsVec );

    const int jump = character->_jumpAcc > FLT_EPSILON;
    if ( jump )
    {
        externalForces *= dot( externalForces, character->upVector );
    }

    const bool doIteration = character->_dtAcc >= fixedDt;

    while( character->_dtAcc >= fixedDt )
    {
    
        //CharacterInternal::simulateMainBodyBegin( character, externalForces, fixedDt );
        CharacterInternal::simulateShapeBodyBegin( character, externalForces, fixedDt );

        bxPhx::contacts_clear( character->contacts );
        bxPhx::collisionSpace_collide( cspace, character->contacts, character->particles.pos1, character->particles.size );

        CharacterInternal::simulateShapeUpdatePose( character, 1.f, shapeStiffness );

        CharacterInternal::simulateFinalize( character, staticFriction, dynamicFriction, fixedDt );
        CharacterInternal::computeCharacterPose( character );

        character->_dtAcc -= fixedDt;
        character->_jumpAcc = 0.f;
    }

    if( doIteration )
    {
        bxGdiRenderSource* rsource = bxGfxExt::meshInstanceRenderSource( character->shapeMeshI );
        const bxGdiVertexBuffer& vbuffer = bxGdi::renderSourceVertexBuffer( rsource, 0 );
        MeshVertex* vertices = (MeshVertex*)ctx->mapVertices( vbuffer, 0, vbuffer.numElements, bxGdi::eMAP_WRITE );
        CharacterInternal::updateMesh( vertices, character->particles.pos0, character->particles.size, character->shapeMeshData.indices, character->shapeMeshData.nIndices );
        ctx->unmapVertices( vbuffer );
    }

    //CharacterInternal::debugDraw( character );
}

Matrix4 character_pose( const Character* ch )
{
    //return Matrix4( Matrix3(  ch->sideVector, ch->upVector, ch->frontVector ), ch->feetCenterPos );
    return Matrix4( ch->shapeBody.com.rot, ch->shapeBody.com.pos );
}
Vector3 character_upVector( const Character* ch )
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


void MeshData::alloc( MeshData* data, int newcap )
{
    int memSize = 0;
    memSize += newcap * sizeof( *data->indices );

    void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 2 );
    memset( mem, 0x00, memSize );

    data->indices = (u16*)mem;
    data->nIndices = newcap;
}

void MeshData::free( MeshData* data )
{
    BX_FREE0( bxDefaultAllocator(), data->indices );
    memset( data, 0x00, sizeof( MeshData ) );
}

}///

namespace bxGame{ namespace CharacterInternal {

void debugDraw( Character* character )
{
    ParticleData* cp = &character->particles;
    const int nPoint = cp->size;
    //for( int ipoint = character->mainBody.particleBegin; ipoint < character->mainBody.particleEnd; ++ipoint )
    //{
    //    bxGfxDebugDraw::addSphere( Vector4( cp->pos0[ipoint], 0.05f ), 0x00FF00FF, true );
    //}

    const int wheelBegin = character->shapeBody.particleBegin;
    const int wheelEnd = character->shapeBody.particleEnd;
    for( int ipoint = wheelBegin; ipoint < wheelEnd; ++ipoint )
    {
        bxGfxDebugDraw::addSphere( Vector4( cp->pos0[ipoint], 0.05f ), 0x333333FF, true );
    }
    bxGfxDebugDraw::addSphere( Vector4( character->leftFootPos, 0.075f ), 0xFFFF00FF, true );
    bxGfxDebugDraw::addSphere( Vector4( character->rightFootPos, 0.075f ), 0xFFFF00FF, true );


    //for( int iconstraint = character->mainBody.constraintBegin; iconstraint < character->mainBody.constraintEnd; ++iconstraint )
    //{
    //    const Constraint& c = character->constraints.desc[iconstraint];
    //    bxGfxDebugDraw::addLine( cp->pos0[c.i0], cp->pos1[c.i1], 0xFF0000FF, true );
    //}

    //for ( int iconstraint = character->wheelBody.constraintBegin; iconstraint < character->wheelBody.constraintEnd; ++iconstraint )
    //{
    //    const Constraint& c = character->constraints.desc[iconstraint];
    //    bxGfxDebugDraw::addLine( cp->pos0[c.i0], cp->pos1[c.i1], 0xFF0000FF, true );
    //}


    Matrix3 comRot( character->shapeBody.com.rot );
    Vector3 comPos( character->shapeBody.com.pos );
    bxGfxDebugDraw::addLine( comPos, comPos + comRot.getCol0(), 0xFF0000FF, true );
    bxGfxDebugDraw::addLine( comPos, comPos + comRot.getCol1(), 0x00FF00FF, true );
    bxGfxDebugDraw::addLine( comPos, comPos + comRot.getCol2(), 0x0000FFFF, true );

}
void collectInputData( Character::Input* charInput, const bxInput& input, float deltaTime )
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
        const int inL2 = bxInput_isKeyPressed( kbd, 'Q' );
        const int inR2 = bxInput_isKeyPressed( kbd, 'E' );

        const float analogX = -(float)inLeft + (float)inRight;
        const float analogY = -(float)inBack + (float)inFwd;

        const float crouch = (f32)inCrouch;
        const float jump = (f32)inJump;

        const float RC = 0.01f;
        charInput->analogX = signalFilter_lowPass( analogX, charInput->analogX, RC, deltaTime );
        charInput->analogY = signalFilter_lowPass( analogY, charInput->analogY, RC, deltaTime );
        charInput->jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
        charInput->crouch = signalFilter_lowPass( crouch, charInput->crouch, RC, deltaTime );
        charInput->L2 = (float)inL2;
        charInput->R2 = (float)inR2;
        //bxLogInfo( "x: %f, y: %f", charInput->analogX, charInput->jump );
    }
}


static inline void initConstraint( Constraint* c, const Vector3& p0, const Vector3& p1, int i0, int i1 )
{
    c->i0 = (i16)i0;
    c->i1 = (i16)i1;
    c->d = length( p0 - p1 ).getAsFloat();
}
//static inline void projectDistanceConstraints( ParticleData* pdata, const Constraint* constraints, int nConstraints, float stiffness, int iterationCount )
//{
//    for( int iter = 0; iter < iterationCount; ++iter )
//    {
//        for( int iconstraint = 0; iconstraint < nConstraints; ++iconstraint )
//        {
//            const Constraint& c = constraints[iconstraint];
//
//            const Vector3& p0 = pdata->pos1[c.i0];
//            const Vector3& p1 = pdata->pos1[c.i1];
//            const float massInv0 = pdata->massInv[c.i0];
//            const float massInv1 = pdata->massInv[c.i1];
//
//            Vector3 dpos0, dpos1;
//            bxPhx::pbd_solveDistanceConstraint( &dpos0, &dpos1, p0, p1, massInv0, massInv1, c.d, stiffness, stiffness );
//
//            pdata->pos1[c.i0] += dpos0;
//            pdata->pos1[c.i1] += dpos1;
//        }
//    }
//}

//void initMainBody( Character1* ch, const Matrix4& worldPose )
//{
//    ParticleData& p = ch->particles;
//    Character1::Body& body = ch->mainBody;
//
//    const float a = 0.5f;
//    const float b = 0.4f;
//
//    const Matrix3 worldRot = worldPose.getUpper3x3();
//    const Vector3 axisX = worldRot.getCol0();
//    const Vector3 axisY = worldRot.getCol1();
//    const Vector3 axisZ = worldRot.getCol2();
//    const Vector3 worldPos = worldPose.getTranslation();
//
//    p.pos0[0] = worldPos + axisZ * a * 0.5f;
//    p.pos0[1] = worldPos + axisX * b - axisZ * a * 0.5f;
//    p.pos0[2] = worldPos - axisX * b - axisZ * a * 0.5f;
//
//
//    float massSum = 0.f;
//    body.com.pos = Vector3( 0.f );
//    body.com.rot = Quat( worldPose.getUpper3x3() );
//
//    for( int i = body.particleBegin; i < body.particleEnd; ++i )
//    {
//        p.pos1[i] = p.pos0[i];
//        p.vel[i] = Vector3( 0.f );
//
//        float mass = 2.f;
//        if( i == 0 )
//            mass += 4.f;
//
//        p.mass[i] = mass;
//        p.massInv[i] = 1.f / mass;
//
//        body.com.pos += p.pos0[0];
//        massSum += mass;
//    }
//
//    body.com.pos /= massSum;
//    
//    {
//        int csBegin = body.constraintBegin;
//        int pBegin = body.particleBegin;
//        
//        Constraint* cs = ch->constraints.desc + csBegin;
//        Vector3* pos0 = ch->particles.pos0 + pBegin;
//
//        initConstraint( &cs[0], pos0[0], pos0[1], pBegin + 0, pBegin + 1 );
//        initConstraint( &cs[1], pos0[0], pos0[2], pBegin + 0, pBegin + 2 );
//        initConstraint( &cs[2], pos0[1], pos0[2], pBegin + 1, pBegin + 2 );
//    }
//}

void updateMesh( MeshVertex* vertices, const Vector3* points, int nPoints, const u16* indices, int nIndices )
{
    for( int ipoint = 0; ipoint < nPoints; ++ipoint )
    {
        storeXYZ( points[ipoint], vertices[ipoint].pos );
        memset( vertices[ipoint].nrm, 0x00, 12 );
    }

    SYS_ASSERT( (nIndices % 3) == 0 );
    for ( int iindex = 0; iindex < nIndices; iindex += 3 )
    {
        u16 i0 = indices[iindex];
        u16 i1 = indices[iindex+1];
        u16 i2 = indices[iindex+2];

        const Vector3& p0 = points[i0];
        const Vector3& p1 = points[i1];
        const Vector3& p2 = points[i2];

        const Vector3 v01 = p1 - p0;
        const Vector3 v02 = p2 - p0;

        const Vector3 n = cross( v01, v02 );
        const SSEScalar ns( n.get128() );

        MeshVertex& vtx0 = vertices[i0];
        MeshVertex& vtx1 = vertices[i1];
        MeshVertex& vtx2 = vertices[i2];

        Vector3 n0, n1, n2;
        loadXYZ( n0, vtx0.nrm );
        loadXYZ( n1, vtx1.nrm );
        loadXYZ( n2, vtx2.nrm );

        n0 += n;
        n1 += n;
        n2 += n;

        storeXYZ( n0, vtx0.nrm );
        storeXYZ( n1, vtx1.nrm );
        storeXYZ( n2, vtx2.nrm );
    }

    for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
    {
        Vector3 n;
        loadXYZ( n, vertices[ipoint].nrm );
        storeXYZ( normalize( n ), vertices[ipoint].nrm );
    }
}

void initShapeMesh( Character* ch, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGfx_World* gfxWorld )
{
    const ParticleData& p = ch->particles;
    const Character::Body& body = ch->shapeBody;
    const int nPoints = body.particleCount();

    MeshVertex* vertices = (MeshVertex*)BX_MALLOC( bxDefaultAllocator(), nPoints * sizeof( MeshVertex ), 4 );
    updateMesh( vertices, p.pos0, nPoints, ch->shapeMeshData.indices, ch->shapeMeshData.nIndices );

    bxGfx_StreamsDesc sdesc( nPoints, ch->shapeMeshData.nIndices );
    sdesc.vstreamBegin().addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 3 ).addBlock( bxGdi::eSLOT_NORMAL, bxGdi::eTYPE_FLOAT, 3, 1 );
    sdesc.vstreamEnd();
    sdesc.istreamSet( bxGdi::eTYPE_USHORT, ch->shapeMeshData.indices );

    bxGfx_HMesh hmesh = bxGfx::meshCreate();
    bxGfx::meshStreamsSet( hmesh, dev, sdesc );
    bxGfx::meshShaderSet( hmesh, dev, resourceManager, bxGfxMaterialManager::findMaterial( "red" ) );

    //bxGfx_HInstanceBuffer hinst = bxGfx::instanceBuffeCreate( 1 );
    //ch->shapeMeshI = bxGfx::worldMeshAdd( gfxWorld, hmesh, 1 );

    bxGfx_HInstanceBuffer hinst = bxGfx::meshInstanceHInstanceBuffer( ch->shapeMeshI );
    Matrix4 pose = Matrix4::identity();
    //bxGfx::instanceBufferDataSet( hinst, &pose, 1 );
    
    BX_FREE0( bxDefaultAllocator(), vertices );
}

void deinitShapeMesh( Character* ch )
{
    bxGfx::worldMeshRemoveAndRelease( &ch->shapeMeshI );
}

void initShapeBody( Character* ch, int shapeIterations, const Matrix4& worldPose )
{
    const int nPoints = computeShapeParticleCount( shapeIterations );

    ParticleData& p = ch->particles;
    Character::Body& body = ch->shapeBody;

    const float a = 0.5f;
    const float b = 0.5f;

    Vector3* localPointsBox = ch->restPos.box;
    Vector3* localPointsSphere = ch->restPos.sphere;

    bxPolyShape shape;
    bxPolyShape_createBox( &shape, shapeIterations );
    
    SYS_ASSERT( shape.nvertices() == nPoints );

    for( int ipoint = 0; ipoint < nPoints; ++ipoint )
    {
        Vector3 pos;
        loadXYZ( pos, shape.position( ipoint ) );
        localPointsBox[ipoint] = pos;
        localPointsSphere[ipoint] = normalize( pos );
    }

    SYS_ASSERT( ch->shapeMeshData.nIndices == shape.num_indices );
    SYS_ASSERT( nPoints < 0xFFFFFF );
    for( int iindex = 0; iindex < shape.num_indices; ++iindex )
    {
        ch->shapeMeshData.indices[iindex] = (u16)shape.indices[iindex];
    }

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

        const float* uv = shape.texcoord( irestpos );
        const Vector3 restPos = localPointsCurrent[irestpos];
        p.pos0[ipoint] = restPos;
        p.pos1[ipoint] = restPos;
        p.vel[ipoint] = Vector3( 0.f );

        float mass = 1.0f;

        bool edge0 = uv[0] < FLT_EPSILON || uv[0] > ( 1.f - FLT_EPSILON );
        bool edge1 = uv[1] < FLT_EPSILON || uv[1] > ( 1.f - FLT_EPSILON );

        if( edge0 && edge1 )
        {
            mass /= 3.f;
        }
        else if( (!edge0 && edge1) || (edge0 && !edge1) )
        {
            mass /= 2.f;
        }
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

    body.com.pos = mulAsVec4( worldPose, com );
    body.com.rot = Quat( worldPose.getUpper3x3() );

    //const int nEdgePoints = shapeIterations + 2;
    //const float step = 1.f / ( nEdgePoints - 1 );
    //int pointCounter = 0;

    //localPointsBox[0] = Vector3( 0.f );
    //localPointsSphere[0] = Vector3( 0.f );
    //++pointCounter;

    //for( int iz = 0; iz < nEdgePoints; ++iz )
    //{
    //    const float z = -0.5f + iz * step;
    //    const bool edgez = iz == 0 || iz == ( nEdgePoints - 1 );
    //    
    //    for( int iy = 0; iy < nEdgePoints; ++iy )
    //    {
    //        const float y = -0.5f + iy * step;
    //        const bool edgey = iy == 0 || iy == ( nEdgePoints - 1 );
    //        
    //        for( int ix = 0; ix < nEdgePoints; ++ix )
    //        {
    //            const bool edgex = ix == 0 || ix == ( nEdgePoints - 1 );
    //            
    //            if( !edgex && !edgey && !edgez )
    //                continue;
    //            
    //            const float x = -0.5f + ix * step;

    //            SYS_ASSERT( pointCounter < nPoints );

    //            Vector3 pos( x, y, z );

    //            localPointsBox[pointCounter] = pos;
    //            localPointsSphere[pointCounter] = normalize( pos );

    //            ++pointCounter;
    //        }
    //    }
    //}
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


    //int mainBegin = ch->mainBody.particleBegin;
    //int shapeBegin = body.particleBegin;

    //const Vector3* mainPos0 = ch->particles.pos0 + mainBegin;
    //const Vector3* shapePos0 = ch->particles.pos0 + shapeBegin;

    //Constraint* cs = ch->constraints.desc + body.constraintBegin;
    //initConstraint( cs + 0, mainPos0[0], shapePos0[0], mainBegin+0, shapeBegin );
    //initConstraint( cs + 1, mainPos0[1], shapePos0[0], mainBegin+1, shapeBegin );
    //initConstraint( cs + 2, mainPos0[2], shapePos0[0], mainBegin+2, shapeBegin );

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
    
    bxPolyShape_deallocateShape( &shape );
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

    static void softBodyUpdatePose( Vector3* comPos, Quat* comRot, ParticleData* cp, int pointBegin, int pointEnd, const Vector3* restPositions, float shapeStiffness, float shapeScale )
    {
        const int pointCount = pointEnd - pointBegin;
        SYS_ASSERT( pointCount > 0 );

        Matrix3 R;
        Vector3 com;
        bxPhx::pbd_softBodyUpdatePose( &R, &com, cp->pos1 + pointBegin, restPositions, cp->mass + pointBegin, pointCount );
        comPos[0] = com;
        comRot[0] = normalize( Quat( R ) );

        //const Matrix3 R1 = appendScale( R, Vector3( shapeScale ) );
        if( ::abs( shapeScale - 1.f ) > FLT_EPSILON )
        {
            R = appendScale( R, Vector3( shapeScale ) );
        }
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
    Character::Body* body;
    
    void operator() ( Vector3& vel, int ipoint, float deltaTime )
    {
        if( ipoint == body->particleBegin )
        {
            vel += inputVec;
        }

        vel += jumpVec;
    }
};

struct PBD_VelPlug_ShapeBody
{
    Vector3 inputVec;
    Vector3 jumpVec;

    void operator() ( Vector3& vel, int ipoint, float deltaTime )
    {
        vel += inputVec;
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
        //massInv0 = 0.f;
    }
};
//
//void simulateMainBodyBegin( Character1* ch, const Vector3& extForce, float deltaTime )
//{
//    ParticleData* cp = &ch->particles;
//    const Character1::Body& body = ch->mainBody;
//
//    //const floatInVec dtv( deltaTime );
//    //const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( 0.1f ), dtv );
//    
//    const Vector3 gravity = -ch->upVector * 9.1f;
//    const Vector3 jumpVector = ch->upVector * ch->_jumpAcc;
//
//    PBD_VelPlug_MainBody velPlug;
//    velPlug.body = &ch->mainBody;
//    velPlug.inputVec = extForce;
//    velPlug.jumpVec = jumpVector;
//
//    PBD_Simulate::predictPosition( cp, body.particleBegin, body.particleEnd, gravity, 0.1f, deltaTime, velPlug );
//    
//    const Constraint* cs = ch->constraints.desc + body.constraintBegin;
//    for( int iiter = 0; iiter < 4; ++iiter )
//        PBD_Simulate::projectDistanceConstraint( cp, cs, body.constaintCount(), 1.f, PBD_ConstraintParamEval_Null() );
//
//    //const int nPoint = body.count();
//    //for( int ipoint = body.begin; ipoint < body.end; ++ipoint )
//    //{
//    //    Vector3 pos = cp->pos0[ipoint];
//    //    Vector3 vel = cp->vel[ipoint];
//
//    //    vel += (gravity)* dtv;
//    //    vel *= dampingCoeff;
//
//    //    if( ipoint == body.begin )
//    //    {
//    //        vel += extForce;
//    //    }
//    //    vel += jumpVector;
//    //    pos += vel * dtv;
//
//    //    cp->pos1[ipoint] = pos;
//    //    cp->vel[ipoint] = vel;
//    //}
//
//    //projectDistanceConstraints( cp, ch->mainBodyConstraints, Character1::eMAIN_BODY_CONSTRAINT_COUNT, 1.f, 4 );
//}

void simulateShapeBodyBegin( Character* ch, const Vector3& extForce, float deltaTime )
{
    ParticleData* cp = &ch->particles;
    const Character::Body& body = ch->shapeBody;

    const Vector3 gravity = -ch->upVector * 9.1f;
    const Vector3 jumpVector = ch->upVector * ch->_jumpAcc;

    PBD_VelPlug_ShapeBody velPlug;
    velPlug.inputVec = extForce;
    velPlug.jumpVec = jumpVector;
    PBD_Simulate::predictPosition( cp, body.particleBegin, body.particleEnd, gravity, 0.7f, deltaTime, velPlug );
    
    //const Constraint* cs = ch->constraints.desc + body.constaintCount();
    //for( int iiter = 0; iiter < 4; ++iiter )
    //    PBD_Simulate::projectDistanceConstraint( cp, cs, body.constaintCount(), 1.f, PBD_ConstraintParamEval_Wheel() );
}

void simulateShapeUpdatePose( Character* ch, float shapeScale, float shapeStiffness )
{
    Character::Body& body = ch->shapeBody;

    Quat comRot;
    Vector3 comPos;
    PBD_Simulate::softBodyUpdatePose( &comPos, &comRot, &ch->particles, body.particleBegin, body.particleEnd, ch->restPos.current, shapeStiffness, shapeScale );

    body.com.pos = comPos;
    body.com.rot = comRot;
}

void simulateFinalize( Character* ch, float staticFriction, float dynamicFriction, float deltaTime )
{
    const float deltaTimeInv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
    
    PBD_Simulate::applyFriction( &ch->particles, ch->contacts, staticFriction, dynamicFriction );
    PBD_Simulate::updateVelocity( &ch->particles, 0, ch->particles.size, deltaTimeInv );
}

void computeCharacterPose( Character* ch )
{
    //Vector3 com( 0.f );
    //float massSum = 0.f;

    //ParticleData* cp = &ch->particles;
    //Character1::Body body = ch->mainBody;

    //for( int ipoint = body.particleBegin; ipoint < body.particleEnd; ++ipoint )
    //{
    //    com += cp->pos0[ipoint] * cp->mass[ipoint];
    //    massSum += cp->mass[ipoint];
    //}
    //com /= massSum;

    //Vector3 dir = normalize( cp->pos0[body.particleBegin] - com );
    //Vector3 side = normalize( cross( ch->upVector, dir ) );
    
    //ch->frontVector = dir;
    //ch->sideVector = side;
    //ch->feetCenterPos = com;

    //body.com.pos = com;
    //body.com.rot = Quat( Matrix3( dir, ch->upVector, side ) );


    //const int leftFootParticleIndex = ch->wheelBody.particleBegin + 2;
    //const int rightFootParticleIndex = ch->wheelBody.particleBegin + 6;
    //const float sideLen = 0.35f;

    //const Vector3& leftVelocity = ch->particles.vel[leftFootParticleIndex];
    //const Vector3& rightVelocity = ch->particles.vel[rightFootParticleIndex];

    //const float leftSpeed = length( leftVelocity ).getAsFloat();
    //const float rightSpeed = length( rightVelocity ).getAsFloat();

    //const Vector3 footOffsetX = side * sideLen;
    //
    //Vector3 leftWalkPos = ch->particles.pos0[leftFootParticleIndex] - footOffsetX;
    //Vector3 rightWalkPos = ch->particles.pos0[rightFootParticleIndex] + footOffsetX;

    //const Vector4 footPlane = makePlane( ch->upVector, com );
    //Vector3 leftRestPos = projectPointOnPlane( leftWalkPos, footPlane );
    //Vector3 rightRestPos = projectPointOnPlane( rightWalkPos, footPlane );

    //

    //const float leftAlpha = smoothstep( 0.f, 2.f, leftSpeed );
    //const float rightAlpha = smoothstep( 0.f, 2.f, rightSpeed );

    //ch->leftFootPos = lerp( leftAlpha, leftRestPos, leftWalkPos );
    //ch->rightFootPos = lerp( rightAlpha, rightRestPos, rightWalkPos );
}

}}///




