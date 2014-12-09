#include "gfx_lights.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include "gfx_camera.h"

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

bxGfxLight_Point bxGfxLights::pointLight( PointInstance i )
{
    bxGfxLight_Point result;

    Indices::Handle handle( i.id );
    SYS_ASSERT( _pointLight_indices.isValid( handle ) );

    const u32 index = _pointLight_indices.get( handle );

    const Vector4& pos_rad = _pointLight_position_radius[index];
    const Vector4& col_int = _pointLight_color_intensity[index];

    m128_to_xyz( result.position.xyz, pos_rad.get128() );
    m128_to_xyz( result.color.xyz, col_int.get128() );
    result.radius = pos_rad.getW().getAsFloat();
    result.intensity = col_int.getW().getAsFloat();
    
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

int bxGfxLights::cullPointLights( bxGfxLightList* list, bxGfxLight_Point* dstBuffer, int dstBufferSize, bxGfxViewFrustum_Tiles* frustumTiles, const Matrix4& viewProj )
{
    const int nX = frustumTiles->numTilesX();
    const int nY = frustumTiles->numTilesY();
    const int tileSize = frustumTiles->tileSize();
    const float nX_rcp = 1.f / nX;
    const float nY_rcp = 1.f / nY;

    const int rtWidth = 1920;
    const int rtHeight = 1080;

    const Vector3 rtWH_rcp( 1.f / rtWidth, 1.f / rtHeight, 1.f );
    const Vector3 rtWH( rtWidth, rtHeight, 1.f );
    const Vector3 nXY_rcp( 1.f / tileSize, 1.f / tileSize, 1.f );

    const int nPointLights = _count_pointLights;

    const bxGfxViewFrustum mainFrustum = bxGfx::viewFrustum_extract( viewProj );

    for( int ilight = 0; ilight < nPointLights; ++ilight )
    {
        const Vector4& pointLightSphere = _pointLight_position_radius[ilight];
        
        const int isInMainFrustum = bxGfx::viewFrustum_SphereIntersectLRBT( mainFrustum, pointLightSphere );
        if ( !isInMainFrustum )
            continue;

        const Vector4 hpos_ = viewProj * Vector4( pointLightSphere.getXYZ(), oneVec );
        const Vector3 hpos = clampv( ( (hpos_.getXYZ() / hpos_.getW()) + Vector3( 1.f ) ) * halfVec, Vector3(0.f), Vector3(1.f) );

        const Vector3 screenPos = mulPerElem( hpos, rtWH );
        
        const Vector3 tileXY = mulPerElem( screenPos, nXY_rcp );

        const int tileX = (int)tileXY.getX().getAsFloat();
        const int tileY = (int)tileXY.getY().getAsFloat();

        const bxGfxViewFrustumLRBT tileFrustum = frustumTiles->frustum( tileX, tileY );
        int inTileFrustum = bxGfx::viewFrustum_SphereIntersectLRBT( tileFrustum, pointLightSphere );
        if( inTileFrustum )
        {
            Vector3 corners[8];
            bxGfx::viewFrustum_computeTileCorners( corners, inverse( viewProj ), tileX, tileY, nX, nY, frustumTiles->tileSize() );
            bxGfx::viewFrustum_debugDraw( corners, 0x00FF00FF );
        }

    }

    
    return 0;
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
