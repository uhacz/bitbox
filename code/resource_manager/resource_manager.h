#pragma once

#include <util/filesystem.h>
#include <util/type.h>

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
    virtual void        insertResource( ResourceID id, ResourcePtr resourcePointer ) = 0;
    virtual ResourcePtr acquireResource( ResourceID id ) = 0;
    // returns number of references left
    virtual unsigned    releaseResource( ResourcePtr resourcePointer ) = 0;

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
}///

namespace bx
{
    extern ResourceManager* getResourceManager();
}///
