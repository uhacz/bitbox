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

#include "scene.h"
#include "phx/phx.h"
#include "tools/remotery/Remotery.h"

namespace 
{
    using namespace bx;
    struct PBD_Simulate
    {
        template< class TVelPlug >
        static void predictPosition( ParticleData* cp, int pointBegin, int pointEnd, const Vector3& gravityVec, float damping, float deltaTime, TVelPlug& velPlug )
        {
            const floatInVec dtv( deltaTime );
            const floatInVec dampingCoeff = fastPow_01Approx( oneVec - floatInVec( damping ), dtv );

            for ( int ipoint = pointBegin; ipoint < pointEnd; ++ipoint )
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

        static void resolveContacts( ParticleData* cp, bx::PhxContacts* contacts )
        {
            const int nContacts = bx::phxContactsSize( contacts );

            Vector3 normal( 0.f );
            float depth = 0.f;
            u16 index0 = 0xFFFF;
            u16 index1 = 0xFFFF;

            for ( int icontact = 0; icontact < nContacts; ++icontact )
            {
                bx::phxContactsGet( contacts, &normal, &depth, &index0, &index1, icontact );
                cp->pos1[index0] += normal * depth;
            }
        }

        static void applyFriction( ParticleData* cp, bx::PhxContacts* contacts, float staticFriction, float dynamicFriction )
        {
            const int nContacts = bx::phxContactsSize( contacts );

            Vector3 normal( 0.f );
            float depth = 0.f;
            u16 index0 = 0xFFFF;
            u16 index1 = 0xFFFF;

            for ( int icontact = 0; icontact < nContacts; ++icontact )
            {
                bx::phxContactsGet( contacts, &normal, &depth, &index0, &index1, icontact );

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
            if ( ::abs( shapeScale - 1.f ) > FLT_EPSILON )
            {
                R = appendScale( R, Vector3( shapeScale ) );
            }
            for ( int ipoint = pointBegin, irestpos = 0; ipoint < pointEnd; ++ipoint, ++irestpos )
            {
                SYS_ASSERT( irestpos < pointCount );
                Vector3 dpos( 0.f );
                bxPhx::pbd_solveShapeMatchingConstraint( &dpos, R, com, restPositions[irestpos], cp->pos1[ipoint], shapeStiffness );
                cp->pos1[ipoint] += dpos;
            }
        }

        static void updateVelocity( ParticleData* cp, int pointBegin, int pointEnd, float deltaTimeInv )
        {
            for ( int ipoint = pointBegin; ipoint < pointEnd; ++ipoint )
            {
                cp->vel[ipoint] = (cp->pos1[ipoint] - cp->pos0[ipoint]) * deltaTimeInv;
                cp->pos0[ipoint] = cp->pos1[ipoint];
            }
        }

        template< class TParamEval >
        static void projectDistanceConstraint( ParticleData* cp, const Constraint* constraints, int nConstraints, float stiffness, TParamEval paramEval )
        {
            for ( int iconstraint = 0; iconstraint < nConstraints; ++iconstraint )
            {
                const Constraint& c = constraints[iconstraint];

                const Vector3& p0 = cp->pos1[c.i0];
                const Vector3& p1 = cp->pos1[c.i1];

                float massInv0 = cp->massInv[c.i0];
                float massInv1 = cp->massInv[c.i1];
                float len = c.d;

                paramEval( massInv0, massInv1, len, c, iconstraint );

                Vector3 dpos0, dpos1;
                bxPhx::pbd_solveDistanceConstraint( &dpos0, &dpos1, p0, p1, massInv0, massInv1, c.d, stiffness, stiffness );

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
            if ( ipoint == body->particleBegin )
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
}


namespace bx
{
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

namespace bx{ namespace CharacterInternal {

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
    float analogX = 0.f;
    float analogY = 0.f;
    float jump = 0.f;
    float crouch = 0.f;
    float L2 = 0.f;
    float R2 = 0.f;

    const bxInput_Pad& pad = bxInput_getPad( input );
    const bxInput_PadState* padState = pad.currentState();
    if( padState->connected )
    {
        analogX = padState->analog.left_X;
        analogY = padState->analog.left_Y;

        crouch = (f32)bxInput_isPadButtonPressedOnce( pad, bxInput_PadState::eCIRCLE );
        jump = (f32)bxInput_isPadButtonPressedOnce( pad, bxInput_PadState::eCROSS );

        L2 = padState->analog.L2;
        R2 = padState->analog.R2;
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

        analogX = -(float)inLeft + (float)inRight;
        analogY = -(float)inBack + (float)inFwd;

        crouch = (f32)inCrouch;
        jump = (f32)inJump;

        L2 = (float)inL2;
        R2 = (float)inR2;
        
        //bxLogInfo( "x: %f, y: %f", charInput->analogX, charInput->jump );
    }

    const float RC = 0.01f;
    charInput->analogX = signalFilter_lowPass( analogX, charInput->analogX, RC, deltaTime );
    charInput->analogY = signalFilter_lowPass( analogY, charInput->analogY, RC, deltaTime );
    charInput->jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
    charInput->crouch = signalFilter_lowPass( crouch, charInput->crouch, RC, deltaTime );
    charInput->L2 = L2;
    charInput->R2 = R2;
}


static inline void initConstraint( Constraint* c, const Vector3& p0, const Vector3& p1, int i0, int i1 )
{
    c->i0 = (i16)i0;
    c->i1 = (i16)i1;
    c->d = length( p0 - p1 ).getAsFloat();
}

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

void initShapeMesh( Character* ch, bxGdiDeviceBackend* dev, bx::GfxScene* gfxScene )
{
    const ParticleData& p = ch->particles;
    const Character::Body& body = ch->shapeBody;
    const int nPoints = body.particleCount();

    bxGdiVertexStreamDesc sdesc;
    sdesc.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 3 );
    sdesc.addBlock( bxGdi::eSLOT_NORMAL, bxGdi::eTYPE_FLOAT, 3, 1 );

    bxGdiVertexBuffer vbuffer = dev->createVertexBuffer( sdesc, nPoints );
    bxGdiIndexBuffer ibuffer = dev->createIndexBuffer( bxGdi::eTYPE_USHORT, ch->shapeMeshData.nIndices, ch->shapeMeshData.indices );

    bxGdiRenderSource* rsource = bxGdi::renderSource_new( 1 );
    bxGdi::renderSource_setVertexBuffer( rsource, vbuffer, 0 );
    bxGdi::renderSource_setIndexBuffer( rsource, ibuffer );

    bx::GfxMeshInstance* meshInstance = nullptr;
    bx::gfxMeshInstanceCreate( &meshInstance, bx::gfxContextGet( gfxScene ) );

    bx::GfxMeshInstanceData miData;
    miData.renderSourceSet( rsource );
    miData.fxInstanceSet( bx::gfxMaterialFind( "white" ) );
    miData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );

    bx::gfxMeshInstanceDataSet( meshInstance, miData );
    bx::gfxMeshInstanceWorldMatrixSet( meshInstance, &Matrix4::identity(), 1 );

    bx::gfxSceneMeshInstanceAdd( gfxScene, meshInstance );

    ch->meshInstance = meshInstance;
}

void deinitShapeMesh( Character* ch, bxGdiDeviceBackend* dev )
{
    bxGdiRenderSource* rsource = bx::gfxMeshInstanceRenderSourceGet( ch->meshInstance );
    bx::gfxMeshInstanceDestroy( &ch->meshInstance );

    bxGdi::renderSource_releaseAndFree( dev, &rsource );

    //bxGfx::worldMeshRemoveAndRelease( &ch->shapeMeshI );
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
	    
    bxPolyShape_deallocateShape( &shape );
}


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

namespace bx
{
	Character* character_new()
	{
		Character* ch = BX_NEW( bxDefaultAllocator(), Character );
		return ch;
	}
	void character_delete( Character** character )
	{
		if( !character[0] )
			return;

		BX_DELETE0( bxDefaultAllocator(), character[0] );
	}
	
	void characterInit( Character* ch, bxGdiDeviceBackend* dev, GameScene* scene, const Matrix4& worldPose )
	{
		const int BODY_SHAPE_ITERATIONS = 6;

		ch->shapeBody.particleBegin = 0;
		ch->shapeBody.particleEnd = ch->shapeBody.particleBegin + computeShapeParticleCount( BODY_SHAPE_ITERATIONS );

		int nConstraints = 0;
		int nPoints = 0;
		nPoints += ch->shapeBody.particleCount();

		int nIndices = 0;
		nIndices += computeShapeTriangleCount( BODY_SHAPE_ITERATIONS ) * 3;

		MeshData::alloc( &ch->shapeMeshData, nIndices );


		ParticleData::alloc( &ch->particles, nPoints );
		ch->particles.size = nPoints;

		BodyRestPosition::alloc( &ch->restPos, ch->shapeBody.particleCount() );

		CharacterInternal::initShapeBody( ch, BODY_SHAPE_ITERATIONS, worldPose );
		CharacterInternal::initShapeMesh( ch, dev, scene->gfx_scene() );
		bx::phxContactsCreate( &ch->contacts, nPoints );
	}
	void characterDeinit( Character* character, bxGdiDeviceBackend* dev )
	{
		bx::phxContactsDestroy( &character->contacts );
		CharacterInternal::deinitShapeMesh( character, dev );
		BodyRestPosition::free( &character->restPos );
		ConstraintData::free( &character->constraints );
		ParticleData::free( &character->particles );
		MeshData::free( &character->shapeMeshData );
	}

	void characterTick( Character* character, bxGdiContextBackend* ctx, GameScene* scene, GfxCamera* camera, const bxInput& input, float deltaTime )
	{
		rmt_BeginCPUSample( CHARACTER );
		CharacterInternal::collectInputData( &character->input, input, deltaTime );

		const Matrix4 cameraWorld = bx::gfxCameraWorldMatrixGet( camera );
		Vector3 externalForces( 0.f );
		{
			Vector3 xInputForce = Vector3::xAxis() * character->input.analogX;
			Vector3 yInputForce = -Vector3::zAxis() * character->input.analogY;

			const float maxInputForce = 0.25f;
			externalForces = (xInputForce + yInputForce) * maxInputForce;

			const floatInVec externalForcesValue = minf4( length( externalForces ), floatInVec( maxInputForce ) );
			externalForces = projectVectorOnPlane( cameraWorld.getUpper3x3() * externalForces, Vector4( character->upVector, oneVec ) );
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
		if( jump )
		{
			externalForces *= dot( externalForces, character->upVector );
		}

		const bool doIteration = character->_dtAcc >= fixedDt;

		rmt_BeginCPUSample( iteration );

		while( character->_dtAcc >= fixedDt )
		{
			CharacterInternal::simulateShapeBodyBegin( character, externalForces, fixedDt );
			{
				const Vector4 bsphere( character->shapeBody.com.pos, 1.5f );
				bx::phxContactsClear( character->contacts );
				bx::phxContactsCollide( character->contacts, scene->phx_scene(), character->particles.pos1, character->particles.size, 0.05f, bsphere );
				bx::terrainCollide( character->contacts, scene->terrain, character->particles.pos1, character->particles.size, 0.05f, bsphere );
				PBD_Simulate::resolveContacts( &character->particles, character->contacts );
				CharacterInternal::simulateShapeUpdatePose( character, 1.f, shapeStiffness );
			}
			CharacterInternal::simulateFinalize( character, staticFriction, dynamicFriction, fixedDt );
			CharacterInternal::computeCharacterPose( character );

			character->_dtAcc -= fixedDt;
			character->_jumpAcc = 0.f;
		}
		rmt_EndCPUSample();
		if( doIteration )
		{
			bxGdiRenderSource* rsource = bx::gfxMeshInstanceRenderSourceGet( character->meshInstance );
			const bxGdiVertexBuffer& vbuffer = bxGdi::renderSourceVertexBuffer( rsource, 0 );
			MeshVertex* vertices = (MeshVertex*)ctx->mapVertices( vbuffer, 0, vbuffer.numElements, bxGdi::eMAP_WRITE );
			CharacterInternal::updateMesh( vertices, character->particles.pos0, character->particles.size, character->shapeMeshData.indices, character->shapeMeshData.nIndices );
			ctx->unmapVertices( vbuffer );


			const Vector3 minAABB = character->shapeBody.com.pos - Vector3( 0.5f );
			const Vector3 maxAABB = character->shapeBody.com.pos + Vector3( 0.5f );
			bx::GfxMeshInstanceData miData;
			miData.locaAABBSet( minAABB, maxAABB );
			bx::gfxMeshInstanceDataSet( character->meshInstance, miData );
		}

		//CharacterInternal::debugDraw( character );
		rmt_EndCPUSample();
	}

	Matrix4 characterPoseGet( const Character* ch )
	{
		//return Matrix4( Matrix3(  ch->sideVector, ch->upVector, ch->frontVector ), ch->feetCenterPos );
		return Matrix4( ch->shapeBody.com.rot, ch->shapeBody.com.pos );
	}
	Vector3 characterUpVectorGet( const Character* ch )
	{
		return ch->upVector;
	}
}////


