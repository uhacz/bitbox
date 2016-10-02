#pragma once

#include <util/filesystem.h>
#include <util/type.h>

namespace bx
{
typedef u64 ResourceID;

class ResourceManager
{
public:
    static ResourceManager* startup( const char* root );
	static void shutdown( ResourceManager** resourceManager );
    
    virtual ~ResourceManager() {}

	virtual bxFS::File readFileSync( const char* relativePath ) = 0;
    virtual bxFS::File readTextFileSync( const char* relativePath ) = 0;
    virtual bxFS::Path absolutePath( const char* relativePath ) = 0;

    virtual void       insert( ResourceID id, uptr data ) = 0;
	virtual uptr       lookup( ResourceID id ) = 0;
    virtual ResourceID find( uptr data ) = 0;

    virtual int   referenceAdd( ResourceID id ) = 0;
	virtual int   referenceRemove( ResourceID id ) = 0;

    static ResourceID createResourceID( const char* path );
};
}///

namespace bx
{
    extern ResourceManager* getResourceManager();
}///
