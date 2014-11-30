#include "lights.h"
#include <util/memory.h>
#include <util/buffer_utils.h>

union bxGfxLights_InstanceImpl
{
    u32 hash;
    struct
    {
        u32 index : 12;
        u32 magic : 16;
        u32 type  : 4;
    };
};

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

bxGfxLights::Instance bxGfxLights::createPointLight(const bxGfxLight_Point& light)
{

}

void bxGfxLights::releaseLight(Instance i)
{
}

bxGfxLight_Point bxGfxLights::pointLight(Instance i)
{
}

void bxGfxLights::setPointLight(Instance i, const bxGfxLight_Point& light)
{
}

