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

#include <anim/anim.h>
#include <util/time.h>

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
    struct FuzzyFunction
    {
        enum EType : u32
        {
            eLINEAR = 0,
            eSPLINE,
        };
        struct Point
        {
            f32 x, y;
        };
        EType type;
        bxTag64 name;
        Point points[4];
        f32 xmin;
        f32 xmax;

        float evaluateLinear( float t )
        {
            
        }
        float evaluateSpline( float t )
        {
        
        }
    };
    float fuzzyFunctionEvalueate( const FuzzyFunction& ff, float x )
    {
        float t = linearstep( ff.xmin, ff.xmax, x );
        switch( type )
        {
        case FuzzyFunction::eLINEAR: return ff.evaluateLinear( t );
        case FuzzyFunction::eSPLINE: return ff.evaluateSpline( t );
        default: return 0.f;
        }
    }
}///


namespace bx
{
	//////////////////////////////////////////////////////////////////////////
	namespace ECharacterAnimClip
	{
        enum {
            eIDLE = 0,
            eWALK,
            eRUN,
            eJUMP,
            eCOUNT,
        };
	};
	namespace ECharacterAnimBranch
	{
        enum {
            eROOT = 0,
            eLOCO,
            eMOTION,
            eCOUNT,
        };
	};
	namespace ECharacterAnimLeaf
	{
        enum {
            eIDLE = 0,
            eWALK,
            eRUN,
            eJUMP,
            eCOUNT,
        };
	};

	struct CharacterAnimBlendTree
	{
        bxAnim_BlendBranch _branch[ECharacterAnimBranch::eCOUNT];
        bxAnim_BlendLeaf _leaf[ECharacterAnimLeaf::eCOUNT];
	};

	struct CharacterAnimController
	{
        Vector3 _prevFootPosL = Vector3( 0.f );
        Vector3 _prevFootPosR = Vector3( 0.f );
		i16 _footIndexL = -1;
		i16 _footIndexR = -1;
		f32 _locomotion = 0.f;

        f32 _runBlendAlpha = 0.f;

		bxAnim_Context* _animCtx = nullptr;
		bxAnim_Skel* _skel = nullptr;
		bxAnim_Clip* _clip[ ECharacterAnimClip::eCOUNT ];

		u64 _timeUS = 0;
	};

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
        PhxGeometry _geometry0 = PhxGeometry( 0.25f, 0.5f );
        PhxCCTMoveResult _cctMoveStats;
        PhxCCT* _cct = nullptr;
        //PhxActor* _actor = nullptr;

		Input _input;
		f32 _jumpAcc = 0.f;
        f32 _prevJumpValue01 = 0.f;
        f32 _jumpValue01 = 0.f;
        f32 _jumpValue02 = 0.f;

        f32 timeAcc_ = 0.f;
		const f32 _deltaTimeInv = 60.f;
		const f32 _deltaTime = 1.f / 60.f;

        void _Init( GameScene* scene, const Matrix4& worldPose )
        {
            _dstate0.setPose( worldPose );
			//PhxContext* phxCtx = phxContextGet( scene->phxScene );
			//const Matrix4 pose( _dstate0._rotation, _dstate0._pos0 );
			//phxActorCreateDynamic( &_actor, phxCtx, pose, _geometry0, -10.f );
			//phxSceneActorAdd( scene->phxScene, &_actor, 1 );
			//phxActorQueryEnable( _actor, false );

            PhxCCTDesc cctDesc;
            cctDesc.capsuleHeight = _geometry0.capsule.halfHeight * 2.f;
            cctDesc.capsuleRadius = _geometry0.capsule.radius;
            cctDesc.position = worldPose.getTranslation();
            cctDesc.upDirection = _upDir;
            bool bres = phxCCTCreate( &_cct, scene->phxScene, cctDesc );
            SYS_ASSERT( bres );
        }

