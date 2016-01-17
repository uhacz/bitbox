#include "game.h"
#include "physics_pbd.h"

#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/random.h>
#include <util/math.h>
#include <util/hashmap.h>
#include <util/array.h>
#include <util/array_util.h>
#include <util/common.h>
#include <util/hash.h>
#include <util/string_util.h>
#include <util/id_array.h>

#include <engine/profiler.h>

#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>
#include <phx/phx.h>
#include <smmintrin.h>
#include "physics.h"
#include <system/input.h>
#include <util/signal_filter.h>
#include "scene.h"

//namespace bxGame
//{
//    struct FlockParticles
//    {
//        void* memoryHandle;
//        
//        Vector3* pos0;
//        Vector3* vel;
//        f32* massInv;
//
//        i32 size;
//        i32 capacity;
//    };
//    void _FlockParticles_allocate( FlockParticles* fp, int capacity )
//    {
//        if ( fp->capacity >= capacity )
//            return;
//
//        int memSize = 0;
//        memSize += capacity * sizeof( *fp->pos0 );
//        memSize += capacity * sizeof( *fp->vel );
//        memSize += capacity * sizeof( *fp->massInv );
//
//        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
//        memset( mem, 0x00, memSize );
//
//        bxBufferChunker chunker( mem, memSize );
//
//        FlockParticles newFp;
//        newFp.memoryHandle = mem;
//        newFp.size = fp->size;
//        newFp.capacity = capacity;
//        newFp.pos0 = chunker.add< Vector3 >( capacity );
//        newFp.vel = chunker.add< Vector3 >( capacity );
//        newFp.massInv = chunker.add< f32 >( capacity );
//        chunker.check();
//
//        if( fp->size )
//        {
//            BX_CONTAINER_COPY_DATA( &newFp, fp, pos0 );
//            BX_CONTAINER_COPY_DATA( &newFp, fp, vel );
//            BX_CONTAINER_COPY_DATA( &newFp, fp, massInv );
//        }
//
//        BX_FREE0( bxDefaultAllocator(), fp->memoryHandle );
//        fp[0] = newFp;
//    }
//    int _FlockParticles_add( FlockParticles* fp, const Vector3& pos )
//    {
//        if( fp->size + 1 >= fp->capacity )
//        {
//            _FlockParticles_allocate( fp, fp->capacity * 2 + 8 );
//        }
//
//        bxRandomGen rnd( u32( u64( fp ) ^ ((fp->size << fp->capacity) << 13) ) );
//
//        const int index = fp->size++;
//        fp->pos0[index] = pos;
//        fp->vel[index] = Vector3( 0.f );
//        fp->massInv[index] = 1.f / bxRand::randomFloat( rnd, 1.f, 0.5f );
//        return index;
//    }
//
//    ////
//    ////
//    union FlockHashmap_Item
//    {
//        u64 hash;
//        struct {
//            u32 index;
//            u32 next;
//        };
//    };
//    inline FlockHashmap_Item makeItem( u64 hash )
//    {
//        FlockHashmap_Item item = { hash };
//        return item;
//    }
//    inline bool isValid( FlockHashmap_Item item ) { return item.hash != UINT64_MAX; }
//    inline bool isSentinel( FlockHashmap_Item item ) { return item.next == UINT32_MAX; }
//
//    union FlockHashmap_Key
//    {
//        u64 hash;
//        struct {
//            i16 x;
//            i16 y;
//            i16 z;
//            i16 w;
//        };
//    };
//
//    struct FlockHashmap
//    {
//        hashmap_t map;
//        array_t< FlockHashmap_Item > items;
//    };
//
//    inline FlockHashmap_Key makeKey( int x, int y, int z )
//    {
//        FlockHashmap_Key key = { 0 };
//        SYS_ASSERT( x < (INT16_MAX) && x >( INT16_MIN ) );
//        SYS_ASSERT( y < (INT16_MAX) && y >( INT16_MIN ) );
//        SYS_ASSERT( z < (INT16_MAX) && z >( INT16_MIN ) );
//        key.x = x;
//        key.y = y;
//        key.z = z;
//        key.w = 1;
//        return key;
//    }
//    inline FlockHashmap_Key makeKey( const Vector3& pos, float cellSizeInv )
//    {
//        const Vector3 gridPos = pos * cellSizeInv;
//        const __m128i rounded = _mm_cvtps_epi32( _mm_round_ps( gridPos.get128(), _MM_FROUND_FLOOR ) );
//        const SSEScalar tmp( rounded );
//        return makeKey( tmp.ix, tmp.iy, tmp.iz );
//    }
//
//
//
//    void hashMapAdd( FlockHashmap* hmap, const Vector3& pos, int index, float cellSizeInv )
//    {
//        FlockHashmap_Key key = makeKey( pos, cellSizeInv );
//        hashmap_t::cell_t* cell = hashmap::lookup( hmap->map, key.hash );
//        if( !cell )
//        {
//            cell = hashmap::insert( hmap->map, key.hash );
//            FlockHashmap_Item begin;
//            begin.index = index;
//            begin.next = UINT32_MAX;
//            cell->value = begin.hash;
//        }
//        else
//        {
//            FlockHashmap_Item begin = { cell->value };
//            FlockHashmap_Item item = { 0 };
//            item.index = index;
//            item.next = begin.next;
//            begin.next = array::push_back( hmap->items, item );
//            cell->value = begin.hash;
//        }
//    }
//    
//    FlockHashmap_Item hashMapItemFirst( const FlockHashmap& hmap, FlockHashmap_Key key )
//    {
//        const hashmap_t::cell_t* cell = hashmap::lookup( hmap.map, key.hash );
//        return (cell) ? makeItem( cell->value ) : makeItem( UINT64_MAX );
//    }
//    FlockHashmap_Item hashMapItemNext( const FlockHashmap& hmap, const FlockHashmap_Item current )
//    {
//        return (current.next != UINT64_MAX) ? hmap.items[current.next] : makeItem( UINT64_MAX );
//    }
//
//    void flock_hashMapDebugDraw( FlockHashmap* hmap, float cellSize, u32 color )
//    {
//        const Vector3 cellExt( cellSize * 0.5f );
//        hashmap::iterator it( hmap->map );
//        while ( it.next() )
//        {
//            FlockHashmap_Key key = { it->key };
//
//            Vector3 center( key.x, key.y, key.z );
//            center *= cellSize;
//            center += cellExt;
//
//            bxGfxDebugDraw::addBox( Matrix4::translation( center ), cellExt, color, 1 );
//        }
//    }
//
//    struct FlockParams
//    {
//        f32 boidRadius;
//        f32 separation;
//        f32 alignment;
//        f32 cohesion;
//        f32 attraction;
//        f32 cellSize;
//
//        FlockParams()
//            : boidRadius( 1.f )
//            , separation( 1.0f )
//            , alignment( 0.2f )
//            , cohesion( 0.05f )
//            , attraction( 0.6f )
//            , cellSize( 2.f )
//        {}
//    };
//    void _FlockParams_show( FlockParams* params )
//    {
//        if ( ImGui::Begin( "Flock" ) )
//        {
//            ImGui::InputFloat( "boidRadius", &params->boidRadius );
//            ImGui::SliderFloat( "separation", &params->separation, 0.f, 1.f );
//            ImGui::SliderFloat( "alignment", &params->alignment, 0.f, 1.f );
//            ImGui::SliderFloat( "cohesion", &params->cohesion, 0.f, 1.f );
//            ImGui::SliderFloat( "attraction", &params->attraction, 0.f, 1.f );
//            ImGui::InputFloat( "cellSize", &params->cellSize );
//        }
//        ImGui::End();
//    }
//
//    struct Flock
//    {
//        FlockParticles particles;
//        FlockParams params;
//        FlockHashmap hmap;
//
//        f32 _dtAcc;
//    };
//
//    Flock* flock_new()
//    {
//        Flock* fl = BX_NEW( bxDefaultAllocator(), Flock );
//        memset( &fl->particles, 0x00, sizeof( FlockParticles ) );
//        fl->_dtAcc = 0.f;
//        return fl;
//    }
//
//    void flock_delete( Flock** flock )
//    {
//        if ( !flock[0] )
//            return;
//
//        //bxGfx::worldMeshRemoveAndRelease( &flock[0]->hMeshI );
//
//        BX_FREE0( bxDefaultAllocator(), flock[0]->particles.memoryHandle );
//        BX_DELETE0( bxDefaultAllocator(), flock[0] );
//    }
//
//    void flock_init( Flock* flock, int nBoids, const Vector3& center, float radius )
//    {
//        _FlockParticles_allocate( &flock->particles, nBoids );
//
//        bxRandomGen rnd( TypeReinterpert( radius ).u );
//        for( int iboid = 0; iboid < nBoids; ++iboid )
//        {
//            const float dx = rnd.getf( -radius, radius );
//            const float dy = rnd.getf( -radius, radius );
//            const float dz = rnd.getf( -radius, radius );
//
//            const Vector3 dpos = Vector3( dx, dy, dz );
//            const Vector3 pos = center + dpos;
//
//            _FlockParticles_add( &flock->particles, pos );
//        }
//    }
//
//    void flock_loadResources( Flock* flock, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
//    {
//        //bxGfx_HMesh hmesh = bxGfx::meshCreate();
//        //flock->hInstanceBuffer = bxGfx::instanceBuffeCreate( flock->particles.size );
//
//        //bxGdiRenderSource* rsource = bxGfx::globalResources()->mesh.sphere;
//        //bxGfx::meshStreamsSet( hmesh, dev, rsource );
//
//        //bxGdiShaderFx_Instance* materialFx = bxGfxMaterialManager::findMaterial( "blue" );
//        //bxGfx::meshShaderSet( hmesh, dev, resourceManager, materialFx );
//        
//        //flock->hMeshI = bxGfx::worldMeshAdd( gfxWorld, hmesh, flock->particles.size );
//    }
//
//
//    inline bool isInNeighbourhood( const Vector3& pos, const Vector3& posB, float radius )
//    {
//        const Vector3 vec = posB - pos;
//        const float dd = lengthSqr( vec ).getAsFloat();
//
//        return ( dd <= radius * radius );
//    }
//    //Quat quatAim( const Vector3& v )
//    //{ 
//    //    const Vector3 vn = normalize( v );
//
//    //    Quat qr;
//    //    qr.setX( v.getY() );
//    //    qr.setY( -v.getX() );
//    //    qr.setZ( zeroVec );
//    //    qr.setW( oneVec - v.getZ() );
//    //    return normalize( qr );
//    //}
//    //Quat quatAim( const Vector3& p1, const Vector3& p2 )
//    //{
//    //    const Vector3 v = p2 - p1;
//    //    return quatAim( v );
//    //}
//
//    void flock_simulate( Flock* flock, float deltaTime )
//    {
//        const Vector3 com( 0.f );
//
//        const float boidRadius = flock->params.boidRadius;
//        const float separation = flock->params.separation;
//        const float alignment  = flock->params.alignment;
//        const float cohesion   = flock->params.cohesion;
//        const float attraction = flock->params.attraction;
//        const float cellSize   = flock->params.cellSize;
//        const float cellSizeInv = 1.f / cellSize;
//
//        const float cellSizeSqr = cellSize * cellSize;
//        const float boidRadiusSqr = boidRadius * boidRadius;
//
//        FlockParticles* fp = &flock->particles;
//
//        const int nBoids = flock->particles.size;
//
//        for( int iboid = 0; iboid < nBoids; ++iboid )
//        {
//            Vector3 separationVec( 0.f );
//            Vector3 cohesionVec( 0.f );
//            Vector3 alignmentVec( 0.f );
//
//            Vector3 pos = fp->pos0[iboid];
//            Vector3 vel = fp->vel[iboid];
//
//            const FlockHashmap_Key mainKey = makeKey( pos, cellSizeInv);
//            hashmap_t::cell_t* mainCell = hashmap::lookup( flock->hmap.map, mainKey.hash );
//            int xBegin = -1, xEnd = 1;
//            int yBegin = -1, yEnd = 1;
//            int zBegin = -1, zEnd = 1;
//
//
//            int neighboursAlignment = 0;
//            int neighboursCohesion = 0;
//            for( int iz = zBegin; iz <= zEnd; ++iz )
//            {
//                for( int iy = yBegin; iy <= yEnd; ++iy )
//                {
//                    for( int ix = xBegin; ix <= xEnd; ++ix )
//                    {
//                        FlockHashmap_Key key = makeKey( mainKey.x + ix, mainKey.y + iy, mainKey.z + iz );
//                        FlockHashmap_Item item = hashMapItemFirst( flock->hmap, key );
//                        
//                        while( !isSentinel( item ) )
//                        {
//                            const int iboid1 = item.index;
//                            if( iboid1 != iboid )
//                            {
//                                const Vector3& posB = fp->pos0[iboid1];
//                                const Vector3& velB = fp->vel[iboid1];
//
//                                {
//                                    const Vector3 vec = posB - pos;
//                                    const floatInVec vecLen = length( vec );
//                                    const floatInVec displ = maxf4( floatInVec( boidRadius ) - vecLen, zeroVec );
//                                    Vector3 dpos = vec * floatInVec( recipf4_newtonrapson( vecLen.get128() ) ) * displ;
//                                    separationVec -= dpos;
//                                }
//                                
//                                if( isInNeighbourhood( pos, posB, cellSize ) )
//                                {
//                                    alignmentVec += velB;
//                                    ++neighboursAlignment;
//
//                                    cohesionVec += posB;
//                                    ++neighboursCohesion;
//                                }
//                            }
//                            item = hashMapItemNext( flock->hmap, item );
//                        }
//                    }
//                }
//            }
//
//            {
//                separationVec = normalizeSafe( separationVec );
//                if( neighboursAlignment > 0 )
//                {
//                    alignmentVec = normalizeSafe( ( alignmentVec / (f32)neighboursAlignment ) - vel );
//                }
//                if( neighboursCohesion )
//                {
//                    cohesionVec = normalizeSafe( ( cohesionVec / (f32)neighboursCohesion ) - pos );
//                }
//            }
//
//
//            Vector3 steering( 0.f );
//            steering += separationVec * separation;
//            steering += alignmentVec * alignment;
//            steering += cohesionVec * cohesion;
//            steering += normalizeSafe( com - pos ) * attraction;
//
//            vel += ( steering * fp->massInv[iboid] ) * deltaTime;
//            pos += vel * deltaTime;
//
//            fp->pos0[iboid] = pos;
//            fp->vel[iboid] = vel;
//        }
//    }
//
//    void flock_tick( Flock* flock, float deltaTime )
//    {
//        rmt_ScopedCPUSample( Flock_tick );
//
//        const float deltaTimeFixed = 1.f / 60.f;
//        const float cellSizeInv = 1.f / flock->params.cellSize;
//
//        //deltaTime = deltaTimeFixed;
//        flock->_dtAcc += deltaTime;
//
//        
//
//        while( flock->_dtAcc >= deltaTimeFixed )
//        {
//            {
//                rmt_ScopedCPUSample( Flock_recreateGrid );
//                hashmap::clear( flock->hmap.map );
//                for( int iboid = 0; iboid < flock->particles.size; ++iboid )
//                {
//                    hashMapAdd( &flock->hmap, flock->particles.pos0[iboid], iboid, cellSizeInv );
//                }
//            }
//            
//            {
//                rmt_ScopedCPUSample( Flock_simulate );
//                flock_simulate( flock, deltaTimeFixed );
//            }
//            flock->_dtAcc -= deltaTimeFixed;
//        }
//
//        //{
//        //    rmt_ScopedCPUSample( Flock_debugDraw );
//        //    flock_hashMapDebugDraw( &flock->hmap, flock->params.cellSize, 0x222222FF );
//        //}
//
//        FlockParticles* fp = &flock->particles;
//
//        const int nBoids = flock->particles.size;
//        for( int iboid = 0; iboid < nBoids; ++iboid )
//        {
//            const Vector3& pos = fp->pos0[iboid];
//            const Vector3& vel = fp->vel[iboid];
//
//            //Vector3 posGrid( _mm_round_ps( pos.get128(), _MM_FROUND_NINT ) );
//
//            //bxGfxDebugDraw::addSphere( Vector4( pos, 0.1f ), 0xFFFF00FF, true );
//            const Matrix3 rotation = Matrix3::identity(); // createBasis( normalizeSafe( vel ) );
//            const Matrix4 pose = appendScale( Matrix4( rotation, pos ), Vector3( 0.1f ) );
//            //bxGfxDebugDraw::addLine( pos, pos + vel, 0x0000FFFF, true );
//            
//            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol0(), 0x0F00FFFF, true );
//            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol1(), 0xF000FFFF, true );
//            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol2(), 0xFF00FFFF, true );
//        }
//
//        _FlockParams_show( &flock->params );
//    }
//
//
//
//}///


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace bx
{
    struct CharacterControllerImpl : public CharacterController
    {
		struct Input
		{
			f32 analogX = 0.f;
			f32 analogY = 0.f;
			f32 jump = 0.f;
			f32 crouch = 0.f;
			f32 L2 = 0.f;
			f32 R2 = 0.f;
		};

        struct DynamicState
        {
            Vector3 _pos0 = Vector3(0.f);
            Vector3 _pos1 = Vector3( 0.f );
            Vector3 _vel = Vector3( 0.f );
            Quat _rotation = Quat::identity();

            void setPose( const Matrix4& worldPose )
            {
                _pos0 = worldPose.getTranslation();
                _pos1 = worldPose.getTranslation();
                _vel = Vector3( 0.f );
                _rotation = Quat( worldPose.getUpper3x3() );
            }
        };

        Vector3 _upDir = Vector3( 0.f, 1.f, 0.f );
        DynamicState _dstate0;
        PhxGeometry _geometry0 = PhxGeometry( 0.5f );
		PhxActor* _actor = nullptr;

		Input _input;
		f32 _jumpAcc = 0.f;

        f32 timeAcc_ = 0.f;

        void _Init( GameScene* scene, const Matrix4& worldPose )
        {
            _dstate0.setPose( worldPose );
			PhxContext* phxCtx = phxContextGet( scene->phxScene );
			const Matrix4 pose( _dstate0._rotation, _dstate0._pos0 );
			phxActorCreateDynamic( &_actor, phxCtx, pose, _geometry0, -10.f );
			phxSceneActorAdd( scene->phxScene, &_actor, 1 );
			phxActorQueryEnable( _actor, false );
        }

        void _Deinit( GameScene* scene )
        {
			phxActorDestroy( &_actor );
        }

        void _Teleport( const Matrix4& worldPose )
        {
            _dstate0.setPose( worldPose );
        }

		void _CollectInputData( const bxInput& sysInput, float deltaTime )
		{
			float analogX = 0.f;
			float analogY = 0.f;
			float jump = 0.f;
			float crouch = 0.f;
			float L2 = 0.f;
			float R2 = 0.f;

			const bxInput_Pad& pad = bxInput_getPad( sysInput );
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
				const bxInput_Keyboard* kbd = &sysInput.kbd;

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
			_input.analogX = signalFilter_lowPass( analogX, _input.analogX, RC, deltaTime );
			_input.analogY = signalFilter_lowPass( analogY, _input.analogY, RC, deltaTime );
			_input.jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
			_input.crouch = signalFilter_lowPass( crouch, _input.crouch, RC, deltaTime );
			_input.L2 = L2;
			_input.R2 = R2;
		}

        //////////////////////////////////////////////////////////////////////////
        virtual void tick( GameScene* scene, const bxInput& input, float deltaTime )
        {
			_CollectInputData( input, deltaTime );

			bx::GfxCamera* camera = scene->cameraManager->stack()->top();
			const Matrix4 cameraWorld = bx::gfxCameraWorldMatrixGet( camera );
			Vector3 inputVector( 0.f );
			{
				Vector3 xInputForce = Vector3::xAxis() * _input.analogX;
				Vector3 yInputForce = -Vector3::zAxis() * _input.analogY;

				const float maxInputForce = 1.25f;
				inputVector = (xInputForce + yInputForce);// *maxInputForce;

				const floatInVec externalForcesValue = minf4( length( inputVector ), floatInVec( maxInputForce ) );
				inputVector = projectVectorOnPlane( cameraWorld.getUpper3x3() * inputVector, Vector4( _upDir, oneVec ) );
				inputVector = normalizeSafe( inputVector ) * externalForcesValue;
				_jumpAcc += _input.jump * 4;

				//bxGfxDebugDraw::addLine( character->bottomBody.com.pos, character->bottomBody.com.pos + externalForces + character->upVector*character->_jumpAcc, 0xFFFFFFFF, true );
			}

			timeAcc_ += deltaTime;
			timeAcc_ = minOfPair( 0.1f, timeAcc_ );

			float deltaTimeFix = 1.f / 60.f;
			const float deltaTimeInv = 60.f; // ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
			const float dampingCoeff = 0.2f;
			const float damping = pow( 1.f - dampingCoeff, deltaTime );
			
			const Vector3 gravity = -_upDir * 9.f;
			const Vector3 jumpVector = _upDir * _jumpAcc;
			
            while( timeAcc_ >= deltaTimeFix )
            {
                {
                    DynamicState* ds = &_dstate0;

                    Vector3 pos0 = ds->_pos0;
                    Vector3 vel = ds->_vel;

					vel += ( inputVector + jumpVector );
					vel += ( gravity ) * deltaTimeFix;
                    vel *= damping;

                    Vector3 pos1 = pos0 + vel * deltaTimeFix;

                    ds->_pos1 = pos1;
                    ds->_vel = vel;
                }

                {
                    const PhxGeometry& sweepGeom = _geometry0;

                    DynamicState* ds = &_dstate0;
                    const Vector3& pos0 = ds->_pos0;
                    Vector3 pos1 = ds->_pos1;

                    const Vector3 rd = pos1 - pos0;
                    const float displ = length( rd ).getAsFloat();


                    if( displ > FLT_EPSILON )
                    {
                        PhxQueryHit hit;
                        const TransformTQ pose( ds->_rotation, ds->_pos1 );
						const Vector3 rdn = normalize( rd );
                        if( phxSweep( &hit, scene->phxScene, sweepGeom, pose, -_upDir, displ ) )
                        {
							//if( hit.distance < displ )
							{
								//pos1 = pos0 + rdn * hit.distance;// ;
								pos1 = hit.position + hit.normal * sweepGeom.sphere.radius;
							}
                        }
                    }
					ds->_pos1 = pos1;
					//const Matrix4 actorPose = phxActorPoseGet( _actor );
                    //ds->_pos1 = actorPose.getTranslation();
                }

                {
                    DynamicState* ds = &_dstate0;
                    ds->_vel = ( ds->_pos1 - ds->_pos0 ) * deltaTimeInv;
                    ds->_pos0 = ds->_pos1;
                }

				{
					phxActorTargetPoseSet( _actor, Matrix4( _dstate0._rotation, _dstate0._pos1 ), scene->phxScene );
				}

                timeAcc_ -= deltaTimeFix;
				_jumpAcc = 0.f;
            }

            bxGfxDebugDraw::addSphere( Vector4( _dstate0._pos0, _geometry0.sphere.radius ), 0xFFFF00FF, 1 );
        }

        virtual Matrix4 worldPose() const
        {
            return Matrix4( _dstate0._rotation, _dstate0._pos0 );
        }

        virtual Vector3 upDirection() const
        {
            return _upDir;
        }


    };

    void CharacterController::create( CharacterController** cc, GameScene* scene, const Matrix4& worldPose )
    {
        CharacterControllerImpl* c = BX_NEW( bxDefaultAllocator(), CharacterControllerImpl );
        c->_Init( scene, worldPose );

        cc[0] = c;
    }
    void CharacterController::destroy( CharacterController** cc, GameScene* scene )
    {
        if( !cc[0] )
            return;

        CharacterControllerImpl* impl = (CharacterControllerImpl*)cc[0];
        impl->_Deinit( scene );

        BX_DELETE0( bxDefaultAllocator(), cc[0] );
    }
}///

