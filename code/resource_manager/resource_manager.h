#pragma once

#include <util/filesystem.h>
#include <util/type.h>
#include <util/debug.h>
#include <util/id_table.h>
#include <util/thread/mutex.h>

namespace bx
{
typedef u64 ResourceID;
typedef void* ResourcePtr;
struct ResourceLoadResult
{
    ResourceID id = 0;
    ResourcePtr ptr = nullptr;
    size_t size = 0;
    bool ok() const { return id != 0; }
};

namespace EResourceFileType
{
    enum Enum
    {
        TEXT,
        BINARY,
    };
}///

class ResourceManager
{
public:
    static ResourceManager* startup( const char* root );
	static void shutdown( ResourceManager** resourceManager );
    
    virtual ~ResourceManager() {}

	virtual bxFS::File readFileSync( const char* relativePath ) = 0;
    virtual bxFS::File readTextFileSync( const char* relativePath ) = 0;
    virtual bxFS::Path absolutePath( const char* relativePath ) = 0;

    // resourceManager takes ownership of loaded data. 
    // When user will call unloadResource underlying resource data will be destroyed when reference counter reach zero
    virtual ResourceLoadResult loadResource( const char* filename, EResourceFileType::Enum fileType ) = 0;
    virtual void        unloadResource( ResourcePtr* resourcePointer ) = 0;

    // these functions does not manage allocated resources data
    virtual int         insertResource( ResourceID id, ResourcePtr resourcePointer ) = 0;
    virtual ResourcePtr acquireResource( ResourceID id ) = 0;
    // returns number of references left
    virtual unsigned    releaseResource( ResourcePtr resourcePointer ) = 0;
    virtual unsigned    releaseResource( ResourceID resourceId ) = 0;

    //virtual void       insert( ResourceID id, ResourcePtr data ) = 0;
	//virtual ResourcePtr lookup( ResourceID id ) = 0;
    //virtual ResourceID find( ResourcePtr data ) = 0;
    //
    //virtual int   referenceAdd( ResourceID id ) = 0;
	//virtual int   referenceRemove( ResourceID id ) = 0;

    static ResourceID createResourceID( const char* path );

    //virtual void lock() = 0;
    //virtual void unlock() = 0;
};
//////////////////////////////////////////////////////////////////////////

typedef id_t Handle;
class HandleManager
{
public:
    Handle Create( uptr data );
    void Destroy( Handle handle );

    bool Alive( Handle handle ) const { return id_table::has( _id_table, handle ); }
    
    uptr Data( Handle handle )
    {
        SYS_ASSERT( id_table::has( _id_table, handle ) );
        return _data[handle.index];
    }
    template< typename T >
    T DataAs( Handle handle )
    {
        SYS_STATIC_ASSERT( sizeof( T ) == sizeof( *_data ) );
        return (T)Data( handle );
    }
    
    void SetData( Handle handle, uptr data )
    {
        SYS_ASSERT( id_table::has( _id_table, handle ) );
        _data[handle.index] = data;
    }
    template< typename T >
    void SetDataAs( Handle handle, T data )
    {
        SYS_STATIC_ASSERT( sizeof( T ) == sizeof( *_data ) );
        SetData( handle, (uptr)data );
    }

private:
    static const u32 MAX_HANDLES = 16*1024;
    id_table_t<MAX_HANDLES> _id_table;
    uptr _data[MAX_HANDLES] = {};

    bxBenaphore _lock;
};

}///

namespace bx
{
    extern ResourceManager* GResourceManager();
    extern HandleManager* GResourceHandle();
}///