        void _Deinit( GameScene* scene )
        {
			//phxActorDestroy( &_actor );
            phxCCTDestroy( &_cct );
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
				const int inCrouch = bxInput_isKeyPressedOnce( kbd, 'Z' );
				const int inL2 = bxInput_isKeyPressed( kbd, bxInput::eKEY_LSHIFT );
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
        static void splitVelocityXZ_Y( Vector3* velXZ, Vector3* velY, const Vector3& vel, const Vector3& upDir )
        {
            velXZ[0] = projectVectorOnPlane( vel, makePlane( upDir, Vector3( 0.f ) ) );
            velY[0] = vel - velXZ[0];
        }

		static Vector3 applySpeedLimitXZ( const Vector3& vel, const Vector3& upDir, const floatInVec spdLimit )
		{
            Vector3 velXZ, velY;
            splitVelocityXZ_Y( &velXZ, &velY, vel, upDir );
			{
				const floatInVec spd = length( velXZ );
				const floatInVec velScaler = (spdLimit / spd );
				velXZ = select( velXZ, velXZ * velScaler, spd > spdLimit );
			}

			return velXZ + velY;
		}

        virtual void tick( GameScene* scene, const bxInput& input, float deltaTime )
        {
			_CollectInputData( input, deltaTime );

			bx::GfxCamera* camera = scene->cameraManager->stack()->top();
			const Matrix4 cameraWorld = bx::gfxCameraWorldMatrixGet( camera );
			Vector3 inputVector( 0.f );
			{
				Vector3 xInputForce = Vector3::xAxis() * _input.analogX;
				Vector3 yInputForce = -Vector3::zAxis() * _input.analogY;

				const float maxInputForce = 0.2f;
				inputVector = (xInputForce + yInputForce);// *maxInputForce;

				const floatInVec externalForcesValue = minf4( length( inputVector ), floatInVec( maxInputForce ) );
				inputVector = projectVectorOnPlane( cameraWorld.getUpper3x3() * inputVector, Vector4( _upDir, oneVec ) );
				inputVector = normalizeSafe( inputVector ) * externalForcesValue;
                inputVector += inputVector * 2.f * _input.L2;
				_jumpAcc += _input.jump * 4;

				//bxGfxDebugDraw::addLine( character->bottomBody.com.pos, character->bottomBody.com.pos + externalForces + character->upVector*character->_jumpAcc, 0xFFFFFFFF, true );
			}

			timeAcc_ += deltaTime;
			timeAcc_ = minOfPair( 0.1f, timeAcc_ );

			const float deltaTimeFix = _deltaTime;
			const float deltaTimeInv = _deltaTimeInv; // ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
            const float dampingCoeff = ( _input.L2 ) ? 0.15f : 0.995f;
			const float damping = pow( 1.f - dampingCoeff, deltaTimeFix );
			
			const Vector3 gravity = -_upDir * 9.f;
			const Vector3 jumpVector = _upDir * _jumpAcc;

            //const bool noInput = ::abs( _input.analogX ) < 0.1f && ::abs( _input.analogY ) < 0.1f;
			const floatInVec SPEED_LIMIT = (lengthSqr( _dstate0._vel ).getAsFloat() < 0.1f ) ? oneVec : floatInVec( scene->canim->_locomotion );

            while( timeAcc_ >= deltaTimeFix )
            {
                {
                    DynamicState* ds = &_dstate0;

                    Vector3 pos0 = ds->_pos0;
                    Vector3 vel = ds->_vel;

					vel += ( inputVector + jumpVector );
					vel += ( gravity ) * deltaTimeFix;
                    
                    Vector3 velXZ, velY;
                    splitVelocityXZ_Y( &velXZ, &velY, vel, _upDir );
                    velXZ *= damping;
                    vel = velXZ + velY;

                    //ds->_vel = applySpeedLimitXZ( ds->_vel, _upDir, SPEED_LIMIT );

                    Vector3 pos1 = pos0 + vel * deltaTimeFix;

                    ds->_pos1 = pos1;
                    ds->_vel = vel;
                }

                {
                    DynamicState* ds = &_dstate0;
                    const Vector3& pos0 = ds->_pos0;
                    Vector3 pos1 = ds->_pos1;

					//Vector4 collisionNrmDepth( 0.f );
                    float distanceFromGround = 0.f;
					{
                        const float MAX_SWEEP_DISTANCE = 1.5f;
                        const Vector3 ro = phxCCTCenterPositionGet( _cct );
						const Vector3 rd = -_upDir;

						PhxQueryHit hit;
						const TransformTQ pose( ro );
						if( phxSweep( &hit, scene->phxScene, _geometry0, pose, -_upDir, MAX_SWEEP_DISTANCE ) )
						{
                            //bxGfxDebugDraw::addSphere( Vector4( hit.position, 0.1f ), 0xFFFF00FF, 1 );
							//bxGfxDebugDraw::addLine( hit.position, hit.position + hit.normal, 0x0000FFFF, 1 );
							//if( hit.
                            //collisionNrmDepth = Vector4( hit.normal, deltaTimeFix );
						}
                        distanceFromGround = linearstep( 0.1f, MAX_SWEEP_DISTANCE, hit.distance );
					}
                    _jumpValue01 = signalFilter_lowPass( distanceFromGround, _jumpValue01, 0.1f, deltaTimeFix );
                    if( _jumpValue01 > 0.01f )
                    {
                        _jumpValue02 += ::abs( _jumpValue01 - _prevJumpValue01 );
                    }
                    else
                    {
                        _jumpValue02 = 0.f;
                    }
                    _prevJumpValue01 = _jumpValue01;
                    //if( lengthSqr( collisionNrmDepth ).getAsFloat() > FLT_EPSILON )
					//{
					//	Vector3 dpos( 0.f );
					//	bxPhx::pbd_computeFriction( &dpos, pos0, pos1, collisionNrmDepth.getXYZ(), collisionNrmDepth.getW().getAsFloat(), 0.1f, 0.1f );
					//	pos1 += dpos;
					//}

                    Vector3 rd = pos1 - pos0;
                    rd = applySpeedLimitXZ( rd, _upDir, SPEED_LIMIT );
                    //const float displ = length( rd ).getAsFloat();
                    phxCCTMove( &_cctMoveStats, _cct, rd, deltaTimeFix );
					
                    pos1 = pos0 + _cctMoveStats.dpos;
                    ds->_pos1 = pos1;
                    //Vector3 v = ( pos1 - pos0 ) * deltaTimeInv;
                    //v = applySpeedLimitXZ( v, _upDir, SPEED_LIMIT );
                    //ds->_pos1 = pos0 + v * deltaTimeFix;
				}
				
				{
                    DynamicState* ds = &_dstate0;
                    ds->_vel = ( ds->_pos1 - ds->_pos0 ) * deltaTimeInv;
                    ds->_pos0 = ds->_pos1;

					//ds->_vel = applySpeedLimitXZ( ds->_vel, _upDir, SPEED_LIMIT );
                }

				{
					const Vector4 plane = makePlane( _upDir, _dstate0._pos0 );
					Vector3 velXZ = projectVectorOnPlane( _dstate0._vel, plane );
					if( lengthSqr( velXZ ).getAsFloat() > 0.1f )
					{
						Vector3 dir = fastRotate( _dstate0._rotation, Vector3::zAxis() );
						const Quat drot = Quat::rotation( dir, normalizeSafe( velXZ ) );

						_dstate0._rotation = slerp( deltaTime*2.f, _dstate0._rotation, _dstate0._rotation * drot );
						_dstate0._rotation = normalize( _dstate0._rotation );
					}
				}
				
				timeAcc_ -= deltaTimeFix;
				_jumpAcc = 0.f;
            }

			bxGfxDebugDraw::addAxes( Matrix4( _dstate0._rotation, _dstate0._pos0 ) );
			//bxGfxDebugDraw::addBox( Matrix4( _dstate0._rotation, _dstate0._pos0 ), Vector3( _geometry0.capsule.radius ), 0xFFFF00FF, 1 );
            //bxGfxDebugDraw::addSphere( Vector4( _dstate0._pos0, _geometry0.sphere.radius ), 0xFFFF00FF, 1 );

            const Vector3 velocityXZ = projectVectorOnPlane( _dstate0._vel, makePlane( _upDir, Vector3( 0.f ) ) );
            if( ImGui::Begin( "CharacterController" ) )
            {
                ImGui::Text( "speed: %f", length( velocityXZ ).getAsFloat() );
                ImGui::Text( "jump: %f", _jumpValue01 );
                ImGui::Text( "jump2: %f", _jumpValue02 );
            }
            ImGui::End();

        }

        virtual Matrix4 worldPose() const
        {
            return Matrix4( _dstate0._rotation, _dstate0._pos0 );
        }
		virtual Matrix4 worldPoseFoot() const
		{
			return Matrix4( _dstate0._rotation, phxCCTFootPositionGet( _cct ) );
		}

        virtual Vector3 upDirection() const
        {
            return _upDir;
        }

		virtual Vector3 footPosition() const
		{
			return phxCCTFootPositionGet( _cct );
		}
		virtual Vector3 centerPosition() const
		{
			return phxCCTCenterPositionGet( _cct );
		}
		virtual Vector3 deltaPosition() const
		{
			return _dstate0._vel * _deltaTime;
		}
        virtual Vector3 velocity() const
        {
            return _dstate0._vel;
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
    
    //////////////////////////////////////////////////////////////////////////
    ///
	void charAnimControllerCreate( CharacterAnimController** canim, GameScene* scene )
	{
		CharacterAnimController* ca = BX_NEW( bxDefaultAllocator(), CharacterAnimController );

		ca->_skel = bxAnimExt::loadSkelFromFile( resourceManagerGet(), "anim/human.skel" );
		ca->_clip[ECharacterAnimClip::eIDLE] = bxAnimExt::loadAnimFromFile( resourceManagerGet(), "anim/idle.anim" );
		ca->_clip[ECharacterAnimClip::eWALK] = bxAnimExt::loadAnimFromFile( resourceManagerGet(), "anim/run.anim" );
        ca->_clip[ECharacterAnimClip::eRUN] = bxAnimExt::loadAnimFromFile( resourceManagerGet(), "anim/fast_run.anim" );
        ca->_clip[ECharacterAnimClip::eJUMP] = bxAnimExt::loadAnimFromFile( resourceManagerGet(), "anim/jump.anim" );
		ca->_animCtx = bxAnim::contextInit( *ca->_skel );
		
        ca->_footIndexL = bxAnim::getJointByName( ca->_skel, "LeftFoot" );
        ca->_footIndexR = bxAnim::getJointByName( ca->_skel, "RightFoot" );

		canim[0] = ca;
	}
	void charAnimControllerDestroy( CharacterAnimController** canim )
	{
		if( !canim[0] )
			return;

		CharacterAnimController* ca = canim[0];
		for( int i = 0; i < ECharacterAnimClip::eCOUNT; ++i )
		{
			bxAnimExt::unloadAnimFromFile( resourceManagerGet(), &ca->_clip[i] );
		}
		bxAnimExt::unloadSkelFromFile( resourceManagerGet(), &ca->_skel );
		bxAnim::contextDeinit( &ca->_animCtx );

		BX_DELETE0( bxDefaultAllocator(), canim[0] );
	}
	void charAnimControllerTick( CharacterAnimController* canim, GameScene* scene, u64 deltaTimeUS )
	{
        const float deltaTimeS = (float)bxTime::toSeconds( deltaTimeUS );
        const float timeS = (float)bxTime::toSeconds( canim->_timeUS );

        CharacterControllerImpl* ccImpl = (CharacterControllerImpl*)scene->cct;
        const Vector3 ccVelocity = ccImpl->velocity();
        const Vector3 ccVelocityXZ = projectVectorOnPlane( ccVelocity, makePlane( ccImpl->upDirection(), Vector3( 0.f ) ) );
        const Vector3 ccVelocityY = ccVelocity - ccVelocityXZ;
        
        const float speed = length( ccVelocity ).getAsFloat();
        const float jumpBlendAlpha = smoothstep( 0.f, 0.9f, ccImpl->_jumpValue01 );
        const float rootBlendAlpha = maxOfPair( smoothstep( 0.02f, 1.5f, speed ), jumpBlendAlpha );
        //const float runBlendAlpha = ccImpl->_input.L2;// linearstep( 2.2f, 2.5f, speedXZ );
        
        if( ccImpl->_input.L2 )
            canim->_runBlendAlpha += deltaTimeS;
        else
            canim->_runBlendAlpha -= deltaTimeS;
        canim->_runBlendAlpha = clamp( canim->_runBlendAlpha, 0.f, 1.f );
        const float runBlendAlpha = canim->_runBlendAlpha;

        const float jumpEvalTime = linearstep( 0.01f, 1.99f, ccImpl->_jumpValue02 );

        CharacterAnimBlendTree btree;
        btree._branch[ECharacterAnimBranch::eROOT] = bxAnim_BlendBranch( ECharacterAnimLeaf::eIDLE | bxAnim::eBLEND_TREE_LEAF, ECharacterAnimBranch::eMOTION| bxAnim::eBLEND_TREE_BRANCH, rootBlendAlpha );
        btree._branch[ECharacterAnimBranch::eLOCO] = bxAnim_BlendBranch( ECharacterAnimLeaf::eWALK| bxAnim::eBLEND_TREE_LEAF, ECharacterAnimLeaf::eRUN| bxAnim::eBLEND_TREE_LEAF, runBlendAlpha );
        btree._branch[ECharacterAnimBranch::eMOTION] = bxAnim_BlendBranch( ECharacterAnimBranch::eLOCO | bxAnim::eBLEND_TREE_BRANCH, ECharacterAnimLeaf::eJUMP | bxAnim::eBLEND_TREE_LEAF, jumpBlendAlpha );
        btree._leaf[ECharacterAnimLeaf::eIDLE] = bxAnim_BlendLeaf( canim->_clip[ECharacterAnimClip::eIDLE], ::fmod( timeS, canim->_clip[ECharacterAnimClip::eIDLE]->duration ) );
        btree._leaf[ECharacterAnimLeaf::eWALK] = bxAnim_BlendLeaf( canim->_clip[ECharacterAnimClip::eWALK], ::fmod( timeS, canim->_clip[ECharacterAnimClip::eWALK]->duration ) );
        btree._leaf[ECharacterAnimLeaf::eRUN]  = bxAnim_BlendLeaf( canim->_clip[ECharacterAnimClip::eRUN] , ::fmod( timeS, canim->_clip[ECharacterAnimClip::eRUN]->duration ) );
        btree._leaf[ECharacterAnimLeaf::eJUMP] = bxAnim_BlendLeaf( canim->_clip[ECharacterAnimClip::eJUMP], canim->_clip[ECharacterAnimClip::eJUMP]->duration * jumpEvalTime );
        
        bxAnim::evaluateBlendTree( canim->_animCtx
                                   , ECharacterAnimBranch::eROOT | bxAnim::eBLEND_TREE_BRANCH
                                   , btree._branch, ECharacterAnimBranch::eCOUNT
                                   , btree._leaf, ECharacterAnimLeaf::eCOUNT
                                   , canim->_skel );
        bxAnim::evaluateCommandList( canim->_animCtx
                                     , btree._branch, ECharacterAnimBranch::eCOUNT
                                     , btree._leaf, ECharacterAnimLeaf::eCOUNT
                                     , canim->_skel );

        const Matrix4 rootPose = ccImpl->worldPoseFoot();
        bxAnim_Joint rootJoint = toAnimJoint_noScale( rootPose );
        bxAnim_Joint* localJoints = bxAnim::poseFromStack( canim->_animCtx, 0 );
        bxAnim_Joint* worldJoints = canim->_animCtx->poseCache[0];
		bxAnim_Joint* worldJointsZero = canim->_animCtx->poseCache[1];

		const Vector3 prevFootPositionL = canim->_prevFootPosL; //worldJointsZero[canim->_footIndexL].position;
		const Vector3 prevFootPositionR = canim->_prevFootPosR; //worldJointsZero[canim->_footIndexR].position;
		//bxAnim::evaluateClip( localJoints, clip, clipTimeS );
        bxAnim_Joint zeroJoint = bxAnim_Joint::identity();
        zeroJoint.rotation = rootJoint.rotation;
        bxAnimExt::localJointsToWorldJoints( worldJointsZero, localJoints, canim->_skel, zeroJoint );
        
        const Vector3 currFootPositionL = worldJointsZero[canim->_footIndexL].position;
        const Vector3 currFootPositionR = worldJointsZero[canim->_footIndexR].position;
        canim->_prevFootPosL = currFootPositionL;
        canim->_prevFootPosR = currFootPositionR;

        Vector3 footDisplacementL = currFootPositionL - prevFootPositionL;
        Vector3 footDisplacementR = currFootPositionR - prevFootPositionR;

		//Vector3 footDisplacement = maxPerElem( footDisplacementL, footDisplacementR );
		const Vector4 planeX = makePlane( rootPose.getCol0().getXYZ(), Vector3(0.f) );
		const Vector4 planeY = makePlane( rootPose.getCol1().getXYZ(), Vector3(0.f) );
		const Vector3 dirZ = rootPose.getCol2().getXYZ();

        footDisplacementL = projectVectorOnPlane( footDisplacementL, planeX );
        footDisplacementL = projectVectorOnPlane( footDisplacementL, planeY );

		footDisplacementR = projectVectorOnPlane( footDisplacementR, planeX );
		footDisplacementR = projectVectorOnPlane( footDisplacementR, planeY );

		floatInVec footDisplacementLValue = select( zeroVec, length( footDisplacementL ), dot( footDisplacementL, dirZ ) <= zeroVec );
        floatInVec footDisplacementRValue = select( zeroVec, length( footDisplacementR ), dot( footDisplacementR, dirZ ) <= zeroVec );

        const floatInVec footDisplacementValue = maxf4( footDisplacementLValue, footDisplacementRValue );

        //bxGfxDebugDraw::addLine( worldPose.getTranslation(), worldPose.getTranslation() + worldPose.getCol2().getXYZ() * footDisplacementValue, 0x0000FFFF, 1 );
        if( jumpBlendAlpha < FLT_EPSILON )
        {
            canim->_locomotion = lerp( deltaTimeS * 10.f, canim->_locomotion, footDisplacementValue.getAsFloat() );// footDisplacementValue.getAsFloat(), canim->_locomotion, 0.1f, deltaTimeS );
        }

        if( ImGui::Begin( "CharacterAnimController" ) )
        {
            ImGui::Text( "locomotion %f", canim->_locomotion );
            ImGui::Text( "rootBlendAlpha: %f", rootBlendAlpha );
            ImGui::Text( "runBlendAlpha: %f", runBlendAlpha );
        }
        ImGui::End();

		bxAnimExt::localJointsToWorldJoints( worldJoints, localJoints, canim->_skel, rootJoint );
		const float scale = 0.05f;
		const i16* parentIndices = TYPE_OFFSET_GET_POINTER( i16, canim->_skel->offsetParentIndices );
		for( int i = 0; i < canim->_skel->numJoints; ++i )
		{
            bxGfxDebugDraw::addSphere( Vector4( worldJoints[i].position, scale ), 0xFF0000FF, 1 );
			if( parentIndices[i] != -1 )
			{
                const bxAnim_Joint& parentJoint = worldJoints[parentIndices[i]];
                bxGfxDebugDraw::addLine( parentJoint.position, worldJoints[i].position, 0x00FF00FF, 1 );
			}
		}

        canim->_timeUS += deltaTimeUS;
	}

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct CharacterAnim
    {
        enum EJoint : u32
        {
            eJOINT_HIPS = 0,

            eJOINT_SPINE0,
            eJOINT_SPINE1,
            eJOINT_HEAD,

            eJOINT_LSHOULDER,
            eJOINT_LELBOW,
            eJOINT_LHAND,

            eJOINT_RSHOULDER,
            eJOINT_RELBOW,
            eJOINT_RHAND,

            eJOINT_LHIP,
            eJOINT_LKNEE,
            eJOINT_LFOOT,

            eJOINT_RHIP,
            eJOINT_RKNEE,
            eJOINT_RFOOT,

            eJOINT_COUNT,
        };
        static const i16 _parentIndices[EJoint::eJOINT_COUNT];
        static const Vector3 _bindPositionWorld[EJoint::eJOINT_COUNT];

        TransformTQ _localPose[EJoint::eJOINT_COUNT];
        TransformTQ _worldPose[EJoint::eJOINT_COUNT];

    };
    const i16 CharacterAnim::_parentIndices[CharacterAnim::EJoint::eJOINT_COUNT] =
    {
        -1,
        0, 1, 2,
        2, 4, 5,
        2, 7, 8,
        0, 10, 11,
        0, 13, 14,
    };

    const Vector3 CharacterAnim::_bindPositionWorld[CharacterAnim::EJoint::eJOINT_COUNT] =
    {
        Vector3( 0.f, 0.466f, 0.f ),
        Vector3( 0.f, 0.6f, 0.f ), Vector3( 0.f, 0.7f, 0.f ), Vector3( 0.f, 0.8f, 0.f ),
        Vector3( 0.15f, 0.633f, 0.f ), Vector3( 0.15f, 0.5f, 0.f ), Vector3( 0.15f, 0.350f, 0.f ),
        Vector3( -0.15f, 0.633f, 0.f ), Vector3( -0.15f, 0.5f, 0.f ), Vector3( -0.15f, 0.350f, 0.f ),
        Vector3( 0.10f, 0.4f, 0.f ), Vector3( 0.10f, 0.2f, 0.f ), Vector3( 0.10f, 0.0f, 0.f ),
        Vector3( -0.10f, 0.4f, 0.f ), Vector3( -0.10f, 0.2f, 0.f ), Vector3( -0.10f, 0.0f, 0.f ),
    };

    namespace
    {
        // quat (qr) aims z-axis along vector
        Quat quatAim( const Vector3& dir )
        {
            union Cvt
            {
                __m128 m128;
                struct { f32 x, y, z, w; };
            };

            Cvt v1 = { dir.get128() };
            Cvt qr;
            qr.x = v1.y;
            qr.y = -v1.x;
            qr.z = 0;
            qr.w = 1 - v1.z;

            Quat result( qr.m128 );
            const floatInVec d = dot( result, result );
            return select( Quat::identity(), result * rsqrtf4( d.get128() ), d > fltEpsVec );
        }
        inline Quat quatAim( const Vector3& P1, const Vector3& P2 )
        {
            return quatAim( normalize( P2 - P1 ) );
        }

        void computeLocalAndWorld( TransformTQ* worldPose, TransformTQ* localPose, const Vector3* bindPositionWorld, const i16* parentIndices, const TransformTQ& rootTQ, int begin, int end )
        {
            for( int i = begin; i <= end; ++i )
            {
                Vector3 dir;
                if( i < end )
                {
                    dir = bindPositionWorld[i + 1] - bindPositionWorld[i];
                }
                else
                {
                    dir = bindPositionWorld[i] - bindPositionWorld[i - 1];
                }

                dir = normalize( dir );
                Quat rotationWorld = quatAim( dir ) * Quat::rotationY( PI_HALF );
                Vector3 positionWorld = bindPositionWorld[i];

                TransformTQ parentTransform = TransformTQ::identity();
                if( parentIndices[i] != -1 )
                {
                    parentTransform = worldPose[parentIndices[i]];
                }

                const TransformTQ world = transform( rootTQ, TransformTQ( rotationWorld, positionWorld ) );
                const TransformTQ local = transformInv( parentTransform, world );

                worldPose[i] = world;
                localPose[i] = local;
            }
        }
    }////

    void charAnimCreate( CharacterAnim** canim, GameScene* scene, const Matrix4& worldPose, float scale )
    {
        CharacterAnim* ca = BX_NEW( bxDefaultAllocator(), CharacterAnim );

        const TransformTQ rootTQ( worldPose );
        Vector3 bindPoseScaled[CharacterAnim::eJOINT_COUNT];
        for( int i = 0; i < CharacterAnim::EJoint::eJOINT_COUNT; ++i )
        {
            bindPoseScaled[i] = ca->_bindPositionWorld[i] * scale;
        }
        ca->_localPose[CharacterAnim::eJOINT_HIPS] = TransformTQ( bindPoseScaled[CharacterAnim::eJOINT_HIPS] );
        ca->_worldPose[CharacterAnim::eJOINT_HIPS] = transform( rootTQ, ca->_localPose[CharacterAnim::eJOINT_HIPS] );

        computeLocalAndWorld( ca->_worldPose, ca->_localPose, bindPoseScaled, CharacterAnim::_parentIndices, rootTQ, CharacterAnim::eJOINT_SPINE0, CharacterAnim::eJOINT_HEAD );
        computeLocalAndWorld( ca->_worldPose, ca->_localPose, bindPoseScaled, CharacterAnim::_parentIndices, rootTQ, CharacterAnim::eJOINT_LSHOULDER, CharacterAnim::eJOINT_LHAND );
        computeLocalAndWorld( ca->_worldPose, ca->_localPose, bindPoseScaled, CharacterAnim::_parentIndices, rootTQ, CharacterAnim::eJOINT_RSHOULDER, CharacterAnim::eJOINT_RHAND );
        computeLocalAndWorld( ca->_worldPose, ca->_localPose, bindPoseScaled, CharacterAnim::_parentIndices, rootTQ, CharacterAnim::eJOINT_LHIP, CharacterAnim::eJOINT_LFOOT );
        computeLocalAndWorld( ca->_worldPose, ca->_localPose, bindPoseScaled, CharacterAnim::_parentIndices, rootTQ, CharacterAnim::eJOINT_RHIP, CharacterAnim::eJOINT_RFOOT );

        //for( int i = 0; i < CharacterAnim::EJoint::eJOINT_COUNT; ++i )
        //{
        //	ca->_pose[i] = mulAsVec4( worldWithScale, ca->_bindPose[i] );
        //}

        canim[0] = ca;
    }
    void charAnimDestroy( CharacterAnim** canim, GameScene* scene )
    {
        if( !canim[0] )
            return;

        BX_DELETE0( bxDefaultAllocator(), canim[0] );
    }
    void charAnimTick( CharacterAnim* canim, const Matrix4& worldPose, float deltaTime )
    {

        const TransformTQ rootTQ( worldPose );
        canim->_worldPose[CharacterAnim::eJOINT_HIPS] = transform( rootTQ, canim->_localPose[CharacterAnim::eJOINT_HIPS] );
        for( int i = CharacterAnim::eJOINT_SPINE0; i < CharacterAnim::eJOINT_COUNT; ++i )
        {
            const i16 parentIndex = canim->_parentIndices[i];
            const TransformTQ& parent = canim->_worldPose[parentIndex];
            const TransformTQ& world = transform( parent, canim->_localPose[i] );

            canim->_worldPose[i] = world;

            bxGfxDebugDraw::addSphere( Vector4( world.t, 0.05f ), 0xFF00FFFF, 1 );
            bxGfxDebugDraw::addLine( parent.t, world.t, 0x00FF00FF, 1 );

            //bxGfxDebugDraw::addAxes( Matrix4( world.q, world.t ) ); // , 0x00FF00FF, 1 );
        }
    }
}///

