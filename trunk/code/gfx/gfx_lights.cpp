#include "gfx_lights.h"
#include "gfx_camera.h"
#include "gfx.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include "gfx_debug_draw.h"

bxGfxLights::bxGfxLights()
    : _memoryHandle( 0 )
    , _pointLight_position_radius(0)
    , _pointLight_color_intensity(0)
    , _count_pointLights(0)
    , _capacity_lights(0)
{
}

void bxGfxLights::startup( int maxLights )
{
    int memSize = 0;
    memSize += maxLights * sizeof( Vector4 );
    memSize += maxLights * sizeof( Vector4 );

    _memoryHandle = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );

    bxBufferChunker chunker( _memoryHandle, memSize );
    _pointLight_position_radius = chunker.add< Vector4 >( maxLights );
    _pointLight_color_intensity = chunker.add< Vector4 >( maxLights );

    chunker.check();

    _count_pointLights = 0;
    _capacity_lights = maxLights;    
}
void bxGfxLights::shutdown()
{
    BX_FREE( bxDefaultAllocator(), _memoryHandle );
    _memoryHandle = 0;
    _pointLight_position_radius = 0;
    _pointLight_color_intensity = 0;
    _count_pointLights = 0;
    _capacity_lights = 0;
}

bxGfxLights::PointInstance bxGfxLights::createPointLight(const bxGfxLight_Point& light)
{
    SYS_ASSERT( _count_pointLights < _capacity_lights );

    const u32 index = _count_pointLights++;
    Indices::Handle handle = _pointLight_indices.add( index );
    PointInstance instance = { handle.asU32() };
    setPointLight( instance, light );
    return instance;
}

void bxGfxLights::releaseLight( PointInstance i )
{
    if ( !hasPointLight( i ) )
        return;

    if ( _count_pointLights <= 0 )
    {
        bxLogWarning( "No point light instances left!!!" );
        return;
    }

    SYS_ASSERT( _count_pointLights == _pointLight_indices.size() );
    
    Indices::Handle handle( i.id );
    SYS_ASSERT( _pointLight_indices.isValid( handle ) );
    
    const u32 index = _pointLight_indices.get( handle );
    Indices::Handle lastHandle = _pointLight_indices.findHandleByValue( --_count_pointLights );
    SYS_ASSERT( _pointLight_indices.isValid( lastHandle ) );
    _pointLight_indices.update( lastHandle, index );
    _pointLight_indices.remove( handle );

    _pointLight_position_radius[index] = _pointLight_position_radius[_count_pointLights];
    _pointLight_color_intensity[index] = _pointLight_color_intensity[_count_pointLights];
}

namespace
{
    void toPointLight( bxGfxLight_Point* pLight, const Vector4& pos_rad, const Vector4& col_int )
    {
        m128_to_xyz( pLight->position.xyz, pos_rad.get128() );
        m128_to_xyz( pLight->color.xyz, col_int.get128() );
        pLight->radius = pos_rad.getW().getAsFloat();
        pLight->intensity = col_int.getW().getAsFloat();
    }

}

bxGfxLight_Point bxGfxLights::pointLight( PointInstance i )
{
    bxGfxLight_Point result;

    Indices::Handle handle( i.id );
    SYS_ASSERT( _pointLight_indices.isValid( handle ) );

    const u32 index = _pointLight_indices.get( handle );

    const Vector4& pos_rad = _pointLight_position_radius[index];
    const Vector4& col_int = _pointLight_color_intensity[index];

    toPointLight( &result, pos_rad, col_int );
    
    return result;
}

void bxGfxLights::setPointLight( PointInstance i, const bxGfxLight_Point& light )
{
    Indices::Handle handle( i.id );
    SYS_ASSERT( _pointLight_indices.isValid( handle ) );

    const u32 index = _pointLight_indices.get( handle );
    _pointLight_position_radius[index] = Vector4( light.position.x, light.position.y, light.position.z, light.radius );
    _pointLight_color_intensity[index] = Vector4( light.color.x, light.color.y, light.color.z, light.intensity );
}

