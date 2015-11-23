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
        i32 _radius = 1;
        
        i32 _centerX = 0;
        i32 _centerY = 0;

        //bxGrid _grid;

        bx::PhxActor** _cellActors = nullptr;
        i32x3* _cellWorldCoords = nullptr;
        u8*    _cellFlags = nullptr;
        
    };
    

    void terrainCreate( Terrain** terr )
    {
        Terrain* t = BX_NEW( bxDefaultAllocator(), Terrain );

        float radiusf = (float)t->_radius;
        float numCells = ::ceil( ( 2 * radiusf ) + 1 );
        int numCellsi = (int)numCells;

        int gridCellCount = numCellsi * numCellsi;
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

        int localGridX = terr->_centerX;
        int localGridZ = terr->_centerY;

        if( gridCoordsDx > 0 )
        {
            for ( int i = 0; i < gridCoordsDx; ++i )
                localGridX = wrap_inc_i32( localGridX, -terr->_radius, terr->_radius );
        }
        else if( gridCoordsDx < 0 )
        {
            for ( int i = 0; i < ::abs( gridCoordsDx ); ++i )
                localGridX = wrap_dec_i32( localGridX, -terr->_radius, terr->_radius );
        }

        if ( gridCoordsDz > 0 )
        {
            for ( int i = 0; i < gridCoordsDz; ++i )
                localGridZ = wrap_inc_i32( localGridZ, -terr->_radius, terr->_radius );
        }
        else if ( gridCoordsDz < 0 )
        {
            for ( int i = 0; i < ::abs( gridCoordsDz ); ++i )
                localGridZ = wrap_dec_i32( localGridZ, -terr->_radius, terr->_radius );
        }
        
        terr->_centerX = localGridX;
        terr->_centerY = localGridZ;

        bxGfxDebugDraw::addBox( Matrix4::translation( currPosWorldRounded), Vector3( terr->_tileSize * 0.5f ), 0xFF00FF00, 1 );

        if( ImGui::Begin( "terrain" ) )
        {
            ImGui::Text( "playerPosition: %.3f, %.3f, %.3f\nplayerPositionGrid: %.3f, %.3f, %.3f\ngridCoords: %d, %d, %d", 
                         playerPosition.getX().getAsFloat(), playerPosition.getY().getAsFloat(), playerPosition.getZ().getAsFloat(),
                         currPosWorldRounded.getX().getAsFloat(), currPosWorldRounded.getY().getAsFloat(), currPosWorldRounded.getZ().getAsFloat(),
                         gridCoords1.ix, gridCoords1.iy, gridCoords1.iz );
            ImGui::Text( "localXY: %d, %d", terr->_centerX, terr->_centerY );
        }
        ImGui::End();


        terr->_prevPlayerPosition = playerPosition;
    }

}///