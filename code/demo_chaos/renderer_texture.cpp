#include "renderer_texture.h"
#include "util\debug.h"
#include <util\id_table.h>

namespace bx{ namespace gfx{

TextureHandle TextureManager::CreateFromFile( const char* fileName )
{
    ResourceManager* resource_manager = getResourceManager();

    ResourceID resource_id = ResourceManager::createResourceID( fileName );
    ResourcePtr resource_ptr = resource_manager->acquireResource( resource_id );
    if( resource_ptr )
    {
        TextureHandle handle = { (u32)resource_ptr };
        return handle;
    }
    else
    {
        TextureHandle handle = {};

        bool create_texture = false;

        _lock.lock();
        ResourcePtr resource_ptr = resource_manager->acquireResource( resource_id );
        if ( !resource_ptr )
        {
            handle = id_table::create( _id_table );
            resource_manager->insertResource( resource_id, ResourcePtr( handle.hash ) );
            create_texture = true;
        }
        else
        {
            handle.hash = (u32)resource_ptr;
        }
        
        _lock.unlock();

        SYS_ASSERT( id_table::has( _id_table, handle ) );

        if( create_texture )
        {
            bxFS::File file = resource_manager->readFileSync( fileName );
            if( file.ok() )
            {
                _texture_ro[handle.index] = rdi::device::CreateTexture( file.bin, file.size );
                //_resource_id[handle.index] = resource_id;
            }
            file.release();
        }
        return handle;
    }
}

void TextureManager::Release( TextureHandle h )
{
    if( !id_table::has( _id_table, h ) )
        return;


    //ResourceID resource_id = _resource_id[h.index];
    ResourcePtr resource_ptr = (ResourcePtr)h.hash;
    int ref_left = getResourceManager()->releaseResource( resource_ptr );
    if( ref_left == 0 )
    {
        rdi::device::DestroyTexture( &_texture_ro[h.index] );
        id_table::destroy( _id_table, h );
        h.hash = 0;
    }
}

rdi::TextureRO TextureManager::Texture( TextureHandle h ) const
{
    SYS_ASSERT( id_table::has( _id_table, h ) );
    return _texture_ro[h.index];
}


static TextureManager* g_texture_manager = nullptr;

void TextureManagerStartUp()
{
    SYS_ASSERT( g_texture_manager == nullptr );
    g_texture_manager = BX_NEW( bxDefaultAllocator(), TextureManager );
}

void TextureManagerShutDown()
{
    BX_DELETE0( bxDefaultAllocator(), g_texture_manager );
}


TextureManager* GTextureManager()
{
    return g_texture_manager;
}

}}///