namespace
{
    Vector3 projectXY( const Matrix4& viewProj, const Vector3& pos, const Vector4& viewport )
    {
        Vector4 a = viewProj * Vector4( pos, oneVec );
        const floatInVec rhw = oneVec / a.getW();

        return Vector3( 
            ( oneVec + a.getX() * rhw ) * viewport.getZ() * halfVec + viewport.getX(),
            ( oneVec + a.getY() * rhw ) * viewport.getW() * halfVec + viewport.getY(),
            zeroVec );
    }
}


int bxGfxLights::cullPointLights( bxGfxLightList* list, bxGfxLight_Point* dstBuffer, int dstBufferSize, bxGfxViewFrustum_Tiles* frustumTiles, const bxGfxCamera& camera )
{
    const int nX = frustumTiles->numTilesX();
    const int nY = frustumTiles->numTilesY();
    const int tileSize = frustumTiles->tileSize();
    const float nX_rcp = 1.f / nX;
    const float nY_rcp = 1.f / nY;

    const int rtWidth  = 1920; //nX * tileSize;
    const int rtHeight = 1080; //nY * tileSize;

    const Vector3 rtWH_rcp( 1.f / rtWidth, 1.f / rtHeight, 1.f );
    const Vector3 rtWH( (float)rtWidth, (float)rtHeight, 1.f );
    const Vector3 tileSize_rcp( 1.f / tileSize, 1.f / tileSize, 1.f );
    const Vector3 aspect( 1.f, (float)rtWidth / rtHeight, 1.f );
    //const Vector4 viewport( 0.f, 0.f, 1920.f, 1080.f );

    const int nPointLights = _count_pointLights;

    const bxGfxViewFrustum mainFrustum = bxGfx::viewFrustum_extract( camera.matrix.viewProj );

    int dstLightCount = 0;

    //for( int itileY = 0; itileY < nY; ++itileY )
    //{
    //    for ( int itileX = 0; itileX < nX; ++itileX )
    //    {
    //        const bxGfxViewFrustumLRBT tileFrustum = frustumTiles->frustum( itileX, itileY );
    //        for ( int ilight = 0; ilight < nPointLights && dstLightCount < dstBufferSize; ++ilight )
    //        {
    //            const Vector4& pointLightSphere = _pointLight_position_radius[ilight];

    //            if ( bxGfx::viewFrustum_SphereIntersectLRBT( tileFrustum, pointLightSphere ) )
    //            {
    //                list->appendPointLight( itileX, itileY, ilight );
    //            }
    //        }
    //    }
    //}
    const floatInVec tileScaleX = linearstepf4( zeroVec, floatInVec( rtWidth ), floatInVec( (float)tileSize ) );
    const floatInVec tileScaleY = linearstepf4( zeroVec, floatInVec( rtHeight ), floatInVec( (float)tileSize ) );
    const Vector3 tileScaleRcp = Vector3( oneVec / tileScaleX, oneVec / tileScaleY, oneVec );

    for( int ilight = 0; ilight < nPointLights && dstLightCount < dstBufferSize; ++ilight )
    {
        const Vector4& pointLightSphere = _pointLight_position_radius[ilight];
        const Vector4& col_int = _pointLight_color_intensity[ilight];
        toPointLight( dstBuffer + dstLightCount, pointLightSphere, col_int );
        ++dstLightCount;

        const int isInMainFrustum = bxGfx::viewFrustum_SphereIntersectLRBT( mainFrustum, pointLightSphere );
        if ( !isInMainFrustum )
            continue;

        const Vector3 spherePos = pointLightSphere.getXYZ();
        const Vector3 spherePosV = mulAsVec4( camera.matrix.view, spherePos );
        const floatInVec sphereRadius = pointLightSphere.getW();

        const Vector3 vertices[8] = 
        {
            spherePosV + Vector3( -sphereRadius, -sphereRadius, -sphereRadius ),
            spherePosV + Vector3(  sphereRadius, -sphereRadius, -sphereRadius ),
            spherePosV + Vector3(  sphereRadius,  sphereRadius, -sphereRadius ),
            spherePosV + Vector3( -sphereRadius,  sphereRadius, -sphereRadius ),
            
            spherePosV + Vector3( -sphereRadius, -sphereRadius,  sphereRadius ),
            spherePosV + Vector3(  sphereRadius, -sphereRadius,  sphereRadius ),
            spherePosV + Vector3(  sphereRadius,  sphereRadius,  sphereRadius ),
            spherePosV + Vector3( -sphereRadius,  sphereRadius,  sphereRadius ),
        };

        bxAABB bboxV = bxAABB::prepare();
        for( int i = 0; i < 8; ++i )
        {   
            const Vector4 hpos4 = camera.matrix.proj * Vector4( vertices[i], oneVec );
            const Vector3 hpos = clampv( ((hpos4.getXYZ() / hpos4.getW()) + Vector3( 1.f )) * halfVec, Vector3( 0.f ), Vector3( 1.f ) );

            bboxV = bxAABB::extend( bboxV, hpos );
        }

        //{
        //    const Vector3 min = ( bxGfx::camera_unprojectNormalized( bboxV.min, inverse( camera.matrix.viewProj ) ) );
        //    const Vector3 max = ( bxGfx::camera_unprojectNormalized( bboxV.max, inverse( camera.matrix.viewProj ) ) );
        //    bxGfxDebugDraw::addBox( Matrix4( Matrix3::identity(), lerp( 0.5f, min, max ) ), ( max-min)*0.5f, 0xFF0000FF, true );
        //}

        //const Vector4 hPos4[2] = 
        //{
        //    camera.matrix.proj * Vector4( bboxV.min, oneVec ),
        //    camera.matrix.proj * Vector4( bboxV.max, oneVec ),
        //    //camera.matrix.proj * Vector4( vertices[0], oneVec ),  
        //    //camera.matrix.proj * Vector4( vertices[1], oneVec ),
        //    //camera.matrix.proj * Vector4( vertices[2], oneVec ),
        //    //camera.matrix.proj * Vector4( vertices[3], oneVec ),
        //};

        //const Vector3 hPos[2] = 
        //{
        //    clampv( ( (hPos4[0].getXYZ() / hPos4[0].getW()) + Vector3( 1.f ) ) * halfVec, Vector3(0.f), Vector3(1.f) ),
        //    clampv( ( (hPos4[1].getXYZ() / hPos4[1].getW()) + Vector3( 1.f ) ) * halfVec, Vector3(0.f), Vector3(1.f) ),
        //    //clampv( ( (hPos4[2].getXYZ() / hPos4[2].getW()) + Vector3( 1.f ) ) * halfVec, Vector3(0.f), Vector3(1.f) ),
        //    //clampv( ( (hPos4[3].getXYZ() / hPos4[3].getW()) + Vector3( 1.f ) ) * halfVec, Vector3(0.f), Vector3(1.f) ),
        //};


        //bxLogInfo( "%f; %f", hPos[0].getX().getAsFloat(), hPos[0].getY().getAsFloat() );

        //const floatInVec a = linearstepf4( zeroVec, floatInVec( rtWidth ), floatInVec( (float)tileSize ) );
        //const Vector3 b0 = hPos[0] / a;
        //const Vector3 b1 = hPos[1] / a;
        //const Vector3 b2 = hPos[2] / a;
        //const Vector3 b3 = hPos[3] / a;

        //const Vector3 sPos[4] = 
        //{
        //    mulPerElem( hPos[0], rtWH ),
        //    mulPerElem( hPos[1], rtWH ),
        //    mulPerElem( hPos[2], rtWH ),
        //    mulPerElem( hPos[3], rtWH ),
        //};

        const Vector3 tileXY[2] = 
        {
            //mulPerElem( sPos[0], tileSize_rcp ),
            //mulPerElem( sPos[1], tileSize_rcp ),
            //mulPerElem( sPos[2], tileSize_rcp ),
            //mulPerElem( sPos[3], tileSize_rcp ),
            mulPerElem( bboxV.min, tileScaleRcp ),
            mulPerElem( bboxV.max, tileScaleRcp ),
        };

        const int minTileX = clamp( int( tileXY[0].getX().getAsFloat() ), 0, nX - 1 );
        const int minTileY = clamp( int( tileXY[0].getY().getAsFloat() ), 0, nY - 1 );
        const int maxTileX = clamp( int( tileXY[1].getX().getAsFloat() ), 0, nX - 1 );
        const int maxTileY = clamp( int( tileXY[1].getY().getAsFloat() ), 0, nY - 1 );

        //const int minTileX0 = clamp( int( tileXY[0].getX().getAsFloat() ), 0, nX - 1 );
        //const int minTileX1 = clamp( int( tileXY[3].getX().getAsFloat() ), 0, nX - 1 );

        //const int minTileY0 = clamp( int( tileXY[0].getY().getAsFloat() ), 0, nY - 1 );
        //const int minTileY1 = clamp( int( tileXY[1].getY().getAsFloat() ), 0, nY - 1 );

        //const int maxTileX0 = clamp( int( tileXY[1].getX().getAsFloat() ), 0, nX - 1 );
        //const int maxTileX1 = clamp( int( tileXY[2].getX().getAsFloat() ), 0, nX - 1 );

        //const int maxTileY0 = clamp( int( tileXY[2].getY().getAsFloat() ), 0, nY - 1 );
        //const int maxTileY1 = clamp( int( tileXY[3].getY().getAsFloat() ), 0, nY - 1 );

        //const int minTileX = minOfPair( minTileX0, minTileX1 );
        //const int maxTileX = maxOfPair( maxTileX0, maxTileX1 );
        //const int minTileY = minOfPair( minTileY0, minTileY1 );
        //const int maxTileY = maxOfPair( maxTileY0, maxTileY1 );


        //Vector4 minHPos4 = camera.matrix.viewProj * Vector4( min, oneVec );
        //Vector4 maxHPos4 = camera.matrix.viewProj * Vector4( max, oneVec );

        //const Vector3 minHPos0 = clampv( ( (minHPos4.getXYZ() / minHPos4.getW()) + Vector3( 1.f ) ) * halfVec, Vector3(0.f), Vector3(1.f) );
        //const Vector3 maxHPos0 = clampv( ( (maxHPos4.getXYZ() / maxHPos4.getW()) + Vector3( 1.f ) ) * halfVec, Vector3(0.f), Vector3(1.f) );

        //const Vector3 minHPos = minPerElem( minHPos0, maxHPos0 );
        //const Vector3 maxHPos = maxPerElem( minHPos0, maxHPos0 );
        //
        //const Vector3 minSPos = mulPerElem( minHPos, rtWH );
        //const Vector3 maxSPos = mulPerElem( maxHPos, rtWH );

        //const Vector3 minTileXY = mulPerElem( minSPos, nXY_rcp );
        //const Vector3 maxTileXY = mulPerElem( maxSPos, nXY_rcp );

        //const int minTileX = clamp( int( minTileXY.getX().getAsFloat() ), 0, nX - 1 );
        //const int minTileY = clamp( int( minTileXY.getY().getAsFloat() ), 0, nY - 1 );
        //const int maxTileX = clamp( int( maxTileXY.getX().getAsFloat() ), 0, nX - 1 );
        //const int maxTileY = clamp( int( maxTileXY.getY().getAsFloat() ), 0, nY - 1 );

        //const Vector3 a = projectXY( viewProj, min, viewport );
        //const Vector3 b = projectXY( viewProj, max, viewport );

        for( int itileY = minTileY; itileY <= maxTileY; ++itileY )
        {
            for( int itileX = minTileX; itileX <= maxTileX; ++itileX )
            {
                //Vector3 corners[8];
                //bxGfx::viewFrustum_computeTileCorners( corners, inverse( camera.matrix.viewProj ), itileX, itileY, nX, nY, frustumTiles->tileSize() );
                //bxGfx::viewFrustum_debugDraw( corners, 0x00FF00FF );

                list->appendPointLight( itileX, itileY, ilight );
            }
        }
    }

    
    return dstLightCount;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


bxGfxViewFrustum_Tiles::bxGfxViewFrustum_Tiles( bxAllocator* allocator )
    : _viewProjInv( Matrix4::identity() )
    , _allocator( allocator )
    , _memoryHandle(0)
    , _frustums(0)
    , _validFlags(0)
    , _numTilesX(0), _numTilesY(0), _tileSize(0)
    , _memorySize(0)
{}

bxGfxViewFrustum_Tiles::~bxGfxViewFrustum_Tiles()
{
    BX_FREE0( _allocator, _memoryHandle );
}

void bxGfxViewFrustum_Tiles::setup( const Matrix4& viewProjInv, int numTilesX, int numTilesY, int tileSize )
{
    _viewProjInv = viewProjInv;
    
    const int numTiles = numTilesX * numTilesY;

    int memSize = 0;
    memSize += numTiles * sizeof( *_validFlags );
    memSize += numTiles * sizeof( *_frustums );

    if( memSize > _memorySize )
    {
        BX_FREE0( _allocator, _memoryHandle );
        _memoryHandle = BX_MALLOC( _allocator, memSize, 16 );
        _memorySize = memSize;

        bxBufferChunker chunker( _memoryHandle, _memorySize );
        _frustums = chunker.add< bxGfxViewFrustumLRBT >( numTiles );
        _validFlags = chunker.add< u8 >( numTiles );
        chunker.check();
    }

    memset( _memoryHandle, 0x00, _memorySize );

    _numTilesX = numTilesX;
    _numTilesY = numTilesY;
    _tileSize = tileSize;
}

const bxGfxViewFrustumLRBT& bxGfxViewFrustum_Tiles::frustum( int tileX, int tileY ) const
{
    const int index = _numTilesX * tileY + tileX;
    SYS_ASSERT( index < (_numTilesX*_numTilesY) );

    if( !_validFlags[index] )
    {
        _frustums[index] = bxGfx::viewFrustum_tile( _viewProjInv, tileX, tileY, _numTilesX, _numTilesY, _tileSize );
        _validFlags[index] = 1;
    }
    return _frustums[index];
}




//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bxGfxLightList::bxGfxLightList( bxAllocator* allocator /*= bxDefaultAllocator() */ )
    : _allocator(allocator)
    , _memoryHandle(0)
    , _tiles(0)
    , _items(0)
    , _numTilesX(0)
    , _numTilesY(0)
    , _numLights(0)
    , _memorySize(0)
{}

bxGfxLightList::~bxGfxLightList()
{
    BX_FREE( _allocator, _memoryHandle );
}

void bxGfxLightList::setup( int nTilesX, int nTilesY, int nLights )
{
    const int numTiles = nTilesX * nTilesY;
    int memSize = 0;
    memSize += numTiles * sizeof( *_tiles );
    memSize += numTiles * nLights * sizeof( *_items );

    if( memSize > _memorySize )
    {
        BX_FREE0( _allocator, _memoryHandle );
        _memoryHandle = BX_MALLOC( _allocator, memSize, 4 );
        _memorySize = memSize;
    }

    memset( _memoryHandle, 0, _memorySize );

    bxBufferChunker chunker( _memoryHandle, _memorySize );
    _tiles = chunker.add< u32 >( numTiles );
    _items = chunker.add< u32 >( numTiles * nLights );
    chunker.check();

    memset( _items, 0xFF, numTiles * nLights * sizeof( *_items ) );

    _numTilesX = nTilesX;
    _numTilesY = nTilesY;
    _numLights = nLights;
}

int bxGfxLightList::appendPointLight( int tileX, int tileY, int lightIndex )
{
    const int tileIndex = _numTilesX * tileY + tileX;
    const int firstItemIndex = _numLights * tileIndex;

    SYS_ASSERT( tileIndex < (_numTilesX*_numTilesY) );
    SYS_ASSERT( firstItemIndex < (_numTilesX*_numTilesY*_numLights) );

    Tile tile = { _tiles[tileIndex] };
    
    const int itemIndex = firstItemIndex + tile.count_pointLight;
    SYS_ASSERT( itemIndex < (_numTilesX*_numTilesY*_numLights) );
    Item item = { _items[itemIndex] };

    item.index_pointLight = lightIndex;
    tile.count_pointLight += 1;
    SYS_ASSERT( tile.count_pointLight <= _numLights );

    _tiles[tileIndex] = tile.hash;
    _items[itemIndex] = item.hash;

    return tile.count_pointLight;
}

////
////
void bxGfxLightsContext::startup( bxGdiDeviceBackend* dev, int maxLights, int tileSiz, int rtWidth, int rtHeight, bxAllocator* allocator )
{
    lightManager.startup( maxLights );
        
    data.numTilesX = iceil( rtWidth, tileSiz );
    data.numTilesY = iceil( rtHeight, tileSiz );
    data.numTiles = data.numTilesX * data.numTilesY;
    data.tileSize = tileSiz;
    data.maxLights = maxLights;

    const int numTiles = data.numTiles;

    buffer_lightsData = dev->createBuffer( maxLights * 2, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
    buffer_lightsTileIndices = dev->createBuffer( numTiles * maxLights, bxGdiFormat( bxGdi::eTYPE_UINT, 1 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
    cbuffer_lightningData = dev->createConstantBuffer( sizeof( bxGfx::LightningData ) );
    
    culledPointLightsBuffer = (bxGfxLight_Point*)BX_MALLOC( bxDefaultAllocator(), maxLights * sizeof(*culledPointLightsBuffer), 4 );
}

void bxGfxLightsContext::shutdown( bxGdiDeviceBackend* dev )
{
    
    BX_FREE0( bxDefaultAllocator(), culledPointLightsBuffer );
    dev->releaseBuffer( &cbuffer_lightningData );
    dev->releaseBuffer( &buffer_lightsTileIndices );
    dev->releaseBuffer( &buffer_lightsData );
}

void bxGfxLightsContext::cullLights( const bxGfxCamera& camera )
{
    lightList.setup( data.numTilesX, data.numTilesY, lightManager.maxLights() );
    frustumTiles.setup( inverse( camera.matrix.viewProj ), data.numTilesX, data.numTilesY, data.tileSize );
    lightManager.cullPointLights( &lightList, culledPointLightsBuffer, lightManager.maxLights(), &frustumTiles, camera );
}

void bxGfxLightsContext::bind( bxGdiContext* ctx )
{
    {
        u8* mappedData = bxGdi::buffer_map( ctx->backend(), buffer_lightsData, 0, lightManager.maxLights() );
        memcpy( mappedData, culledPointLightsBuffer, lightManager.maxLights() * sizeof( *culledPointLightsBuffer ) );
        ctx->backend()->unmap( buffer_lightsData.rs );
    }
    {
        const int maxItems = lightManager.maxLights() * data.numTiles;
        u8* mappedData = bxGdi::buffer_map( ctx->backend(), buffer_lightsTileIndices, 0, maxItems );
        memcpy( mappedData, lightList.items(), maxItems * sizeof( u32 ) );
        ctx->backend()->unmap( buffer_lightsTileIndices.rs );
    }
    {
        ctx->backend()->updateCBuffer( cbuffer_lightningData, &data );
    }

    ctx->setBufferRO( buffer_lightsData, bxGfx::eBIND_SLOT_LIGHTS_DATA_BUFFER, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setBufferRO( buffer_lightsTileIndices, bxGfx::eBIND_SLOT_LIGHTS_TILE_INDICES_BUFFER, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setCbuffer( cbuffer_lightningData, bxGfx::eBIND_SLOT_LIGHTNING_DATA, bxGdi::eSTAGE_MASK_PIXEL );
}
