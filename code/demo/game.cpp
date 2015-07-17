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
#include <gfx/gfx_material.h>

#include <smmintrin.h>



namespace bxGame
{
    struct FlockParticles
    {
        void* memoryHandle;
        
        Vector3* pos0;
        Vector3* vel;
        f32* massInv;

        i32 size;
        i32 capacity;
    };
    void _FlockParticles_allocate( FlockParticles* fp, int capacity )
    {
        if ( fp->capacity >= capacity )
            return;

        int memSize = 0;
        memSize += capacity * sizeof( *fp->pos0 );
        memSize += capacity * sizeof( *fp->vel );
        memSize += capacity * sizeof( *fp->massInv );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );

        FlockParticles newFp;
        newFp.memoryHandle = mem;
        newFp.size = fp->size;
        newFp.capacity = capacity;
        newFp.pos0 = chunker.add< Vector3 >( capacity );
        newFp.vel = chunker.add< Vector3 >( capacity );
        newFp.massInv = chunker.add< f32 >( capacity );
        chunker.check();

        if( fp->size )
        {
            BX_CONTAINER_COPY_DATA( &newFp, fp, pos0 );
            BX_CONTAINER_COPY_DATA( &newFp, fp, vel );
            BX_CONTAINER_COPY_DATA( &newFp, fp, massInv );
        }

