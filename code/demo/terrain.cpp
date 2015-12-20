#include "terrain.h"

#include <util/type.h>
#include <util/memory.h>
#include <util/debug.h>
#include <util/grid.h>
#include <util/buffer_utils.h>

#include <gdi/gdi_render_source.h>
#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>
#include <phx/phx.h>

#include <smmintrin.h>
#include <cmath>

#include <engine/engine.h>

#include "game.h"
#include "scene.h"
#include "util/common.h"
#include "util/perlin_noise.h"

namespace bx
{
    union i32x2
    {
        struct
        {
            i32 x, y;
        };
        i32 xyz[2];

        i32x2() : x( 0 ), y( 0 ) {}
        i32x2( i32 all ) : x( all ), y( all ) {}
        i32x2( i32 a, i32 b ) : x( a ), y( b ) {}
    };
    union i32x3
    {
        struct
        {
            i32 x, y, z;
        };
        i32 xyz[3];

        i32x3(): x(0), y(0), z(0) {}
        i32x3( i32 all ) : x( all ), y( all ), z( all ) {}
        i32x3( i32 a, i32 b, i32 c ): x( a ), y( b ), z( c ) {}
    };
    inline Vector3 toVector3( const i32x3& i ) { return Vector3( (float)i.x, (float)i.y, (float)i.z ); }


    enum ECellFlag : u8
    {
        NEW = 0x1,
    };

    struct Terrain
    {
        Vector3 _prevPlayerPosition = Vector3( -10000.f );
        f32 _tileSize = 25.f;
        i32 _radius = 5;
        i32 _tileSubdiv = 4;

        u32 _centerGridSpaceX = _radius;
        u32 _centerGridSpaceY = _radius;

        bxGdiIndexBuffer _tileIndices;

        //bxGrid _grid;
        void* _memoryHandle = nullptr;
        GfxMeshInstance** _meshInstances = nullptr;
        PhxActor** _phxActors = nullptr;
        bxGdiRenderSource* _renderSources = nullptr;
        i32x3* _cellWorldCoords = nullptr;
        f32*   _cellHeightSamples = nullptr;
        u8*    _cellFlags = nullptr;
    };
    
    inline int radiusToLength( int rad ) { return rad * 2 + 1; }
    inline int radiusToDataLength( int rad ) { int a = radiusToLength( rad ); return a*a; }
    inline int xyToIndex( int x, int y, int w ) { return y * w + x; }
    inline i32x2 indexToXY( int index, int w ) { return i32x2( index % w, index / w ); }

    inline int _ComputeNumQuadsInRow( int subdiv )
    {
        return 1 << ( subdiv - 1 );
    }
    inline int _ComputeNumQuads( int subdiv )
    {
        return 1 << ( 2 * ( subdiv - 1 ) );
    }

    inline float* _GetCellHeightSamples( const Terrain* terr, int cellIndex )
    {
        const int numQuadsInRow = _ComputeNumQuadsInRow( terr->_tileSubdiv );
        const int numSamplesInRow = numQuadsInRow + 1;
        const int numSamplesInCell = numSamplesInRow * numSamplesInRow;
        float* result = terr->_cellHeightSamples + numSamplesInCell * cellIndex;
        SYS_ASSERT( (uptr)(result + numSamplesInCell) <= (uptr)(terr->_cellHeightSamples + numSamplesInCell * radiusToDataLength( terr->_radius ) ) );
        return result;
    }
    
