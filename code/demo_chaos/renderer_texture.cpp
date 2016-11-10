#include "renderer_texture.h"
#include <util\debug.h>

namespace bx{ namespace gfx{

TextureHandle TextureManager::CreateFromFile( const char* fileName )
{
    
    
    ResourceManager* resource_manager = GResourceManager();

    ResourceID resource_id = ResourceManager::createResourceID( fileName );
    ResourcePtr resource_ptr = resource_manager->acquireResource( resource_id );
    if( resource_ptr )
    {
        auto resource = ( rdi::TextureRO* )resource_ptr;
        TextureHandle handle = GHandle()->Create( (uptr)resource );
        return handle;
    }
    else
    {
        TextureHandle handle = {};
        rdi::TextureRO* ptr = nullptr;
        bool create_texture = false;

        _lock.lock();
        ResourcePtr resource_ptr = resource_manager->acquireResource( resource_id );
        if ( !resource_ptr )
        {
            ptr = _Alloc();
            handle = GHandle()->Create( (uptr)ptr );
            resource_manager->insertResource( resource_id, ResourcePtr( ptr ) );
            create_texture = true;
        }
        else
        {
            handle = GHandle()->Create( (uptr)resource_ptr );
        }
        
        _lock.unlock();


        SYS_ASSERT( GHandle()->Alive( handle ) );

        if( create_texture )
        {
            bxFS::File file = resource_manager->readFileSync( fileName );
            if( file.ok() )
            {
                ptr[0] = rdi::device::CreateTexture( file.bin, file.size );
            }
            file.release();
        }
        return handle;
    }
}

void TextureManager::Release( TextureHandle h )
{
    if( !GHandle()->Alive( h ) )
        return;


    //ResourceID resource_id = _resource_id[h.index];
    ResourcePtr resource_ptr = GHandle()->DataAs<ResourcePtr>( h );
    int ref_left = GResourceManager()->releaseResource( resource_ptr );
    if( ref_left == 0 )
    {
        rdi::TextureRO* texture = ( rdi::TextureRO* )resource_ptr;
        rdi::device::DestroyTexture( texture );
        _Free( texture );
    }

    GHandle()->Destroy( h );
}

void TextureManager::_StartUp()
{
    _allocator.startup( sizeof( rdi::TextureRO ), MAX_TEXTURES, bxDefaultAllocator(), sizeof( void* ) );
}

void TextureManager::_ShutDown()
{
    _allocator.shutdown();
}

rdi::TextureRO* TextureManager::_Alloc()
{
    _lock.lock();
    rdi::TextureRO* ptr = new( _allocator.alloc() ) rdi::TextureRO();
    _lock.unlock();
    return ptr;
}

void TextureManager::_Free( rdi::TextureRO* ptr )
{
    _lock.lock();
    _allocator.free( ptr );
    _lock.unlock();
}

static TextureManager* g_texture_manager = nullptr;

void TextureManagerStartUp()
{
    SYS_ASSERT( g_texture_manager == nullptr );
    g_texture_manager = BX_NEW( bxDefaultAllocator(), TextureManager );
    g_texture_manager->_StartUp();
}

void TextureManagerShutDown()
{
    if( !g_texture_manager )
        return;

    g_texture_manager->_ShutDown();
    BX_DELETE0( bxDefaultAllocator(), g_texture_manager );
}


TextureManager* GTextureManager()
{
    return g_texture_manager;
}

}}///
