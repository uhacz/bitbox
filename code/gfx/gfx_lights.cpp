#include "gfx_lights.h"
#include "gfx_camera.h"
#include "gfx.h"
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/color.h>
#include "gfx_debug_draw.h"

bxGfxLightManager::bxGfxLightManager()
    : _memoryHandle( 0 )
    , _pointLight_position_radius(0)
    , _pointLight_color_intensity(0)
    , _count_pointLights(0)
    , _capacity_lights(0)
{
}

void bxGfxLightManager::startup( int maxLights )
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
void bxGfxLightManager::shutdown()
{
    BX_FREE( bxDefaultAllocator(), _memoryHandle );
    _memoryHandle = 0;
    _pointLight_position_radius = 0;
    _pointLight_color_intensity = 0;
    _count_pointLights = 0;
    _capacity_lights = 0;
}

bxGfxLightManager::PointInstance bxGfxLightManager::createPointLight(const bxGfxLight_Point& light)
{
    SYS_ASSERT( _count_pointLights < _capacity_lights );

    const u32 index = _count_pointLights++;
    Indices::Handle handle = _pointLight_indices.add( index );
    PointInstance instance = { handle.asU32() };
    setPointLight( instance, light );
    return instance;
}

void bxGfxLightManager::releaseLight( PointInstance i )
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

bxGfxLight_Point bxGfxLightManager::pointLight( PointInstance i )
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

void bxGfxLightManager::setPointLight( PointInstance i, const bxGfxLight_Point& light )
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