        BX_FREE0( bxDefaultAllocator(), fp->memoryHandle );
        fp[0] = newFp;
    }
    int _FlockParticles_add( FlockParticles* fp, const Vector3& pos )
    {
        if( fp->size + 1 >= fp->capacity )
        {
            _FlockParticles_allocate( fp, fp->capacity * 2 + 8 );
        }

        bxRandomGen rnd( u32( u64( fp ) ^ ((fp->size << fp->capacity) << 13) ) );

        const int index = fp->size++;
        fp->pos0[index] = pos;
        fp->vel[index] = Vector3( 0.f );
        fp->massInv[index] = 1.f / bxRand::randomFloat( rnd, 1.f, 0.5f );
        return index;
    }

    ////
    ////
    union FlockHashmap_Item
    {
        u64 hash;
        struct {
            u32 index;
            u32 next;
        };
    };
    inline FlockHashmap_Item makeItem( u64 hash )
    {
        FlockHashmap_Item item = { hash };
        return item;
    }
    inline bool isValid( FlockHashmap_Item item ) { return item.hash != UINT64_MAX; }
    inline bool isSentinel( FlockHashmap_Item item ) { return item.next == UINT32_MAX; }

    union FlockHashmap_Key
    {
        u64 hash;
        struct {
            i16 x;
            i16 y;
            i16 z;
            i16 w;
        };
    };

    struct FlockHashmap
    {
        hashmap_t map;
        array_t< FlockHashmap_Item > items;
    };

    inline FlockHashmap_Key makeKey( int x, int y, int z )
    {
        FlockHashmap_Key key = { 0 };
        SYS_ASSERT( x < (INT16_MAX) && x >( INT16_MIN ) );
        SYS_ASSERT( y < (INT16_MAX) && y >( INT16_MIN ) );
        SYS_ASSERT( z < (INT16_MAX) && z >( INT16_MIN ) );
        key.x = x;
        key.y = y;
        key.z = z;
        key.w = 1;
        return key;
    }
    inline FlockHashmap_Key makeKey( const Vector3& pos, float cellSizeInv )
    {
        const Vector3 gridPos = pos * cellSizeInv;
        const __m128i rounded = _mm_cvtps_epi32( _mm_round_ps( gridPos.get128(), _MM_FROUND_FLOOR ) );
        const SSEScalar tmp( rounded );
        return makeKey( tmp.ix, tmp.iy, tmp.iz );
    }



    void hashMapAdd( FlockHashmap* hmap, const Vector3& pos, int index, float cellSizeInv )
    {
        FlockHashmap_Key key = makeKey( pos, cellSizeInv );
        hashmap_t::cell_t* cell = hashmap::lookup( hmap->map, key.hash );
        if( !cell )
        {
            cell = hashmap::insert( hmap->map, key.hash );
            FlockHashmap_Item begin;
            begin.index = index;
            begin.next = UINT32_MAX;
            cell->value = begin.hash;
        }
        else
        {
            FlockHashmap_Item begin = { cell->value };
            FlockHashmap_Item item = { 0 };
            item.index = index;
            item.next = begin.next;
            begin.next = array::push_back( hmap->items, item );
            cell->value = begin.hash;
        }
    }
    
    FlockHashmap_Item hashMapItemFirst( const FlockHashmap& hmap, FlockHashmap_Key key )
    {
        const hashmap_t::cell_t* cell = hashmap::lookup( hmap.map, key.hash );
        return (cell) ? makeItem( cell->value ) : makeItem( UINT64_MAX );
    }
    FlockHashmap_Item hashMapItemNext( const FlockHashmap& hmap, const FlockHashmap_Item current )
    {
        return (current.next != UINT64_MAX) ? hmap.items[current.next] : makeItem( UINT64_MAX );
    }

    void flock_hashMapDebugDraw( FlockHashmap* hmap, float cellSize, u32 color )
    {
        const Vector3 cellExt( cellSize * 0.5f );
        hashmap::iterator it( hmap->map );
        while ( it.next() )
        {
            FlockHashmap_Key key = { it->key };

            Vector3 center( key.x, key.y, key.z );
            center *= cellSize;
            center += cellExt;

            bxGfxDebugDraw::addBox( Matrix4::translation( center ), cellExt, color, 1 );
        }
    }

    struct FlockParams
    {
        f32 boidRadius;
        f32 separation;
        f32 alignment;
        f32 cohesion;
        f32 attraction;
        f32 cellSize;

        FlockParams()
            : boidRadius( 1.f )
            , separation( 1.0f )
            , alignment( 0.2f )
            , cohesion( 0.05f )
            , attraction( 0.6f )
            , cellSize( 2.f )
        {}
    };
    void _FlockParams_show( FlockParams* params )
    {
        if ( ImGui::Begin( "Flock" ) )
        {
            ImGui::InputFloat( "boidRadius", &params->boidRadius );
            ImGui::SliderFloat( "separation", &params->separation, 0.f, 1.f );
            ImGui::SliderFloat( "alignment", &params->alignment, 0.f, 1.f );
            ImGui::SliderFloat( "cohesion", &params->cohesion, 0.f, 1.f );
            ImGui::SliderFloat( "attraction", &params->attraction, 0.f, 1.f );
            ImGui::InputFloat( "cellSize", &params->cellSize );
        }
        ImGui::End();
    }

    struct Flock
    {
        FlockParticles particles;
        FlockParams params;
        FlockHashmap hmap;
        
        bxGfx_HMesh hMesh;
        bxGfx_HInstanceBuffer hInstanceBuffer;
        
        f32 _dtAcc;
    };

    Flock* flock_new()
    {
        Flock* fl = BX_NEW( bxDefaultAllocator(), Flock );
        memset( &fl->particles, 0x00, sizeof( FlockParticles ) );
        fl->_dtAcc = 0.f;
        return fl;
    }

    void flock_delete( Flock** flock )
    {
        if ( !flock[0] )
            return;

        bxGfx::mesh_release( &flock[0]->hMesh );

        BX_FREE0( bxDefaultAllocator(), flock[0]->particles.memoryHandle );
        BX_DELETE0( bxDefaultAllocator(), flock[0] );
    }

    void flock_init( Flock* flock, int nBoids, const Vector3& center, float radius )
    {
        _FlockParticles_allocate( &flock->particles, nBoids );

        bxRandomGen rnd( TypeReinterpert( radius ).u );
        for( int iboid = 0; iboid < nBoids; ++iboid )
        {
            const float dx = rnd.getf( -radius, radius );
            const float dy = rnd.getf( -radius, radius );
            const float dz = rnd.getf( -radius, radius );

            const Vector3 dpos = Vector3( dx, dy, dz );
            const Vector3 pos = center + dpos;

            _FlockParticles_add( &flock->particles, pos );
        }
    }

    void flock_loadResources( Flock* flock, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGfx_HWorld gfxWorld )
    {
        flock->hMesh = bxGfx::mesh_create();
        flock->hInstanceBuffer = bxGfx::instanceBuffer_create( flock->particles.size );

        bxGdiRenderSource* rsource = bxGfx::globalResources()->mesh.sphere;
        bxGfx::mesh_setStreams( flock->hMesh, dev, rsource );

        bxGdiShaderFx_Instance* materialFx = bxGfxMaterialManager::findMaterial( "blue" );
        bxGfx::mesh_setShader( flock->hMesh, dev, resourceManager, materialFx );
        
        bxGfx::world_meshAdd( gfxWorld, flock->hMesh, flock->hInstanceBuffer );
    }


    inline bool isInNeighbourhood( const Vector3& pos, const Vector3& posB, float radius )
    {
        const Vector3 vec = posB - pos;
        const float dd = lengthSqr( vec ).getAsFloat();

        return ( dd <= radius * radius );
    }
    //Quat quatAim( const Vector3& v )
    //{
    //    const Vector3 vn = normalize( v );

    //    Quat qr;
    //    qr.setX( v.getY() );
    //    qr.setY( -v.getX() );
    //    qr.setZ( zeroVec );
    //    qr.setW( oneVec - v.getZ() );
    //    return normalize( qr );
    //}
    //Quat quatAim( const Vector3& p1, const Vector3& p2 )
    //{
    //    const Vector3 v = p2 - p1;
    //    return quatAim( v );
    //}

    void flock_simulate( Flock* flock, float deltaTime )
    {
        const Vector3 com( 0.f );

        const float boidRadius = flock->params.boidRadius;
        const float separation = flock->params.separation;
        const float alignment  = flock->params.alignment;
        const float cohesion   = flock->params.cohesion;
        const float attraction = flock->params.attraction;
        const float cellSize   = flock->params.cellSize;
        const float cellSizeInv = 1.f / cellSize;

        const float cellSizeSqr = cellSize * cellSize;
        const float boidRadiusSqr = boidRadius * boidRadius;

        FlockParticles* fp = &flock->particles;

        const int nBoids = flock->particles.size;

        for( int iboid = 0; iboid < nBoids; ++iboid )
        {
            Vector3 separationVec( 0.f );
            Vector3 cohesionVec( 0.f );
            Vector3 alignmentVec( 0.f );

            Vector3 pos = fp->pos0[iboid];
            Vector3 vel = fp->vel[iboid];

            const FlockHashmap_Key mainKey = makeKey( pos, cellSizeInv);
            hashmap_t::cell_t* mainCell = hashmap::lookup( flock->hmap.map, mainKey.hash );
            int xBegin = -1, xEnd = 1;
            int yBegin = -1, yEnd = 1;
            int zBegin = -1, zEnd = 1;


            int neighboursAlignment = 0;
            int neighboursCohesion = 0;
            for( int iz = zBegin; iz <= zEnd; ++iz )
            {
                for( int iy = yBegin; iy <= yEnd; ++iy )
                {
                    for( int ix = xBegin; ix <= xEnd; ++ix )
                    {
                        FlockHashmap_Key key = makeKey( mainKey.x + ix, mainKey.y + iy, mainKey.z + iz );
                        FlockHashmap_Item item = hashMapItemFirst( flock->hmap, key );
                        
                        while( !isSentinel( item ) )
                        {
                            const int iboid1 = item.index;
                            if( iboid1 != iboid )
                            {
                                const Vector3& posB = fp->pos0[iboid1];
                                const Vector3& velB = fp->vel[iboid1];

                                {
                                    const Vector3 vec = posB - pos;
                                    const floatInVec vecLen = length( vec );
                                    const floatInVec displ = maxf4( floatInVec( boidRadius ) - vecLen, zeroVec );
                                    Vector3 dpos = vec * floatInVec( recipf4_newtonrapson( vecLen.get128() ) ) * displ;
                                    separationVec -= dpos;
                                }
                                
                                if( isInNeighbourhood( pos, posB, cellSize ) )
                                {
                                    alignmentVec += velB;
                                    ++neighboursAlignment;

                                    cohesionVec += posB;
                                    ++neighboursCohesion;
                                }
                            }
                            item = hashMapItemNext( flock->hmap, item );
                        }
                    }
                }
            }

            {
                separationVec = normalizeSafe( separationVec );
                if( neighboursAlignment > 0 )
                {
                    alignmentVec = normalizeSafe( ( alignmentVec / (f32)neighboursAlignment ) - vel );
                }
                if( neighboursCohesion )
                {
                    cohesionVec = normalizeSafe( ( cohesionVec / (f32)neighboursCohesion ) - pos );
                }
            }


            Vector3 steering( 0.f );
            steering += separationVec * separation;
            steering += alignmentVec * alignment;
            steering += cohesionVec * cohesion;
            steering += normalizeSafe( com - pos ) * attraction;

            vel += ( steering * fp->massInv[iboid] ) * deltaTime;
            pos += vel * deltaTime;

            fp->pos0[iboid] = pos;
            fp->vel[iboid] = vel;
        }
    }

    void flock_tick( Flock* flock, float deltaTime )
    {
        rmt_ScopedCPUSample( Flock_tick );

        const float deltaTimeFixed = 1.f / 60.f;
        const float cellSizeInv = 1.f / flock->params.cellSize;

        //deltaTime = deltaTimeFixed;
        flock->_dtAcc += deltaTime;

        

        while( flock->_dtAcc >= deltaTimeFixed )
        {
            {
                rmt_ScopedCPUSample( Flock_recreateGrid );
                hashmap::clear( flock->hmap.map );
                for( int iboid = 0; iboid < flock->particles.size; ++iboid )
                {
                    hashMapAdd( &flock->hmap, flock->particles.pos0[iboid], iboid, cellSizeInv );
                }
            }
            
            {
                rmt_ScopedCPUSample( Flock_simulate );
                flock_simulate( flock, deltaTimeFixed );
            }
            flock->_dtAcc -= deltaTimeFixed;
        }

        //{
        //    rmt_ScopedCPUSample( Flock_debugDraw );
        //    flock_hashMapDebugDraw( &flock->hmap, flock->params.cellSize, 0x222222FF );
        //}

        FlockParticles* fp = &flock->particles;

        const int nBoids = flock->particles.size;
        for( int iboid = 0; iboid < nBoids; ++iboid )
        {
            const Vector3& pos = fp->pos0[iboid];
            const Vector3& vel = fp->vel[iboid];

            //Vector3 posGrid( _mm_round_ps( pos.get128(), _MM_FROUND_NINT ) );

            //bxGfxDebugDraw::addSphere( Vector4( pos, 0.1f ), 0xFFFF00FF, true );
            const Matrix3 rotation = Matrix3::identity(); // createBasis( normalizeSafe( vel ) );
            const Matrix4 pose = appendScale( Matrix4( rotation, pos ), Vector3( 0.1f ) );
            bxGfx::instanceBuffer_set( flock->hInstanceBuffer, &pose, 1, iboid );
            //bxGfxDebugDraw::addLine( pos, pos + vel, 0x0000FFFF, true );
            
            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol0(), 0x0F00FFFF, true );
            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol1(), 0xF000FFFF, true );
            //bxGfxDebugDraw::addLine( pos, pos + rot.getCol2(), 0xFF00FFFF, true );
        }

        _FlockParams_show( &flock->params );
    }

}///


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace bxDesignBlock_Util
{
    static inline bxDesignBlock::Handle makeInvalidHandle()
    {
        bxDesignBlock::Handle h = { 0 };
        return h;
    }
    static inline bxDesignBlock::Handle makeHandle( id_t id )
    {
        bxDesignBlock::Handle h = { id.hash };
        return h;
    }
    static inline id_t makeId( bxDesignBlock::Handle h )
    {
        return make_id( h.h );
    }

    inline u32 makeNameHash( bxDesignBlock* _this, const char* name )
    {
        const u32 SEED = u32( _this ) ^ 0xDEADF00D;
        return murmur3_hash32( name, string::length( name ), SEED );
    }

    struct CreateDesc
    {
        bxDesignBlock::Handle h;
        bxGdiShaderFx_Instance* material;
    };

}///
using namespace bxDesignBlock_Util;
struct bxDesignBlock_Impl : public bxDesignBlock
{
    static const u64 DEFAULT_TAG = bxMakeTag64<'_', 'D', 'B', 'L', 'O', 'C', 'K', '_'>::tag;
    enum
    {
        eMAX_BLOCKS = 0xFFFF,
    };
    struct Data
    {
        Matrix4* pose;
        bxDesignBlock::Shape* shape;

