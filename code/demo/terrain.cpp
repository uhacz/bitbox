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

namespace bx
{
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

    struct Terrain
    {
        Vector3 _prevPlayerPosition = Vector3( 0.f );
        f32 _tileSize = 2.f;
        i32 _radius = 10;
        
        i32 _centerX = 0;
        i32 _centerY = 0;

        bxGrid _grid;

        u8*    _cellFlags = nullptr;
        i32x3* _cellWorldCoords = nullptr;
        bx::PhxActor** _cellActors = nullptr;
        
    };
    

    void terrainCreate( Terrain** terr )
    {
        Terrain* t = BX_NEW( bxDefaultAllocator(), Terrain );

        float radiusf = (float)t->_radius;
        float numCells = ::ceil( 2.f * radiusf / t->_tileSize );
        int numCellsi = (int)numCells;
        t->_grid = bxGrid( numCellsi, numCellsi, numCellsi );

        int gridCellCount = t->_grid.numCells();
        int memSize = 0;
        memSize += gridCellCount * sizeof( *t->_cellFlags );
        memSize += gridCellCount * sizeof( *t->_cellWorldCoords );
        memSize += gridCellCount * sizeof( *t->_cellActors );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );
        t->_cellFlags = chunker.add< u8 >( gridCellCount );
        t->_cellWorldCoords = chunker.add< i32x3 >( gridCellCount );
        t->_cellActors = chunker.add< bx::PhxActor* >( gridCellCount );
        chunker.check();

        terr[0] = t;
    }

    void terrainDestroy( Terrain** terr )
    {
        if ( !terr[0] )
            return;

        BX_FREE0( bxDefaultAllocator(), terr[0]->_cellFlags );

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

        computeGridPositions( &currPosWorldRounded, &currPosWorldGrid, playerPosition, terr->_tileSize );
        computeGridPositions( &prevPosWorldRounded, &prevPosWorldGrid, terr->_prevPlayerPosition, terr->_tileSize );

        const SSEScalar gridCoords0( prevPosWorldGrid );
        const SSEScalar gridCoords1( currPosWorldGrid );

        const int gridCoordsDx = currPosWorldGrid.ix - prevPosWorldGrid.ix;
        const int gridCoordsDz = currPosWorldGrid.iz - prevPosWorldGrid.iz;

        if( gridCoordsDx > 0 )
        {
            while( 
        }

        int localGridX = prevPosWorldGrid.ix;
        int localGridZ = prevPosWorldGrid.iz;
        

        bxGfxDebugDraw::addBox( Matrix4::translation( currPosWorldRounded), Vector3( terr->_tileSize * 0.5f ), 0xFF00FF00, 1 );

        if( ImGui::Begin( "terrain" ) )
        {
            ImGui::Text( "playerPosition: %.3f, %.3f, %.3f\nplayerPositionGrid: %.3f, %.3f, %.3f\n gridCoords: %d, %d, %d", 
                         playerPosition.getX().getAsFloat(), playerPosition.getY().getAsFloat(), playerPosition.getZ().getAsFloat(),
                         currPosWorldRounded.getX().getAsFloat(), currPosWorldRounded.getY().getAsFloat(), currPosWorldRounded.getZ().getAsFloat(),
                         gridCoords1.ix, gridCoords1.iy, gridCoords1.iz );
        }
        ImGui::End();


        terr->_prevPlayerPosition = playerPosition;
    }

}///