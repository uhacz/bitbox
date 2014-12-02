#include "lights.h"
#include <util/memory.h>
#include <util/buffer_utils.h>

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
    Indices::Handle lastHandle = _pointLight_indices.getHandleByIndex( --_count_pointLights );
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

