#pragma once
#include <rdi/rdi_backend.h>
#include <resource_manager/resource_manager.h>
#include <util/pool_allocator.h>
//#include <util/containers.h>
#include <util/thread/mutex.h>

namespace bx{ namespace gfx{

typedef Handle TextureHandle;
//////////////////////////////////////////////////////////////////////////
class TextureManager
{
public:
    TextureHandle CreateFromFile( const char* fileName );
    void Release( TextureHandle h );

    void _StartUp();
    void _ShutDown();

private:
    rdi::TextureRO* _Alloc();
    void _Free( rdi::TextureRO* ptr );

    static const u32 MAX_TEXTURES = 64;
    bxDynamicPoolAllocator _allocator;
    bxBenaphore _lock;

    //id_table_t<MAX_TEXTURES> _id_table;
    //rdi::TextureRO _texture_ro[MAX_TEXTURES] = {};
    //ResourceID _resource_id[MAX_TEXTURES] = {};
};

void TextureManagerStartUp();
void TextureManagerShutDown();

TextureManager* GTextureManager();

}}///