        u32* name;
        u64* tag;

        bxGfx_HMeshInstance* gfxMeshI;
        bxPhx_HShape* phxShape;

        void* memoryHandle;
        i32 size;
        i32 capacity;
    };

    id_array_t< eMAX_BLOCKS > _idContainer;
    Data _data;

    array_t< Handle > _list_updatePose;
    array_t< CreateDesc > _list_create;
    array_t< Handle > _list_release;

    u32 _flag_releaseAll : 1;

    void startup()
    {
        memset( &_data, 0x00, sizeof( bxDesignBlock_Impl::Data ) );
        _flag_releaseAll = 0;
    }
    void shutdown()
    {
        BX_FREE0( bxDefaultAllocator(), _data.memoryHandle );
    }

    ////
    ////

    inline int dataIndex_get( Handle h )
    {
        id_t id = makeId( h );
        return ( id_array::has( _idContainer, id ) ) ? id_array::index( _idContainer, id ) : -1;
    }

    void _AllocateData( int newcap )
    {
        if ( newcap <= _data.capacity )
            return;

        int memSize = 0;
        memSize += newcap * sizeof( *_data.pose );
        memSize += newcap * sizeof( *_data.shape );
        memSize += newcap * sizeof( *_data.name );
        memSize += newcap * sizeof( *_data.tag );
        memSize += newcap * sizeof( *_data.gfxMeshI );
        memSize += newcap * sizeof( *_data.phxShape );
        

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        
        Data newdata;
        newdata.memoryHandle = mem;
        newdata.size = _data.size;
        newdata.capacity = newcap;
        
        bxBufferChunker chunker( mem, memSize );
        newdata.pose = chunker.add< Matrix4 >( newcap );
        newdata.shape = chunker.add< bxDesignBlock::Shape >( newcap );
        newdata.name = chunker.add< u32 >( newcap );
        newdata.tag  = chunker.add< u64 >( newcap );
        newdata.gfxMeshI = chunker.add< bxGfx_HMeshInstance >( newcap );
        newdata.phxShape = chunker.add< bxPhx_HShape >( newcap );
        
        chunker.check();

        if( _data.size )
        {
            BX_CONTAINER_COPY_DATA( &newdata, &_data, pose );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, shape );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, name );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, tag );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, gfxMeshI );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, phxShape );
            
        }

        BX_FREE0( bxDefaultAllocator(), _data.memoryHandle );
        _data = newdata;
    }
    
    

    int findByHash( u32 hash )
    {
        return array::find1( _data.name, _data.name + _data.size, array::OpEqual<u32>( hash ) );
    }

    virtual Handle create( const char* name, const Matrix4& pose, const Shape& shape, const char* material )
    {
        const u32 nameHash = makeNameHash( this, name );
        int index = findByHash( nameHash );
        if( index != -1 )
        {
            bxLogError( "Block with given name '%s' already exists at index: %d!", name, index );
            return makeInvalidHandle();
        }

        SYS_ASSERT( id_array::size( _idContainer ) == _data.size );

        id_t id = id_array::create( _idContainer );
        index = id_array::index( _idContainer, id );
        while( index >= _data.capacity )
        {
            _AllocateData( _data.capacity * 2 + 8 );
        }

        SYS_ASSERT( index == _data.size );
        ++_data.size;

        _data.pose[index] = pose;
        _data.shape[index] = shape;
        _data.name[index] = nameHash;
        _data.tag[index] = DEFAULT_TAG;
        _data.gfxMeshI[index] = makeInvalidHandle< bxGfx_HMeshInstance >();
        _data.phxShape[index] = makeInvalidHandle< bxPhx_HShape >();
        
        Handle handle = makeHandle( id );

        CreateDesc createDesc;
        createDesc.h = handle;
        createDesc.material = bxGfxMaterialManager::findMaterial( material );
        if( !createDesc.material )
        {
            createDesc.material = bxGfxMaterialManager::findMaterial( "red" );
        }
        array::push_back( _list_create, createDesc );

        return handle;

    }
    virtual void release( Handle* h )
    {
        id_t id = makeId( h[0] );
        if( !id_array::has( _idContainer, id ) )
            return;

        array::push_back( _list_release, h[0] );

        //int thisIndex = id_array::index( _idContainer, id );
        //int lastIndex = id_array::size( _idContainer ) - 1;
        //SYS_ASSERT( lastIndex == (_data.size - 1) );

        //id_array::destroy( _idContainer, id );

        //_data.name[thisIndex]     = _data.name[lastIndex];
        //_data.tag[thisIndex]      = _data.tag[lastIndex];
        //_data.gfxMeshI[thisIndex]     = _data.gfxMeshI[lastIndex];
        //_data.phxShape[thisIndex]   = _data.phxShape[lastIndex];

        h[0] = makeInvalidHandle();
    }

    void cleanUp()
    {
        _flag_releaseAll = 1;
    }

    void bxDesignBlock::manageResources( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxPhx_CollisionSpace* cs, bxGfx_HWorld gfxWorld )
    {
        if( _flag_releaseAll )
        { 
            _flag_releaseAll = 0;

            for( int i = 0; i < _data.size; ++i )
            {
                bxPhx::shape_release( cs, &_data.phxShape[i] );
                bxGfx::world_meshRemoveAndRelease( &_data.gfxMeshI[i] );
            }
            id_array::destroyAll( _idContainer );
            array::clear( _list_updatePose );
            array::clear( _list_create );
            array::clear( _list_release );
        }
        
        ////
        for( int ihandle = 0; ihandle < array::size( _list_create ); ++ihandle )
        {
            const CreateDesc& createDesc = _list_create[ihandle];
            Handle h = createDesc.h;
            int index = dataIndex_get( h );
            if( index == -1 )
                continue;

            SYS_ASSERT( _data.phxShape[index].h == 0 );
            SYS_ASSERT( _data.gfxMeshI[index].h == 0 );

            const bxDesignBlock::Shape& shape = _data.shape[index];
            const Matrix4& pose = _data.pose[index];
            Vector3 scale( 1.f );
            bxPhx_HShape phxShape = makeInvalidHandle< bxPhx_HShape >();
            bxGdiRenderSource* rsource = 0;
            bxGdiShaderFx_Instance* fxI = createDesc.material;
            switch( shape.type )
            {
            case Shape::eSPHERE:
                {
                    phxShape = bxPhx::shape_createSphere( cs, Vector4( pose.getTranslation(), shape.radius ) );
                    scale = Vector3( shape.radius * 2.f );
                    rsource = bxGfx::globalResources()->mesh.sphere;
                }break;
            case Shape::eCAPSULE:
                {
                    SYS_NOT_IMPLEMENTED;
                }break;
            case Shape::eBOX:
                {
                    phxShape = bxPhx::shape_createBox( cs, pose.getTranslation(), Quat( pose.getUpper3x3() ), Vector3( shape.vec4 ) );
                    scale = Vector3( shape.vec4 ) * 2.f;
                    rsource = bxGfx::globalResources()->mesh.box;
                }break;
            }
            
            bxGfx_HMesh hmesh = bxGfx::mesh_create();
            bxGfx_HInstanceBuffer hibuff = bxGfx::instanceBuffer_create( 1 );

            bxGfx::mesh_setStreams( hmesh, dev, rsource );
            bxGfx::mesh_setShader( hmesh, dev, resourceManager, fxI );

            const Matrix4 poseWithScale = appendScale( pose, scale );
            bxGfx::instanceBuffer_set( hibuff, &poseWithScale, 1, 0 );

            bxGfx_HMeshInstance gfxMeshI = bxGfx::world_meshAdd( gfxWorld, hmesh, hibuff );

            _data.phxShape[index] = phxShape;
            _data.gfxMeshI[index] = gfxMeshI;
        }
        array::clear( _list_create );

        ////
        for( int ihandle = 0; ihandle < array::size( _list_release ); ++ihandle )
        {
            id_t id = makeId( _list_release[ihandle] );
            if( !id_array::has( _idContainer, id ) )
                return;

            int thisIndex = id_array::index( _idContainer, id );
            int lastIndex = id_array::size( _idContainer ) - 1;
            SYS_ASSERT( lastIndex == (_data.size - 1) );

            id_array::destroy( _idContainer, id );

            bxGfx::world_meshRemoveAndRelease( &_data.gfxMeshI[thisIndex] );
            bxPhx::shape_release( cs, &_data.phxShape[thisIndex] );

            _data.pose[thisIndex]     = _data.pose[lastIndex];
            _data.shape[thisIndex]    = _data.shape[lastIndex];
            _data.name[thisIndex]     = _data.name[lastIndex];
            _data.tag[thisIndex]      = _data.tag[lastIndex];
            _data.gfxMeshI[thisIndex] = _data.gfxMeshI[lastIndex];
            _data.phxShape[thisIndex] = _data.phxShape[lastIndex];
        }
        array::clear( _list_release );
    }


    virtual void assignTag( Handle h, u64 tag )
    {
        int dataIndex = dataIndex_get( h );
        if( dataIndex == -1 )
            return;

        _data.tag[dataIndex] = tag;
    }
    //virtual void assignMesh( Handle h, bxGfx_HMeshInstance hmeshi )
    //{
    //    int dataIndex = dataIndex_get( h );
    //    if( dataIndex == -1 )
    //        return;

    //    _data.meshi[dataIndex] = hmeshi;

    //    array::push_back( _list_updateMeshInstance, h );

    //}
    //virtual void assignCollisionShape( Handle h, bxPhx_HShape hshape )
    //{
    //    int dataIndex = dataIndex_get( h );
    //    if( dataIndex == -1 )
    //        return;

    //    _data.cshape[dataIndex] = hshape;
    //    
    //    array::push_back( _list_updateMeshInstance, h );
    //}

    virtual void tick( bxPhx_CollisionSpace* cs, bxGfx_HWorld gfxWorld )
    {
        {
            for( int i = 0; i < array::size( _list_updatePose ); ++i )
            {
                int dataIndex = dataIndex_get( _list_updatePose[i] );
                if( dataIndex == -1 )
                    continue;

                bxGfx_HMesh hmesh;
                bxGfx_HInstanceBuffer hibuffer;
                bxGfx::world_instance( &hmesh, &hibuffer, _data.gfxMeshI[dataIndex] );
                
                const Matrix4 pose = bxPhx::shape_pose( cs, _data.phxShape[dataIndex] );
                bxGfx::instanceBuffer_set( hibuffer, &pose, 1, 0 );
            }
            array::clear( _list_updatePose );
        }
    }

    ////
    ////
};

