#pragma once
#include <rdi/rdi_backend.h>
#include <resource_manager/resource_manager.h>
#include <util/pool_allocator.h>
#include <util/thread/mutex.h>

namespace bx{ namespace gfx{

typedef Handle TextureHandle;
//////////////////////////////////////////////////////////////////////////
class TextureManager
{
public:
    TextureHandle CreateFromFile( const char* fileName );
    void Release( TextureHandle h );

    bool Alive( TextureHandle h ) { return GHandle()->Alive( h ); }
    rdi::TextureRO* Texture( TextureHandle h ) { return GHandle()->DataAs<rdi::TextureRO*>( h ); }

    static void _StartUp();
    static void _ShutDown();

private:
    rdi::TextureRO* _Alloc();
    void _Free( rdi::TextureRO* ptr );

    static const u32 MAX_TEXTURES = 64;
    bxDynamicPoolAllocator _allocator;
    bxBenaphore _lock;
};

TextureManager* GTextureManager();

}}///
