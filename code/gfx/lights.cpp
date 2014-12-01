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
    memSize += maxLights * sizeof( Index );
    memSize += maxLights * sizeof( Vector4 );
    memSize += maxLights * sizeof( Vector4 );

    _memoryHandle = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );

    bxBufferChunker chunker( _memoryHandle, memSize );
    _pointLight_indices = chunker.add< Index >( maxLights );
    _pointLight_position_radius = chunker.add< Vector4 >( maxLights );
    _pointLight_color_intensity = chunker.add< Vector4 >( maxLights );

    chunker.check();

    _count_pointLights = 0;
    _capacity_lights = maxLights;    

    for( u32 i = 0; i < maxLights; ++i )
    {
        _pointLight_indices[i].index = i;
        _pointLight_indices[i].next = i + 1;
        _pointLight_indices[i].i.magic = i + 1;
        _pointLight_indices[i].i.type = eTYPE_POINT;
    }
    _pointLight_freelistDequeue = 0;
    _pointLight_freelistEnqueue = maxLights - 1;
}
void bxGfxLights::shutdown()
{
    BX_FREE( bxDefaultAllocator(), _memoryHandle );
    _memoryHandle = 0;
    _pointLight_indices = 0;
    _pointLight_position_radius = 0;
    _pointLight_color_intensity = 0;
    _count_pointLights = 0;
    _capacity_lights = 0;
}

bxGfxLights::Instance bxGfxLights::createPointLight(const bxGfxLight_Point& light)
{
    Index& idx = _pointLight_indices[_pointLight_freelistDequeue];
    _pointLight_freelistDequeue = idx.next;
    idx.index = ++_count_pointLights;
    return idx.i;
}

void bxGfxLights::releaseLight(Instance i)
{
    if( i.type == eTYPE_POINT )
    {
        if ( !hasPointLight( i ) )
            return;

        Index& idx = _pointLight_indices[i.index];
        

    }
}

bxGfxLight_Point bxGfxLights::pointLight(Instance i)
{
    bxGfxLight_Point result;
    return result;
}

void bxGfxLights::setPointLight(Instance i, const bxGfxLight_Point& light)
{
}

