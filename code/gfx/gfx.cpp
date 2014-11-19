#include "gfx.h"

union bxGfxSortKey_Color
{
    u64 hash;
    struct
    {
        u64 rSource : 8;
        u64 shader  : 32;
        u64 depth   : 16;
        u64 layer   : 8;
    };
};
union bxGfxSortKey_Depth
{
    u16 hash;
    struct
    {
        u16 depth;
    };
};

////
////
bxGfxContext::bxGfxContext()
{

}

bxGfxContext::~bxGfxContext()
{

}
int bxGfxContext::startup( bxGdiDeviceBackend* dev )
{


}

void bxGfxContext::shutdown( bxGdiDeviceBackend* dev )
{

}

void bxGfxContext::frameBegin(  )
{

}
void bxGfxContext::frameDraw( bxGdiContext* ctx )
{

}
void bxGfxContext::frameEnd()
{

}


