#pragma once
#include <rdi/rdi_backend.h>
#include <resource_manager/resource_manager.h>
#include <util/pool_allocator.h>
#include <util/containers.h>
#include <util/thread/mutex.h>

namespace bx{ namespace gfx{

typedef id_t TextureHandle;
//////////////////////////////////////////////////////////////////////////
class TextureManager
{
public:
    TextureHandle CreateFromFile( const char* fileName );
    void Release( TextureHandle h );
    
    rdi::TextureRO Texture( TextureHandle h ) const;

private:
    static const u32 MAX_TEXTURES = 64;
    id_table_t<MAX_TEXTURES> _id_table;
    
    rdi::TextureRO _texture_ro[MAX_TEXTURES] = {};
    //ResourceID _resource_id[MAX_TEXTURES] = {};

    bxBenaphore _lock;
};

void TextureManagerStartUp();
void TextureManagerShutDown();

TextureManager* GTextureManager();

}}///
