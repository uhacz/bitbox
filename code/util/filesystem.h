#pragma once

namespace bxIO
{
    extern int readFile( unsigned char** outBuffer, size_t* outSizeInBytes, const char* path );
    extern int readTextFile( unsigned char** outBuffer, size_t* outSizeInBytes, const char* path );
    extern int writeFile( const char* absPath, unsigned char* buf, size_t sizeInBytes );
    extern int copyFile( const char* absDstPath, const char* absSrcPath );
    extern int createDir( const char* absPath );
}//

/// file
namespace bxFS
{
    struct File
    {
	    File()
        : ptr(0)
        , size(0)
        {}
        union
	    {
            void* ptr;
            unsigned char* bin;
		    char* txt;
	    };
	    size_t size;

        bool ok() const { return size > 0; }
        void release();
    };

    struct Path
    {
        enum { ePATH_LEN = 255 };
        char name[ePATH_LEN+1];
        size_t length;

        bool ok() const { return length > 0; }
    };
}//

/// filesystem
class bxFileSystem
{
public:
	bxFileSystem();
	~bxFileSystem();

	int startup( const char* rootdir );
	void shutdown();

	const bxFS::File readFile( const char* relativePath ) const;
	const bxFS::File readTextFile( const char* relativePath ) const;
	
    int writeFile( const char* relativePath, const unsigned char* data, size_t dataSize );
	int createDir( const char* relativePath );
	
    void absolutePath( bxFS::Path* absolutePath, const char* relativePath ) const ;

    const char* rootDir() const { return _rootDir; }

private:
	char* _rootDir;
	unsigned _rootDirLength;
};

