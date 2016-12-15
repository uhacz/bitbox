#pragma once
#include <rdi/rdi_backend.h>
#include <util/thread/mutex.h>
#include <util/containers.h>

namespace bx{ namespace gfx{

//typedef Handle TextureHandle;
    struct TextureHandle { u32 id; };
//////////////////////////////////////////////////////////////////////////
class TextureManager
{
public:
    TextureHandle CreateFromFile( const char* fileName );
    void Release( TextureHandle h );

    bool Alive( TextureHandle h );
    rdi::TextureRO* Texture( TextureHandle h );

    //////////////////////////////////////////////////////////////////////////
    static void _StartUp();
    static void _ShutDown();

private:
    u32 _Find( u32 hashed_name );

private:
    static const u32 MAX_TEXTURES = 64;

    id_table_t<MAX_TEXTURES> _ids;
    rdi::TextureRO _textures[MAX_TEXTURES] = {};
    u32 _hashed_names[MAX_TEXTURES] = {};
    u32 _refcounter[MAX_TEXTURES] = {};
  
    bxRecursiveBenaphore _lock;
};

TextureManager* GTextureManager();

}}///
