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
        f32 _tileSize = 10.f;
        i32 _radius = 2;
        
        u32 _centerGridSpaceX = _radius;
        u32 _centerGridSpaceY = _radius;

        bxGdiIndexBuffer _tileIndices;

        //bxGrid _grid;
        void* _memoryHandle = nullptr;
        GfxMeshInstance** _meshInstances = nullptr;
        PhxActor** _phxActors = nullptr;
        bxGdiRenderSource* _renderSources = nullptr;
        i32x3* _cellWorldCoords = nullptr;
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
                const u16* sequence = ( sequenceIndex == 0 ) ? odd : even;
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
        bxGdiShaderFx_Instance* material = bx::gfxMaterialFind( "white" );

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

        int numCells = radiusToLength( t->_radius );
        int gridCellCount = numCells * numCells;
        
        int memSize = 0;
        memSize += gridCellCount * sizeof( *t->_meshInstances );
        memSize += gridCellCount * sizeof( *t->_phxActors );
        memSize += gridCellCount * sizeof( *t->_renderSources );
        memSize += gridCellCount * sizeof( *t->_cellWorldCoords );
        memSize += gridCellCount * sizeof( *t->_cellFlags );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        t->_memoryHandle = mem;

        bxBufferChunker chunker( mem, memSize );
        t->_meshInstances = chunker.add< bx::GfxMeshInstance* >( gridCellCount );
        t->_phxActors = chunker.add< bx::PhxActor* >( gridCellCount );
        t->_renderSources = chunker.add< bxGdiRenderSource >( gridCellCount );
        t->_cellWorldCoords = chunker.add< i32x3 >( gridCellCount );
        t->_cellFlags = chunker.add< u8 >( gridCellCount );
        chunker.check();

        _TerrainCreatePhysics( t, gameScene->phxScene );

        const int SUBDIV = 2;
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

    

    void terrainTick( Terrain* terr, GameScene* gameScene, float deltaTime )
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

        if( ImGui::Begin( "terrain" ) )
        {
            ImGui::Text( "playerPosition: %.3f, %.3f, %.3f\nplayerPositionGrid: %.3f, %.3f, %.3f\ngridCoords: %d, %d, %d", 
                         playerPosition.getX().getAsFloat(), playerPosition.getY().getAsFloat(), playerPosition.getZ().getAsFloat(),
                         currPosWorldRounded.getX().getAsFloat(), currPosWorldRounded.getY().getAsFloat(), currPosWorldRounded.getZ().getAsFloat(),
                         gridCoords1.ix, gridCoords1.iy, gridCoords1.iz );
            ImGui::Text( "localXY: %d, %d", terr->_centerGridSpaceX, terr->_centerGridSpaceY );
        }
        ImGui::End();

        for( int i = 0; i < gridNumCells; ++i )
        {
            i32x3 ipos = terr->_cellWorldCoords[i];
            Vector3 pos = toVector3( ipos );

            u32 color = 0xff00ff00;
            if( terr->_cellFlags[i] & ECellFlag::NEW )
            {
                phxActorPoseSet( terr->_phxActors[i], Matrix4::translation( pos ), gameScene->phxScene );
                color = 0xffff0000;
            }
            
            const Vector3 ext( terr->_tileSize * 0.5f, 0.1f, terr->_tileSize * 0.5f );

            bxGfxDebugDraw::addBox( Matrix4::translation( pos ), ext, color, 1 );



            terr->_cellFlags[i] &= ~( ECellFlag::NEW );
        }

        terr->_prevPlayerPosition = playerPosition;
    }

}///