int bxGfxLightManager::cullPointLights( bxGfxLightList* list, bxGfxLight_Point* dstBuffer, int dstBufferSize, const bxGfx::LightningData& lightData, const bxGfxCamera& camera )
{
    const int nX = lightData.numTilesX;
    const int nY = lightData.numTilesY;
    const int tileSize = lightData.tileSize;

    const int rtWidth  = 1920; //nX * tileSize;
    const int rtHeight = 1080; //nY * tileSize;

    const Vector3 rtWH( (float)rtWidth, (float)rtHeight, 1.f );
    const Vector3 tileSize_rcp( lightData.tileSizeRcp );

    const int nPointLights = _count_pointLights;
    const bxGfxViewFrustum mainFrustum = bxGfx::viewFrustum_extract( camera.matrix.viewProj );

    int dstLightCount = 0;

    const floatInVec tileScaleX = linearstepf4( zeroVec, floatInVec( (float)rtWidth ), floatInVec( (float)tileSize ) );
    const floatInVec tileScaleY = linearstepf4( zeroVec, floatInVec( (float)rtHeight ), floatInVec( (float)tileSize ) );
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

        const Vector3 tileXY[2] = 
        {
            mulPerElem( bboxV.min, tileScaleRcp ),
            mulPerElem( bboxV.max, tileScaleRcp ),
        };

        const int minTileX = clamp( int( tileXY[0].getX().getAsFloat() ), 0, nX - 1 );
        const int minTileY = clamp( int( tileXY[0].getY().getAsFloat() ), 0, nY - 1 );
        const int maxTileX = clamp( int( tileXY[1].getX().getAsFloat() ), 0, nX - 1 );
        const int maxTileY = clamp( int( tileXY[1].getY().getAsFloat() ), 0, nY - 1 );

        for( int itileY = minTileY; itileY <= maxTileY; ++itileY )
        {
            for( int itileX = minTileX; itileX <= maxTileX; ++itileX )
            {
                //Vector3 corners[8];
                //bxGfx::viewFrustum_computeTileCorners( corners, inverse( camera.matrix.viewProj ), itileX, itileY, nX, nY, tileSize );
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

bxGfxLightList::bxGfxLightList()
    : _allocator(0)
    , _memoryHandle(0)
    , _tiles(0)
    , _items(0)
    , _numTilesX(0)
    , _numTilesY(0)
    , _numLights(0)
    , _memorySize(0)
{}
bxGfxLightList::~bxGfxLightList()
{}

void bxGfxLightList::startup( bxAllocator* allocator /*= bxDefaultAllocator() */ )
{
    _allocator = allocator;
}

void bxGfxLightList::shutdown()
{
    BX_FREE0( _allocator, _memoryHandle );

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
void bxGfxLights::startup( bxGdiDeviceBackend* dev, int maxLights, int tileSiz, int rtWidth, int rtHeight, bxAllocator* allocator )
{
    SYS_STATIC_ASSERT( sizeof( bxGfx::LightningData ) == 64 );
    
    lightManager.startup( maxLights );
    lightList.startup( allocator );
        
    data.numTilesX = iceil( rtWidth, tileSiz );
    data.numTilesY = iceil( rtHeight, tileSiz );
    data.numTiles = data.numTilesX * data.numTilesY;
    data.tileSize = tileSiz;
    data.maxLights = maxLights;
    data.tileSizeRcp = 1.f / (float)tileSiz;

    data.sunAngularRadius = 0.00942477796076937972f;
    setSunDir( Vector3( 1.f, -1.f, 0.f ) );
    setSunIlluminance( 110000.f );
    setSunColor( float3_t( 1.0f, 1.f, .98f ) );

    const int numTiles = data.numTiles;

    buffer_lightsData = dev->createBuffer( maxLights * 2, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
    buffer_lightsTileIndices = dev->createBuffer( numTiles * maxLights, bxGdiFormat( bxGdi::eTYPE_UINT, 1 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
    cbuffer_lightningData = dev->createConstantBuffer( sizeof( bxGfx::LightningData ) );
    
    culledPointLightsBuffer = (bxGfxLight_Point*)BX_MALLOC( bxDefaultAllocator(), maxLights * sizeof(*culledPointLightsBuffer), 4 );
}

void bxGfxLights::shutdown( bxGdiDeviceBackend* dev )
{
    BX_FREE0( bxDefaultAllocator(), culledPointLightsBuffer );
    dev->releaseBuffer( &cbuffer_lightningData );
    dev->releaseBuffer( &buffer_lightsTileIndices );
    dev->releaseBuffer( &buffer_lightsData );

    lightList.shutdown();
    lightManager.shutdown();
}

void bxGfxLights::setSunDir(const Vector3& dir)
{
    m128_to_xyz( data.sunDirection.xyz, dir.get128() );
}

void bxGfxLights::setSunColor(const float3_t& rgb)
{
    data.sunColor = rgb;
}

void bxGfxLights::setSunIlluminance(float lux)
{
    data.sunIlluminanceInLux = lux;
}

void bxGfxLights::cullLights( const bxGfxCamera& camera )
{
    lightList.setup( data.numTilesX, data.numTilesY, lightManager.maxLights() );
    //frustumTiles.setup( inverse( camera.matrix.viewProj ), data.numTilesX, data.numTilesY, data.tileSize );
    lightManager.cullPointLights( &lightList, culledPointLightsBuffer, lightManager.maxLights(), data, camera );
}

void bxGfxLights::bind( bxGdiContext* ctx )
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


#include "gfx_gui.h"
bxGfxLightsGUI::bxGfxLightsGUI()
    : flag_isVisible(0)
{
}

void bxGfxLightsGUI::show( bxGfxLights* lights, const bxGfxLightManager::PointInstance* instances, int nInstances )
{
    ImGui::Begin( "Points lights", (bool*)&flag_isVisible );

    for( int i = 0; i < nInstances; ++i )
    {
        bxGfxLightManager::PointInstance instance = instances[i];

        char instanceName[32];
        sprintf( instanceName, "%u", instance.id );

        if ( ImGui::TreeNode( instanceName ) )
        {

            bxGfxLight_Point data = lights->lightManager.pointLight( instance );

            bool changed = false;

            changed |= ImGui::InputFloat3( "position", data.position.xyz, 3 );
            changed |= ImGui::InputFloat( "radius", &data.radius, 0.1f, 1.f );
            changed |= ImGui::ColorEdit3( "color", data.color.xyz );
            changed |= ImGui::InputFloat( "intensity", &data.intensity, 0.1f, 1.f );

            if ( changed )
            {
                lights->lightManager.setPointLight( instance, data );
            }
            ImGui::TreePop();
        }

    }



    ImGui::End();
}