    void _TerrainMeshCreateIndices( bxGdiIndexBuffer* ibuffer, bxGdiDeviceBackend* dev, int subdiv )
    {
        const int numQuadsInRow = _ComputeNumQuadsInRow( subdiv );
        const int numQuads = _ComputeNumQuads( subdiv );
        const int numTriangles = numQuads * 2;
        const int numIndices = numTriangles * 3;
        const int numVertices = numQuads * 4;

        SYS_ASSERT( numQuadsInRow*numQuadsInRow == numQuads );
        SYS_ASSERT( numVertices < 0xFFFF );

        const u16 odd[] = { 0, 1, 2, 0, 2, 3 };
        const u16 even[] = { 3, 0, 1, 3, 1, 2 };
        const int N_SEQ = sizeof( odd ) / sizeof( *odd );

        u16* indices = (u16*)BX_MALLOC( bxDefaultAllocator(), numIndices * sizeof( u16 ), 2 );
                
        u16* iPtr = indices;
        int vertexOffset = 0;
        for( int z = 0; z < numQuadsInRow; ++z )
        {
            int sequenceIndex = z % 2;
            for( int x = 0; x < numQuadsInRow; ++x, ++sequenceIndex )
            {
                const u16* sequence = ( ( sequenceIndex % 2 ) == 0 ) ? odd : even;
                for( int s = 0; s < N_SEQ; ++s )
                {
                    iPtr[s] = vertexOffset + sequence[s];
                }

                vertexOffset += 4;
                iPtr += N_SEQ;
            }
        }

        SYS_ASSERT( (uptr)iPtr == (uptr)( indices + numIndices ) );
        SYS_ASSERT( vertexOffset == numVertices );

        ibuffer[0] = dev->createIndexBuffer( bxGdi::eTYPE_USHORT, numIndices, indices );

        BX_FREE0( bxDefaultAllocator(), indices );
    }

    void _TerrainMeshCreate( Terrain* terr, bxGdiDeviceBackend* dev, GfxScene* gfxScene, int subdiv )
    {
        int gridNumCells = radiusToDataLength( terr->_radius );
        GfxContext* gfx = gfxContextGet( gfxScene );
        bxGdiShaderFx_Instance* material = bx::gfxMaterialFind( "grey" );

        const Vector3 localAABBMin( -terr->_tileSize * 0.5f );
        const Vector3 localAABBMax(  terr->_tileSize * 0.5f );

        bxGdiVertexStreamDesc vstream;
        vstream.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 3, 0 );
        vstream.addBlock( bxGdi::eSLOT_NORMAL, bxGdi::eTYPE_FLOAT, 3, 1 );
        
        SYS_ASSERT( subdiv > 0 );
        const int numQuads = _ComputeNumQuads( subdiv );
        const int numVertices = numQuads * 4;

