#include "renderer_texture.h"
#include <util\debug.h>
#include <util\hash.h>
#include <util\tag.h>
#include <util\id_table.h>
#include <resource_manager\resource_manager.h>


namespace bx{ namespace gfx{

namespace texture_manager_internal
{
    static const u32 HASH_SEED = bxTag32( "TEXR" );
    static inline u32 GenerateNameHash( const char* name )
    {
        return murmur3_hash32( name, (u32)strlen( name ), HASH_SEED );
    }
}

TextureHandle TextureManager::CreateFromFile( const char* fileName )
{
    u32 hashed_name = texture_manager_internal::GenerateNameHash( fileName );
    
    TextureHandle result = {};
    bool load_texture = false;
    _lock.lock();

    u32 index = _Find( hashed_name );
    if( index != UINT32_MAX )
    {
        _refcounter[index] += 1;
        result.id = id_table::id( _ids, index ).hash;
    }
    else
    {
        id_t id = id_table::create( _ids );
        _refcounter[id.index] = 1;
        _hashed_names[id.index] = hashed_name;
        _textures[id.index] = {};

        result.id = id.hash;
        load_texture = true;
    }

    _lock.unlock();
    
    if( load_texture )
    {
        const id_t id = { result.id };

        rdi::TextureRO* ptr = &_textures[id.index];

        ResourceManager* resource_manager = GResourceManager();
        bxFS::File file = resource_manager->readFileSync( fileName );
        if( file.ok() )
        {
            if( strstr( fileName, ".dds" ) || strstr( fileName, ".DDS" ) )
                ptr[0] = rdi::device::CreateTextureFromDDS( file.bin, file.size );
            else if( strstr( fileName, ".hdr" ) )
                ptr[0] = rdi::device::CreateTextureFromHDR( file.bin, file.size );
            else
                SYS_NOT_IMPLEMENTED;

        }
        file.release();
    }

    return result;


    //ResourceID resource_id = ResourceManager::createResourceID( fileName );
    //ResourcePtr resource_ptr = resource_manager->acquireResource( resource_id );
    //if( resource_ptr )
    //{
    //    auto resource = ( rdi::TextureRO* )resource_ptr;
    //    TextureHandle handle = GHandle()->Create( (uptr)resource );
    //    return handle;
    //}
    //else
    //{
    //    TextureHandle handle = {};
    //    rdi::TextureRO* ptr = nullptr;
    //    bool create_texture = false;

    //    _lock.lock();
    //    ResourcePtr resource_ptr = resource_manager->acquireResource( resource_id );
    //    if ( !resource_ptr )
    //    {
    //        ptr = _Alloc();
    //        handle = GHandle()->Create( (uptr)ptr );
    //        resource_manager->insertResource( resource_id, ResourcePtr( ptr ) );
    //        create_texture = true;
    //    }
    //    else
    //    {
    //        handle = GHandle()->Create( (uptr)resource_ptr );
    //    }
    //    
    //    _lock.unlock();


    //    SYS_ASSERT( GHandle()->Alive( handle ) );

    //    if( create_texture )
    //    {
    //        bxFS::File file = resource_manager->readFileSync( fileName );
    //        if( file.ok() )
    //        {
    //            if( strstr( fileName, ".dds" ) || strstr( fileName, ".DDS" ) )
    //                ptr[0] = rdi::device::CreateTextureFromDDS( file.bin, file.size );
    //            else if( strstr( fileName, ".hdr" ) )
    //                ptr[0] = rdi::device::CreateTextureFromHDR( file.bin, file.size );
    //            else
    //                SYS_NOT_IMPLEMENTED;

    //        }
    //        file.release();
    //    }
    //    return handle;
    //}
}

void TextureManager::Release( TextureHandle h )
{
    id_t id = { h.id };
    if( !id_table::has( _ids, id ) )
    {
        return;
    }

    rdi::TextureRO to_release = {};


    _lock.lock();

    SYS_ASSERT( _refcounter[id.index] > 0 );
    if( --_refcounter[id.index] == 0 )
    {
        to_release = _textures[id.index];

        _textures[id.index] = {};
        _hashed_names[id.index] = 0;
    }

    _lock.unlock();

    if( to_release.id )
    {
        rdi::device::DestroyTexture( &to_release );
    }

    //if( !GHandle()->Alive( h ) )
    //    return;


    ////ResourceID resource_id = _resource_id[h.index];
    //ResourcePtr resource_ptr = GHandle()->DataAs<ResourcePtr>( h );
    //int ref_left = GResourceManager()->releaseResource( resource_ptr );
    //if( ref_left == 0 )
    //{
    //    rdi::TextureRO* texture = ( rdi::TextureRO* )resource_ptr;
    //    rdi::device::DestroyTexture( texture );
    //    _Free( texture );
    //}

    //GHandle()->Destroy( h );
}


bool TextureManager::Alive( TextureHandle h )
{
    id_t id = { h.id };
    return id_table::has( _ids, id );
}

rdi::TextureRO* TextureManager::Texture( TextureHandle h )
{
    id_t id = { h.id };
    SYS_ASSERT( id_table::has( _ids, id ) );
    return &_textures[id.index];
}

//rdi::TextureRO* TextureManager::_Alloc()
//{
//    _lock.lock();
//    rdi::TextureRO* ptr = new( _allocator.alloc() ) rdi::TextureRO();
//    _lock.unlock();
//    return ptr;
//}
//
//void TextureManager::_Free( rdi::TextureRO* ptr )
//{
//    _lock.lock();
//    _allocator.free( ptr );
//    _lock.unlock();
//}

static TextureManager* g_texture_manager = nullptr;
void TextureManager::_StartUp()
{
    SYS_ASSERT( g_texture_manager == nullptr );
    g_texture_manager = BX_NEW( bxDefaultAllocator(), TextureManager );

    //g_texture_manager->_allocator.startup( sizeof( rdi::TextureRO ), MAX_TEXTURES, bxDefaultAllocator(), sizeof( void* ) );
}

void TextureManager::_ShutDown()
{
    //g_texture_manager->_allocator.shutdown();
    BX_DELETE0( bxDefaultAllocator(), g_texture_manager );
}

u32 TextureManager::_Find( u32 hashed_name )
{
    SYS_ASSERT( hashed_name != 0 );
    for( u32 i = 0; i < MAX_TEXTURES; ++i )
    {
        if( _hashed_names[i] == hashed_name )
            return i;
    }

    return UINT32_MAX;
}


TextureManager* GTextureManager()
{
    return g_texture_manager;
}

}}///
