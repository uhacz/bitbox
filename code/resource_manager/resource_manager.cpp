#include "resource_manager.h"
#include <util/hashmap.h>
#include <util/hash.h>
#include <util/string_util.h>
#include <util/filesystem.h>
#include <util/thread/mutex.h>
#include <util/debug.h>

ResourceID bxResourceManager::createResourceID( const char* path )
{
	const size_t NAME_SIZE = 128;
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
	const u32 name_hash = simple_hash( name );
	const u32 type_hash = simple_hash( type );

	return (u64(name_hash) << 32) | u64(type_hash);
}

struct bxResource
{
	bxResource()
		: data(0)
		, referenceCounter(0) 
	{}
	bxResource( uptr d )
		: data(d)
		, referenceCounter(1)
	{}
	uptr data;
	i32 referenceCounter;
};

class bxResourceManagerImpl : public bxResourceManager
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

    virtual void insert( ResourceID id, uptr data )
	{
		_mapLock.lock();
		SYS_ASSERT( !hashmap::lookup( _map, id ) );
        
        bxResource* res = BX_NEW( bxDefaultAllocator(), bxResource, data );

        hashmap::insert( _map, id )->value = (size_t)res;
		
        _mapLock.unlock();
	}
	virtual uptr lookup( ResourceID id )
	{
		uptr result = 0;
		_mapLock.lock();

        hashmap_t::cell_t* cell = hashmap::lookup( _map, id );
        if ( cell )
        {
            bxResource* res = (bxResource*)cell->value;
            result = res->data;
        }

		_mapLock.unlock();

		return result;
	}
    virtual ResourceID find( uptr data )
    {
        ResourceID result = 0;
		_mapLock.lock();

        hashmap::iterator it( _map );

        while( it.next() )
        {
            bxResource* res = (bxResource*)it->value;
            if( res->data == data )
            {
                result = it->key;
                break;
            }
        }
		_mapLock.unlock();

		return result;
    }
    virtual int referenceAdd( ResourceID id )
	{
		_mapLock.lock();
        hashmap_t::cell_t* cell = hashmap::lookup( _map, id );
        SYS_ASSERT( cell != 0 );
        bxResource* resource = (bxResource*)cell->value;
		if (resource->data)
		{
			++resource->referenceCounter;
		}
		_mapLock.unlock();

        return resource->referenceCounter;
	
	}
	virtual int referenceRemove( ResourceID id )
	{
		_mapLock.lock();
		hashmap_t::cell_t* cell = hashmap::lookup( _map, id );
        SYS_ASSERT( cell != 0 );
        bxResource* resource = (bxResource*)cell->value;

		if (resource->data)
		{
			SYS_ASSERT( resource->referenceCounter > 0 );
            if( --resource->referenceCounter == 0 )
            {
                hashmap::erase( _map, id );
                BX_DELETE0( bxDefaultAllocator(), resource );
            }
		}
		_mapLock.unlock();
        
        return resource->referenceCounter;
	}
};

bxResourceManager* bxResourceManager::startup( const char* root )
{
	bxResourceManagerImpl* impl= BX_NEW( bxDefaultAllocator(), bxResourceManagerImpl );
	impl->startup( root );
	return impl;
}
void bxResourceManager::shutdown( bxResourceManager* resuorceManager )
{
	bxResourceManagerImpl* impl = (bxResourceManagerImpl*)resuorceManager;
	impl->shutdown();
	BX_DELETE( bxDefaultAllocator(), impl );
}

