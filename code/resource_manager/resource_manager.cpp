#include "resource_manager.h"
#include <util/hashmap.h>
#include <util/hash.h>
#include <util/string_util.h>
#include <util/filesystem.h>
#include <util/thread/mutex.h>
#include <util/debug.h>

namespace bx
{

ResourceID ResourceManager::createResourceID( const char* path )
{
    const size_t NAME_SIZE = 256;
    const size_t TYPE_SIZE = 32;
    char name[NAME_SIZE];
    char type[TYPE_SIZE];
    memset( name, 0, sizeof(name) );
    memset( type, 0, sizeof(type) );

    char* str = (char*)path;

    str = string::token( str, name, NAME_SIZE-1, "." );
    if( str )
    {
        string::token( str, type, TYPE_SIZE-1, " .\n" );
    }
    return createResourceID( name, type );
}

ResourceID ResourceManager::createResourceID( const char* name, const char* type )
{
    const u32 name_hash = simple_hash( name );
    const u32 type_hash = simple_hash( type );

    return ( u64( name_hash ) << 32 ) | u64( type_hash );
}

struct Resource
{
    Resource( ResourceLoadResult d )
        : data( d )
        , referenceCounter(1)
    {}
    ResourceLoadResult data;
    i32 referenceCounter;
};

class bxResourceManagerImpl : public ResourceManager
{
public:
    typedef hashmap_t ResourceMap;

    bxFileSystem _fs;
    ResourceMap _map;
    bxBenaphore _mapLock;

public:
    virtual ~bxResourceManagerImpl() {}

    int startup( const char* root )
    {
        int ires = _fs.startup( root );

        return ires;
    }

    void shutdown()
    {
        _fs.shutdown();
        if( !hashmap::empty( _map ) )
        {
            bxLogError( "There are live resources!!!" );
        }
    }

    virtual bxFS::File readFileSync( const char* relative_path )
    {
        return _fs.readFile( relative_path );
    }
    virtual bxFS::File readTextFileSync( const char* relative_path )
    {
        return _fs.readTextFile( relative_path );
    }
    virtual bxFS::Path absolutePath( const char* relativePath )
    {
        bxFS::Path path;
        _fs.absolutePath( &path, relativePath );
        return path;
    }

    void insert( ResourceID id, ResourceLoadResult data )
    {
        //_mapLock.lock();
        SYS_ASSERT( !hashmap::lookup( _map, id ) );
        
        Resource* res = BX_NEW( bxDefaultAllocator(), Resource, data );

        hashmap::insert( _map, id )->value = (size_t)res;
        
        //_mapLock.unlock();
    }
    ResourceLoadResult lookup( ResourceID id )
    {
        ResourceLoadResult result = {};
        //_mapLock.lock();

        hashmap_t::cell_t* cell = hashmap::lookup( _map, id );
        if ( cell )
        {
            Resource* res = (Resource*)cell->value;
            result = res->data;
        }

        //_mapLock.unlock();

        return result;
    }
    
    ResourceID find( ResourcePtr data )
    {
        ResourceID result = 0;
        //_mapLock.lock();

        hashmap::iterator it( _map );

        while( it.next() )
        {
            Resource* res = (Resource*)it->value;
            if( res->data.ptr == data )
            {
                result = it->key;
                break;
            }
        }
        //_mapLock.unlock();

        return result;
    }
    int referenceAdd( ResourceID id )
    {
        //_mapLock.lock();
        hashmap_t::cell_t* cell = hashmap::lookup( _map, id );
        SYS_ASSERT( cell != 0 );
        Resource* resource = (Resource*)cell->value;
        //if (resource->data )
        //{
        ++resource->referenceCounter;
        //}
        //_mapLock.unlock();

        return resource->referenceCounter;
    
    }
    
    int referenceRemove( ResourceID id )
    {
        //_mapLock.lock();
        hashmap_t::cell_t* cell = hashmap::lookup( _map, id );
        SYS_ASSERT( cell != 0 );
        Resource* resource = (Resource*)cell->value;

        //if (resource->data)
        {
            SYS_ASSERT( resource->referenceCounter > 0 );
            if( --resource->referenceCounter == 0 )
            {
                hashmap::eraseByKey( _map, id );
                BX_DELETE0( bxDefaultAllocator(), resource );
            }
        }
        //_mapLock.unlock();
        
        return ( resource ) ? resource->referenceCounter : 0;
    }

