#include "terrain.h"

#include <util/type.h>
#include <util/memory.h>
#include <util/debug.h>
#include <util/grid.h>
#include <util/buffer_utils.h>

#include <smmintrin.h>

#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>
#include <phx/phx.h>

#include <cmath>
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

        //bxGrid _grid;

        bx::PhxActor** _cellActors = nullptr;
        i32x3* _cellWorldCoords = nullptr;
        u8*    _cellFlags = nullptr;
    };
    
    inline int radiusToLength( int rad ) { return rad * 2 + 1; }
    inline int radiusToDataLength( int rad ) { int a = radiusToLength( rad ); return a*a; }
    inline int xyToIndex( int x, int y, int w ) { return y * w + x; }
    inline i32x2 indexToXY( int index, int w ) { return i32x2( index % w, index / w ); }

    void _TerrainCreatePhysics( Terrain* terr, PhxScene* phxScene )
    {
        int gridNumCells = radiusToDataLength( terr->_radius );

        PhxContext* phx = phxSceneContextGet( phxScene );

        const PhxGeometry geom( terr->_tileSize * 0.5f, 0.1f, terr->_tileSize * 0.5f );
        const Matrix4 pose = Matrix4::identity();

        for ( int i = 0; i < gridNumCells; ++i )
        {
            PhxActor* actor = nullptr;
            bool bres = phxActorCreateDynamic( &actor, phx, pose, geom, -1.f );

            SYS_ASSERT( bres );
            SYS_ASSERT( terr->_cellActors[i] == nullptr );

            terr->_cellActors[i] = actor;
        }

        phxSceneActorAdd( phxScene, terr->_cellActors, gridNumCells );
    }
    void _TerrainDestroyPhysics( Terrain* terr, PhxScene* phxScene )
    {
        (void)phxScene;

        int gridNumCells = radiusToDataLength( terr->_radius );
        for ( int i = 0; i < gridNumCells; ++i )
        {
            phxActorDestroy( &terr->_cellActors[i] );
        }
    }

    void terrainCreate( Terrain** terr, GameScene* gameScene )
    {
        Terrain* t = BX_NEW( bxDefaultAllocator(), Terrain );

        int numCells = radiusToLength( t->_radius );
        int gridCellCount = numCells * numCells;
        
        int memSize = 0;
        memSize += gridCellCount * sizeof( *t->_cellFlags );
        memSize += gridCellCount * sizeof( *t->_cellWorldCoords );
        memSize += gridCellCount * sizeof( *t->_cellActors );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );
        t->_cellActors = chunker.add< bx::PhxActor* >( gridCellCount );
        t->_cellWorldCoords = chunker.add< i32x3 >( gridCellCount );
        t->_cellFlags = chunker.add< u8 >( gridCellCount );
        chunker.check();

        _TerrainCreatePhysics( t, gameScene->phxScene );

        terr[0] = t;
    }

    void terrainDestroy( Terrain** terr, GameScene* gameScene )
    {
        if ( !terr[0] )
            return;

        _TerrainDestroyPhysics( terr[0], gameScene->phxScene );

        BX_FREE0( bxDefaultAllocator(), terr[0]->_cellActors );

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
                phxActorPoseSet( terr->_cellActors[i], Matrix4::translation( pos ), gameScene->phxScene );
                color = 0xffff0000;
            }
            
            const Vector3 ext( terr->_tileSize * 0.5f, 0.1f, terr->_tileSize * 0.5f );

            bxGfxDebugDraw::addBox( Matrix4::translation( pos ), ext, color, 1 );



            terr->_cellFlags[i] &= ~( ECellFlag::NEW );
        }

        terr->_prevPlayerPosition = playerPosition;
    }

}///