        for( int i = 0; i < gridNumCells; ++i )
        {
            bxGdiRenderSource* rsource = &terr->_renderSources[i];
            rsource->numVertexBuffers = 1;

            bxGdiVertexBuffer vbuffer = dev->createVertexBuffer( vstream, numVertices, nullptr );
            bxGdi::renderSource_setVertexBuffer( rsource, vbuffer, 0 );
            bxGdi::renderSource_setIndexBuffer( rsource, terr->_tileIndices );
            
            bx::GfxMeshInstance* meshInstance = nullptr;
            bx::gfxMeshInstanceCreate( &meshInstance, gfx );

            bx::GfxMeshInstanceData miData;
            miData.renderSourceSet( rsource );
            miData.fxInstanceSet( material );
            miData.locaAABBSet( localAABBMin, localAABBMax );

            bx::gfxMeshInstanceDataSet( meshInstance, miData );
            bx::gfxSceneMeshInstanceAdd( gfxScene, meshInstance );

            terr->_meshInstances[i] = meshInstance;
        }
    }
    void _TerrainMeshDestroy( Terrain* terr, bxGdiDeviceBackend* dev )
    {
        int gridNumCells = radiusToDataLength( terr->_radius );
        for( int i = 0; i < gridNumCells; ++i )
        {
            bx::gfxMeshInstanceDestroy( &terr->_meshInstances[i] );

            bxGdiRenderSource* rsource = &terr->_renderSources[i];
            bxGdi::renderSource_setIndexBuffer( rsource, bxGdiIndexBuffer() );
            bxGdi::renderSource_release( dev, rsource );
        }

        dev->releaseIndexBuffer( &terr->_tileIndices );
    }
    
    struct _TerrainVertex
    {
        float3_t pos;
        float3_t nrm;
    };

    static const Vector3 noiseOffset( 43.32214f, 5.3214325467f, 1.0412312f );
    static const float noiseFreq = 0.05f;

    inline float _TerrainNoise( const Vector3& vertex, float height, const Vector3& offset, float freq )
    {
        SSEScalar s( ( vertex * freq + noiseOffset ).get128() );
        float nx = s.x;
        float ny = s.x + s.z + s.y;
        float nz = s.z;
        float y = bxNoise_perlin( nx, ny, nz, 16, 64, 8 );
        return y * height;
    }

    void _FindQuad( const Terrain* terr, const Vector3& wpos, int tileIndex )
    {
        const int numQuadsInRow = _ComputeNumQuadsInRow( terr->_tileSubdiv );
        const int numSamplesInRow = numQuadsInRow + 1;
        const Vector3 tileCenterPos = toVector3( terr->_cellWorldCoords[tileIndex] );
        const float tileExt = terr->_tileSize * 0.5f;
        const float quadSize = terr->_tileSize / (float)numQuadsInRow;
        const float quadExt = quadSize * 0.5f;

        const Vector3 beginTileLocal = Vector3( tileExt, 0.f, -tileExt );
        const Vector3 beginTileWorld = tileCenterPos + beginTileLocal;
        const Vector3 posInTile = wpos - beginTileWorld;
        Vector3 quadCoordV = posInTile / quadSize;
        quadCoordV = (Vector3( _mm_round_ps( quadCoordV.get128(), _MM_FROUND_TRUNC ) ));
        SSEScalar quadCoord( _mm_cvtps_epi32( ( quadCoordV ).get128() ) );
        quadCoord.ix *= -1; // switch to left handed

        const int heightSampleIndex0 = quadCoord.iz * numSamplesInRow + quadCoord.ix;
        const int heightSampleIndex1 = heightSampleIndex0 + 1;
        const int heightSampleIndex2 = (quadCoord.iz + 1) * numSamplesInRow + ( quadCoord.ix + 1 );
        const int heightSampleIndex3 = (quadCoord.iz + 1) * numSamplesInRow + quadCoord.ix;
        SYS_ASSERT( heightSampleIndex0 < numSamplesInRow*numSamplesInRow );
        SYS_ASSERT( heightSampleIndex1 < numSamplesInRow*numSamplesInRow );
        SYS_ASSERT( heightSampleIndex2 < numSamplesInRow*numSamplesInRow );
        SYS_ASSERT( heightSampleIndex3 < numSamplesInRow*numSamplesInRow );
        
        const float* heightSamples = _GetCellHeightSamples( terr, tileIndex );
        
        const Vector3 localXoffset = Vector3( quadSize, 0.f, 0.f );
        const Vector3 localZoffset = Vector3( 0.f, 0.f, quadSize );

        Vector3 samplePosLocal0 = quadCoordV * quadSize;
        Vector3 samplePosLocal1 = samplePosLocal0 - localXoffset;
        Vector3 samplePosLocal2 = samplePosLocal1 + localZoffset;
        Vector3 samplePosLocal3 = samplePosLocal0 + localZoffset;
        
        samplePosLocal0.setY( heightSamples[heightSampleIndex0] );
        samplePosLocal1.setY( heightSamples[heightSampleIndex1] );
        samplePosLocal2.setY( heightSamples[heightSampleIndex2] );
        samplePosLocal3.setY( heightSamples[heightSampleIndex3] );

        bxGfxDebugDraw::addSphere( Vector4( beginTileWorld, 0.2f ), 0xFF0000FF, 1 );
        bxGfxDebugDraw::addSphere( Vector4( samplePosLocal0 + beginTileWorld, 0.1f ), 0xFFFF00FF, 1 );
        bxGfxDebugDraw::addSphere( Vector4( samplePosLocal1 + beginTileWorld, 0.1f ), 0xFFFF00FF, 1 );
        bxGfxDebugDraw::addSphere( Vector4( samplePosLocal2 + beginTileWorld, 0.1f ), 0xFFFF00FF, 1 );
        bxGfxDebugDraw::addSphere( Vector4( samplePosLocal3 + beginTileWorld, 0.1f ), 0xFFFF00FF, 1 );

    }

    void _TerrainMeshTileVerticesCompute( Terrain* terr, int tileIndex, bxGdiContextBackend* gdi )
    {
        const int numQuadsInRow = _ComputeNumQuadsInRow( terr->_tileSubdiv );
        const Vector3 tileCenterPos = toVector3( terr->_cellWorldCoords[tileIndex] );
        const float tileExt = terr->_tileSize * 0.5f;
        const float quadSize = terr->_tileSize / (float)numQuadsInRow;
        const float quadExt = quadSize * 0.5f;
        
        const Vector3 beginVertex = Vector3( tileExt, 0.f, -tileExt );
        const Vector3 localXoffset = Vector3( quadSize, 0.f, 0.f );
        const Vector3 localZoffset = Vector3( 0.f, 0.f, quadSize );

        const bxGdiVertexBuffer& tileVbuffer = terr->_renderSources[tileIndex].vertexBuffers[0];

        float* heightSamples = _GetCellHeightSamples( terr, tileIndex );

        _TerrainVertex* vertices = (_TerrainVertex*)gdi->mapVertices( tileVbuffer, 0, tileVbuffer.numElements, bxGdi::eMAP_WRITE );
        __m128* vtxPtr128 = (__m128*)vertices;

        //const float phase = PI2 / (float)(numQuadsInRow - 1);
        const int numSamplesInRow = numQuadsInRow + 1;
        const float height = 5.f;

        for ( int iz = 0; iz <= numQuadsInRow; ++iz )
        {
            const float z = iz * quadSize;
            //const float c = cos( iz * phase );
            for ( int ix = 0; ix <= numQuadsInRow; ++ix )
            {
                const float x = ix * quadSize;
                //const float y = c * sin( phase * ix );
                Vector3 v0 = beginVertex + Vector3( -x, 0.f, z );
                const float h = height;
                const float h0 = _TerrainNoise( tileCenterPos + v0, h, noiseOffset, noiseFreq );

                const int sampleIndex0 = iz * numSamplesInRow + ix;
                SYS_ASSERT( sampleIndex0 < numSamplesInRow*numSamplesInRow );

                heightSamples[sampleIndex0] = h0;
            }
        }

        for( int iz = 0; iz < numQuadsInRow; ++iz )
        {
            const float z = iz * quadSize;
            //const float c = cos( iz * phase );
            for( int ix = 0; ix < numQuadsInRow; ++ix )
            {
                const float x = ix * quadSize;
                //const float y = c * sin( phase * ix );
                Vector3 v0 = beginVertex + Vector3( -x, 0.f, z );
                Vector3 v1 = v0 - localXoffset;
                Vector3 v2 = v1 + localZoffset;
                Vector3 v3 = v0 + localZoffset;
                
                //const float h = height;
                //const float h0 = _TerrainNoise( tileCenterPos + v0, h, noiseOffset, noiseFreq );
                //const float h1 = _TerrainNoise( tileCenterPos + v1, h, noiseOffset, noiseFreq );
                //const float h2 = _TerrainNoise( tileCenterPos + v2, h, noiseOffset, noiseFreq );
                //const float h3 = _TerrainNoise( tileCenterPos + v3, h, noiseOffset, noiseFreq );

                const int sampleIndex0 = iz * numSamplesInRow + ix;
                const int sampleIndex1 = iz * numSamplesInRow + (ix + 1);
                const int sampleIndex2 = (iz + 1) * numSamplesInRow + (ix + 1);
                const int sampleIndex3 = (iz + 1) * numSamplesInRow + ix;
                
                SYS_ASSERT( sampleIndex0 < numSamplesInRow*numSamplesInRow );
                SYS_ASSERT( sampleIndex1 < numSamplesInRow*numSamplesInRow );
                SYS_ASSERT( sampleIndex2 < numSamplesInRow*numSamplesInRow );
                SYS_ASSERT( sampleIndex3 < numSamplesInRow*numSamplesInRow );

                const float h0 = heightSamples[sampleIndex0];
                const float h1 = heightSamples[sampleIndex1];
                const float h2 = heightSamples[sampleIndex2];
                const float h3 = heightSamples[sampleIndex3];

                //heightSamples[sampleIndex0] = h0;
                //heightSamples[sampleIndex1] = h1;
                //heightSamples[sampleIndex2] = h2;
                //heightSamples[sampleIndex3] = h3;

                v0.setY( h0 );
                v1.setY( h1 );
                v2.setY( h2 );
                v3.setY( h3 );

                const Vector3 e01 = v1 - v0;
                const Vector3 e03 = v3 - v0;
                const Vector3 e12 = v2 - v1;
                const Vector3 e32 = v2 - v3;

                const Vector3 n0 = normalize( cross( e01, e03 ) );
                const Vector3 n1 = normalize( cross( e01, e12 ) );
                const Vector3 n2 = normalize( cross( e32, e12 ) );
                const Vector3 n3 = normalize( cross( e32, e03 ) );

                storeXYZArray( v0, n0, v1, n1, vtxPtr128 );
                vtxPtr128 += 3;

                storeXYZArray( v2, n2, v3, n3, vtxPtr128 );
                vtxPtr128 += 3;
            }
        }
        gdi->unmapVertices( tileVbuffer );

        SYS_ASSERT( (uptr)(vtxPtr128) == (uptr)(vertices + tileVbuffer.numElements) );
    }

    void _TerrainCreatePhysics( Terrain* terr, PhxScene* phxScene )
    {
        int gridNumCells = radiusToDataLength( terr->_radius );

        PhxContext* phx = phxContextGet( phxScene );

        const PhxGeometry geom( terr->_tileSize * 0.5f, 0.1f, terr->_tileSize * 0.5f );
        const Matrix4 pose = Matrix4::identity();

        for ( int i = 0; i < gridNumCells; ++i )
        {
            PhxActor* actor = nullptr;
            bool bres = phxActorCreateDynamic( &actor, phx, pose, geom, -1.f );

            SYS_ASSERT( bres );
            SYS_ASSERT( terr->_phxActors[i] == nullptr );

            terr->_phxActors[i] = actor;
        }

        phxSceneActorAdd( phxScene, terr->_phxActors, gridNumCells );
    }
    void _TerrainDestroyPhysics( Terrain* terr, PhxScene* phxScene )
    {
        (void)phxScene;

        int gridNumCells = radiusToDataLength( terr->_radius );
        for ( int i = 0; i < gridNumCells; ++i )
        {
            phxActorDestroy( &terr->_phxActors[i] );
        }
    }

    void terrainCreate( Terrain** terr, GameScene* gameScene, bxEngine* engine )
    {
        Terrain* t = BX_NEW( bxDefaultAllocator(), Terrain );

        const int numCells = radiusToLength( t->_radius );
        const int gridCellCount = numCells * numCells;
        
        const int numQuadsInRow = _ComputeNumQuadsInRow( t->_tileSubdiv );
        const int numSamplesInRow = numQuadsInRow + 1;
        const int numSamplesInCell = numSamplesInRow * numSamplesInRow;
        
        int memSize = 0;
        memSize += gridCellCount * sizeof( *t->_meshInstances );
        memSize += gridCellCount * sizeof( *t->_phxActors );
        memSize += gridCellCount * sizeof( *t->_renderSources );
        memSize += gridCellCount * sizeof( *t->_cellWorldCoords );
        memSize += numSamplesInCell * gridCellCount * sizeof( *t->_cellHeightSamples );
        memSize += gridCellCount * sizeof( *t->_cellFlags );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        t->_memoryHandle = mem;

        bxBufferChunker chunker( mem, memSize );
        t->_meshInstances = chunker.add< bx::GfxMeshInstance* >( gridCellCount );
        t->_phxActors = chunker.add< bx::PhxActor* >( gridCellCount );
        t->_renderSources = chunker.add< bxGdiRenderSource >( gridCellCount );
        t->_cellWorldCoords = chunker.add< i32x3 >( gridCellCount );
        t->_cellHeightSamples = chunker.add< f32 >( numSamplesInCell * gridCellCount );
        t->_cellFlags = chunker.add< u8 >( gridCellCount );
        chunker.check();

        _TerrainCreatePhysics( t, gameScene->phxScene );

        const int SUBDIV = t->_tileSubdiv;
        _TerrainMeshCreateIndices( &t->_tileIndices, engine->gdiDevice, SUBDIV );
        _TerrainMeshCreate( t, engine->gdiDevice, gameScene->gfxScene, SUBDIV );

        terr[0] = t;
    }

    void terrainDestroy( Terrain** terr, GameScene* gameScene, bxEngine* engine )
    {
        if ( !terr[0] )
            return;

        _TerrainMeshDestroy( terr[0], engine->gdiDevice );
        _TerrainDestroyPhysics( terr[0], gameScene->phxScene );
        BX_FREE0( bxDefaultAllocator(), terr[0]->_memoryHandle );
        BX_FREE0( bxDefaultAllocator(), terr[0] );
    }

    void computeGridPositions( Vector3* posWorldRounded, __m128i* posGrid, const Vector3& inPos, const Vector3& upVector, float cellSize )
    {
        Vector3 positionOnPlane = projectPointOnPlane( inPos, Vector4( upVector, zeroVec ) );
        Vector3 positionWorldRounded = positionOnPlane / cellSize;

        const __m128i rounded = _mm_cvtps_epi32( _mm_round_ps( positionWorldRounded.get128(), _MM_FROUND_NINT ) );
        positionWorldRounded = Vector3( _mm_cvtepi32_ps( rounded ) ) * cellSize;

        posWorldRounded[0] = positionWorldRounded;
        posGrid[0] = rounded;
    }

    void terrainComputeTileRange( int* begin, int* end, u32* coord, int delta, int gridRadius, int gap, u32 maxCoordIndex )
    {
        if( delta > 0 )
        {
            end[0] = gridRadius + 1;
            begin[0] = end[0] - delta;

            /// compute column index in data array
            for( int i = 0; i < gap; ++i )
                coord[0] = wrap_inc_u32( coord[0], 0, maxCoordIndex );
        }
        else
        {
            begin[0] = -gridRadius;
            end[0] = begin[0] - delta;

            /// compute column index in data array
            for( int i = 0; i < gap; ++i )
                coord[0] = wrap_dec_u32( coord[0], 0, maxCoordIndex );
        }
    }

    

    void terrainTick( Terrain* terr, GameScene* gameScene, bxGdiContextBackend* gdi, float deltaTime )
    {
        const Vector3 playerPosition = bx::characterPoseGet( gameScene->character ).getTranslation();
        Vector3 currPosWorldRounded, prevPosWorldRounded;
        __m128i currPosWorldGrid, prevPosWorldGrid;

        computeGridPositions( &currPosWorldRounded, &currPosWorldGrid, playerPosition, Vector3::yAxis(), terr->_tileSize );
        computeGridPositions( &prevPosWorldRounded, &prevPosWorldGrid, terr->_prevPlayerPosition, Vector3::yAxis(), terr->_tileSize );

        const SSEScalar gridCoords0( prevPosWorldGrid );
        const SSEScalar gridCoords1( currPosWorldGrid );

        const int gridCoordsDx = gridCoords1.ix - gridCoords0.ix;
        const int gridCoordsDz = gridCoords1.iz - gridCoords0.iz;

        int gridRadius = terr->_radius;
        int gridWH = radiusToLength( gridRadius );
        int gridNumCells = radiusToDataLength( gridRadius );
        if( ::abs( gridCoordsDx ) >= gridWH || ::abs( gridCoordsDz ) >= gridWH )
        {
            terr->_centerGridSpaceX = terr->_radius;
            terr->_centerGridSpaceY = terr->_radius;
            
            const int cellSize = (int)terr->_tileSize;
            
            for( int iz = -gridRadius; iz <= gridRadius; ++iz )
            {
                int coordZ = gridCoords1.iz + iz;
                int coordGridSpaceZ = coordZ + terr->_radius;

                for( int ix = -gridRadius; ix <= gridRadius; ++ix )
                {
                    int coordX = gridCoords1.ix + ix;
                    int coordGridSpaceX = coordX + terr->_radius;

                    i32x3 worldSpaceCoords( coordX * cellSize, 0, coordZ * cellSize );
                    
                    int dataIndex = xyToIndex( coordGridSpaceX, coordGridSpaceZ, gridWH );
                    SYS_ASSERT( dataIndex < gridNumCells );
                    
                    terr->_cellWorldCoords[dataIndex] = worldSpaceCoords;
                    terr->_cellFlags[dataIndex] |= ECellFlag::NEW;
                }
            }
        }
        else
        {
            u32 localGridX = terr->_centerGridSpaceX;
            u32 localGridZ = terr->_centerGridSpaceY;

            if( gridCoordsDx > 0 )
            {
                for ( int i = 0; i < gridCoordsDx; ++i )
                    localGridX = wrap_inc_u32( localGridX, 0, gridWH - 1 );
            }
            else if( gridCoordsDx < 0 )
            {
                for ( int i = 0; i < ::abs( gridCoordsDx ); ++i )
                    localGridX = wrap_dec_u32( localGridX, 0, gridWH - 1 );
            }

            if ( gridCoordsDz > 0 )
            {
                for ( int i = 0; i < gridCoordsDz; ++i )
                    localGridZ = wrap_inc_u32( localGridZ, 0, gridWH - 1 );
            }
            else if ( gridCoordsDz < 0 )
            {
                for ( int i = 0; i < ::abs( gridCoordsDz ); ++i )
                    localGridZ = wrap_dec_u32( localGridZ, 0, gridWH - 1 );
            }
        
            terr->_centerGridSpaceX = localGridX;
            terr->_centerGridSpaceY = localGridZ;
        
            const int cellSize = (int)terr->_tileSize;

            if( ::abs( gridCoordsDx ) > 0 )
            {
                SYS_ASSERT( ::abs( gridCoordsDx ) < gridRadius );
                                
                const int colGap = gridRadius - ::abs( gridCoordsDx );
                u32 col = localGridX;
                u32 rowBegin = localGridZ;
                for ( int i = 0; i < gridRadius; ++i )
                    rowBegin = wrap_dec_u32( rowBegin, 0, gridWH - 1 );

                int endCol, beginCol;
                terrainComputeTileRange( &beginCol, &endCol, &col, gridCoordsDx, gridRadius, colGap, gridWH - 1 );

                for ( int ix = beginCol; ix < endCol; ++ix )
                {
                    col = (gridCoordsDx > 0) ? wrap_inc_u32( col, 0, gridWH - 1 ) : wrap_dec_u32( col, 0, gridWH - 1 );
                    u32 row = rowBegin;
                    for ( int iz = -gridRadius; iz <= gridRadius; ++iz )
                    {
                        int x = gridCoords1.ix + ix;
                        int z = gridCoords1.iz + iz;

                        i32x3 worldSpaceCoords( x * cellSize, 0, z * cellSize );

                        int dataIndex = xyToIndex( col, row, gridWH );
                        SYS_ASSERT( dataIndex < gridNumCells );
                        terr->_cellWorldCoords[dataIndex] = worldSpaceCoords;

                        terr->_cellFlags[dataIndex] |= ECellFlag::NEW;

                        row = wrap_inc_u32( row, 0, gridWH - 1 );
                    }

                    
                }
            }

            if( ::abs( gridCoordsDz ) > 0 )
            {
                SYS_ASSERT( ::abs( gridCoordsDz ) < gridRadius );
                const int gap = gridRadius - ::abs( gridCoordsDz );
                u32 row = localGridZ;
                u32 colBegin = localGridX;
                for( int i = 0; i < gridRadius; ++i )
                    colBegin = wrap_dec_u32( colBegin, 0, gridWH - 1 );

                int beginRow, endRow;
                terrainComputeTileRange( &beginRow, &endRow, &row, gridCoordsDz, gridRadius, gap, gridWH - 1 );

                for( int iz = beginRow; iz < endRow; ++iz )
                {
                    row = ( gridCoordsDz > 0 ) ? wrap_inc_u32( row, 0, gridWH - 1 ) : wrap_dec_u32( row, 0, gridWH - 1 );
                    u32 col = colBegin;
                    for( int ix = -gridRadius; ix <= gridRadius; ++ix )
                    {
                        int x = gridCoords1.ix + ix;
                        int z = gridCoords1.iz + iz;

                        i32x3 worldSpaceCoords( x * cellSize, 0, z * cellSize );

                        int dataIndex = xyToIndex( col, row, gridWH );
                        SYS_ASSERT( dataIndex < gridNumCells );
                        terr->_cellWorldCoords[dataIndex] = worldSpaceCoords;

                        terr->_cellFlags[dataIndex] |= ECellFlag::NEW;
                        col = wrap_inc_u32( col, 0, gridWH - 1 );
                    }
                }
            }
        }

        //bxGfxDebugDraw::addBox( Matrix4::translation( currPosWorldRounded), Vector3( terr->_tileSize * 0.5f ), 0xFF00FF00, 1 );




        for( int i = 0; i < gridNumCells; ++i )
        {
            i32x3 ipos = terr->_cellWorldCoords[i];
            Vector3 pos = toVector3( ipos );

            u32 color = 0xff00ff00;
            if( terr->_cellFlags[i] & ECellFlag::NEW )
            {
                const Matrix4 tilePose = Matrix4::translation( pos );
                phxActorPoseSet( terr->_phxActors[i], tilePose, gameScene->phxScene );
                gfxMeshInstanceWorldMatrixSet( terr->_meshInstances[i], &tilePose, 1 );
                color = 0xffff0000;
                _TerrainMeshTileVerticesCompute( terr, i, gdi );
            }
            
            const Vector3 ext( terr->_tileSize * 0.5f, 0.1f, terr->_tileSize * 0.5f );

            //bxGfxDebugDraw::addBox( Matrix4::translation( pos ), ext, color, 1 );

            terr->_cellFlags[i] &= ~( ECellFlag::NEW );
        }
        terr->_prevPlayerPosition = playerPosition;

        const int centerTileIndex = terr->_centerGridSpaceY * radiusToLength( terr->_radius ) + terr->_centerGridSpaceX;
        _FindQuad( terr, playerPosition, centerTileIndex );

        i32x3 ipos = terr->_cellWorldCoords[centerTileIndex];
        Vector3 pos = toVector3( ipos );
        const Vector3 ext( terr->_tileSize * 0.5f, 0.1f, terr->_tileSize * 0.5f );
        bxGfxDebugDraw::addBox( Matrix4::translation( pos ), ext, 0xFFFF0000, 1 );

        if ( ImGui::Begin( "terrain" ) )
        {
            ImGui::Text( "playerPosition: %.3f, %.3f, %.3f\nplayerPositionGrid: %.3f, %.3f, %.3f\ngridCoords: %d, %d, %d",
                         playerPosition.getX().getAsFloat(), playerPosition.getY().getAsFloat(), playerPosition.getZ().getAsFloat(),
                         currPosWorldRounded.getX().getAsFloat(), currPosWorldRounded.getY().getAsFloat(), currPosWorldRounded.getZ().getAsFloat(),
                         gridCoords1.ix, gridCoords1.iy, gridCoords1.iz );
            ImGui::Text( "localXY: %d, %d", terr->_centerGridSpaceX, terr->_centerGridSpaceY );
        }
        ImGui::End();
    }

}///