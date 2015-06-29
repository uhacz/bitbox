#pragma once

#include <util/filesystem.h>
#include <util/type.h>


typedef u64 bxResourceID;

class bxResourceManager
{
public:
    static bxResourceManager* startup( const char* root );
	static void shutdown( bxResourceManager** resourceManager );
    
    virtual ~bxResourceManager() {}

	virtual bxFS::File readFileSync( const char* relativePath ) = 0;
    virtual bxFS::File readTextFileSync( const char* relativePath ) = 0;
    virtual bxFS::Path absolutePath( const char* relativePath ) = 0;

    virtual void       insert( bxResourceID id, uptr data ) = 0;
	virtual uptr       lookup( bxResourceID id ) = 0;
    virtual bxResourceID find( uptr data ) = 0;

    virtual int   referenceAdd( bxResourceID id ) = 0;
	virtual int   referenceRemove( bxResourceID id ) = 0;

    static bxResourceID createResourceID( const char* path );
};