bxDesignBlock* bxDesignBlock_new()
{
    bxDesignBlock_Impl* impl = BX_NEW( bxDefaultAllocator(), bxDesignBlock_Impl );
    impl->startup();
    return impl;
}

void bxDesignBlock_delete( bxDesignBlock** dblock )
{
    bxDesignBlock_Impl* impl = (bxDesignBlock_Impl*)dblock[0];
    impl->shutdown();
    BX_DELETE0( bxDefaultAllocator(), impl );

    dblock[0] = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bxDesignBlock_SceneScriptCallback::bxDesignBlock_SceneScriptCallback()
    : dblock(0)
{
    memset( &desc.name, 0x00, sizeof( desc.name ) );
    memset( &desc.material, 0x00, sizeof( desc.material ) );
    memset( &desc.shape, 0x00, sizeof( bxDesignBlock::Shape ) );
    desc.pose = Matrix4::identity();
}

void bxDesignBlock_SceneScriptCallback::onCreate( const char* typeName, const char* objectName )
{
    const size_t bufferSize = sizeof( desc.name ) - 1;
    sprintf_s( desc.name, bufferSize, "%s", objectName );
}

void bxDesignBlock_SceneScriptCallback::onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData )
{
    if( string::equal( attrName, "pos" ) )
    {
        if( attribData.dataSizeInBytes() != 12 )
            goto label_besignBlockAttributeInvalidSize;
        
        desc.pose.setTranslation( Vector3( attribData.fnumber[0], attribData.fnumber[1], attribData.fnumber[2] ) );
    }
    else if( string::equal( attrName, "rot" ) )
    {
        if( attribData.dataSizeInBytes() != 12 )
            goto label_besignBlockAttributeInvalidSize;

        const Vector3 eulerXYZ( attribData.fnumber[0], attribData.fnumber[1], attribData.fnumber[2] );
        desc.pose.setUpper3x3( Matrix3::rotationZYX( eulerXYZ ) );
    }
    else if( string::equal( attrName, "material" ) )
    {
        if( attribData.dataSizeInBytes() <= 0 )
            goto label_besignBlockAttributeInvalidSize;

        const size_t bufferSize = sizeof( desc.material ) - 1;
        sprintf_s( desc.material, bufferSize, "%s", attribData.string );
    }
    else if( string::equal( attrName, "shape" ) )
    {
        if( attribData.dataSizeInBytes() < 4 )
            goto label_besignBlockAttributeInvalidSize;

        if( attribData.dataSizeInBytes() == 4 )
        {
            desc.shape = bxDesignBlock::Shape( attribData.fnumber[0] );
        }
        else if( attribData.dataSizeInBytes() == 8 )
        {
            desc.shape = bxDesignBlock::Shape( attribData.fnumber[0], attribData.fnumber[1] );
        }
        else if( attribData.dataSizeInBytes() == 12 )
        {
            desc.shape = bxDesignBlock::Shape( attribData.fnumber[0], attribData.fnumber[1], attribData.fnumber[2] );
        }
        else
        {
            goto label_besignBlockAttributeInvalidSize;
        }
    }

    return;

label_besignBlockAttributeInvalidSize:
    bxLogError( "invalid data size" );
}

void bxDesignBlock_SceneScriptCallback::onCommand( const char* cmdName, const bxAsciiScript_AttribData& args )
{
    (void)args;
    if( string::equal( cmdName, "dblock_commit" ) )
    {
        int descValid = 1;
        descValid &= string::length( desc.name ) > 0;
        descValid &= string::length( desc.material ) > 0;
        descValid &= ( desc.shape.type != UINT32_MAX  );

        if( !descValid )
        {
            bxLogError( "Design block desc invalid!" );
        }
        else
        {
            dblock->create( desc.name, desc.pose, desc.shape, desc.material );
        }
    }
}
