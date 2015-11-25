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


    struct Terrain
    {
        Vector3 _prevPlayerPosition = Vector3( -10000.f );
        f32 _tileSize = 2.f;
        i32 _radius = 2;
        
        u32 _centerGridSpaceX = _radius;
        u32 _centerGridSpaceY = _radius;

        //bxGrid _grid;

        bx::PhxActor** _cellActors = nullptr;
        i32x3* _cellWorldCoords = nullptr;
        u8*    _cellFlags = nullptr;
    };
    
    inline int radiusToLength( int rad ) { return rad * 2 + 1; }
    inline int xyToIndex( int x, int y, int w ) { return y * w + x; }
    inline i32x2 indexToXY( int index, int w ) { return i32x2( index % w, index / w ); }

    void terrainCreate( Terrain** terr )
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

        terr[0] = t;
    }

    void terrainDestroy( Terrain** terr )
    {
        if ( !terr[0] )
            return;

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

    void terrainTick( Terrain* terr, const Vector3& playerPosition, float deltaTime )
    {
        Vector3 currPosWorldRounded, prevPosWorldRounded;
        __m128i currPosWorldGrid, prevPosWorldGrid;

        computeGridPositions( &currPosWorldRounded, &currPosWorldGrid, playerPosition, Vector3::yAxis(), terr->_tileSize );
        computeGridPositions( &prevPosWorldRounded, &prevPosWorldGrid, terr->_prevPlayerPosition, Vector3::yAxis(), terr->_tileSize );

        const SSEScalar gridCoords0( prevPosWorldGrid );
        const SSEScalar gridCoords1( currPosWorldGrid );

        const int gridCoordsDx = gridCoords0.ix - gridCoords1.ix;
        const int gridCoordsDz = gridCoords0.iz - gridCoords1.iz;

        int gridRadius = terr->_radius;
        int gridWH = radiusToLength( gridRadius );
        int gridNumCells = gridWH * gridWH;
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
                }
            }
        }
        else
        {
            int localGridX = terr->_centerGridSpaceX;
            int localGridZ = terr->_centerGridSpaceY;

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
        
            if( ::abs( gridCoordsDx ) > 0 )
            {
                int endCol = gridCoords1.ix + gridRadius;
                int beginCol = endCol - gridCoordsDx;
                for( int ic = beginCol; ic <= endCol; ++ic )
                {
                    aa
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

            bxGfxDebugDraw::addBox( Matrix4::translation( pos ), Vector3( terr->_tileSize * 0.5f ), 0xFF00FF00, 1 );
        }

        terr->_prevPlayerPosition = playerPosition;
    }

}///