    ResourceLoadResult loadResource( const char* filename, EResourceFileType::Enum fileType ) override
    {
        ResourceID resource_id = ResourceManager::createResourceID( filename );
        _mapLock.lock();
        
         ResourceLoadResult result = lookup( resource_id );
        if( !result.ok() )
        {
            bxFS::File f = {};
            switch( fileType )
            {
            case EResourceFileType::TEXT:
                f = readTextFileSync( filename );
                break;
            case EResourceFileType::BINARY:
                f = readFileSync( filename );
                break;
            default:
                break;
            }//
            
            if( f.ok() )
            {
                result.id = resource_id;
                result.ptr = f.ptr;
                result.size = f.size;
                insert( resource_id, result );
            }
        }
        else
        {
            referenceAdd( resource_id );
        }

        _mapLock.unlock();
        return result;
    }
    void unloadResource( ResourcePtr* resourcePointer ) override
    {
        _mapLock.lock();
        ResourceID resource_id = find( *resourcePointer );
        SYS_ASSERT( resource_id != 0 );
        int references_left = referenceRemove( resource_id );
        if( references_left == 0 )
        {
            BX_FREE0( bxDefaultAllocator(), resourcePointer[0] );
        }
        _mapLock.unlock();
    }
    
    int ResourceManager::insertResource( ResourceID id, ResourcePtr ptr )
    {
        int reference_count = 0;
        _mapLock.lock();
        ResourceLoadResult result = lookup( id );
        if( result.ok() )
        {
            reference_count = referenceAdd( id );
        }
        else
        {
            ResourceLoadResult rlr;
            rlr.id = id;
            rlr.ptr = ptr;
            rlr.size = 0;
            insert( id, rlr );
            reference_count = 1;
        }
        _mapLock.unlock();

        return reference_count;
    }

    virtual ResourcePtr acquireResource( ResourceID id )
    {
        _mapLock.lock();
        ResourceLoadResult result = lookup( id );
        if( result.ok() )
        {
            referenceAdd( id );
        }
        _mapLock.unlock();

        return result.ptr;
    }
    virtual unsigned releaseResource( ResourcePtr resourcePointer )
    {
        _mapLock.lock();
        ResourceID resource_id = find( resourcePointer );
        SYS_ASSERT( resource_id != 0 );
        int references_left = referenceRemove( resource_id );
        _mapLock.unlock();

        return (unsigned)references_left;
    }
    virtual unsigned releaseResource( ResourceID resourceId )
    {
        _mapLock.lock();
        int references_left = referenceRemove( resourceId );
        _mapLock.unlock();
        return (unsigned)references_left;
    }


};

static ResourceManager* __resourceManager = nullptr;

ResourceManager* ResourceManager::startup( const char* root )
{
    bxResourceManagerImpl* impl= BX_NEW( bxDefaultAllocator(), bxResourceManagerImpl );
    impl->startup( root );

    __resourceManager = impl;
    return impl;
}
void ResourceManager::shutdown( ResourceManager** resourceManager )
{
    bxResourceManagerImpl* impl = (bxResourceManagerImpl*)resourceManager[0];
    impl->shutdown();
    BX_DELETE( bxDefaultAllocator(), impl );

    __resourceManager = nullptr;

    resourceManager[0] = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
Handle HandleManager::Create( uptr data )
{
    _lock.lock();
    Handle handle = id_table::create( _id_table );
    _lock.unlock();

    _data[handle.index] = data;
    return handle;
}

void HandleManager::Destroy( Handle handle )
{
    if( !id_table::has( _id_table, handle ) )
        return;

    _lock.lock();
    u32 index = handle.index;
    id_table::destroy( _id_table, handle );
    _lock.unlock();

    _data[handle.index] = 0;
}

static HandleManager* g_handle_manager = nullptr;
HandleManager* HandleManager::_StartUp()
{
    g_handle_manager = BX_NEW( bxDefaultAllocator(), HandleManager );
    return g_handle_manager;
}

void HandleManager::_ShutDown( HandleManager** handleManager )
{
    SYS_ASSERT( *handleManager == g_handle_manager );
    BX_DELETE0( bxDefaultAllocator(), handleManager[0] );
}

}///

namespace bx
{
    ResourceManager* bx::GResourceManager()
    {
        return __resourceManager;
    }

    HandleManager* GHandle()
    {
        return g_handle_manager;
    }

}///
