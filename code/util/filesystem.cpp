#include "fileSystem.h"
#include "memory.h"
#include "debug.h"
#include <stdio.h>
#include <direct.h>
#include <errno.h>
#include <string.h>

//#pragma warning( disable: 4996 )

namespace bxIO
{

static FILE* _OpenFile( const char* path, const char* mode )
{
	if ( !path || strlen(path) == 0 )
	{
		bxLogError( "Provided path is empty!\n" );
		return 0;
	}

    FILE* f = nullptr;
    errno_t err = fopen_s( &f, path, mode );
	if( err != 0 )
	{
        bxLogError( "Can not open file %s (mode: %s | errno: %d)\n", path, mode, err );
	}

	return f;
}

int readFile( unsigned char** outBuffer, size_t* outSizeInBytes, const char* path )
{
	FILE* f = _OpenFile( path, "rb" );
	if ( !f )
	{
		return -1;
	}

	fseek( f, 0, SEEK_END );
	u32 sizeInBytes = (u32)ftell( f );
	fseek( f, 0, SEEK_SET );

    u8* buf = (u8*)BX_MALLOC( bxDefaultAllocator(), sizeInBytes, 1 );
	SYS_ASSERT( buf && "out of memory?" );
	size_t readBytes = fread( buf, 1, sizeInBytes, f );
	SYS_ASSERT( readBytes == sizeInBytes );
	int ret = fclose( f );
	SYS_ASSERT( ret != EOF );

	*outBuffer = buf;
	*outSizeInBytes = sizeInBytes;

	return 0;	
}

int readTextFile( unsigned char** outBuffer, size_t* outSizeInBytes, const char* path )
{
	FILE* f = _OpenFile( path, "rb" );
	if ( !f )
	{
		return -1;
	}

	fseek( f, 0, SEEK_END );
	u32 sizeInBytes = (u32)ftell( f );
	fseek( f, 0, SEEK_SET );

	u8* buf = (u8*)BX_MALLOC( bxDefaultAllocator(), sizeInBytes + 1, 1 );
	SYS_ASSERT( buf && "out of memory?" );
	size_t readBytes = fread( buf, 1, sizeInBytes, f );
	SYS_ASSERT( readBytes == sizeInBytes );
	int ret = fclose( f );
	SYS_ASSERT( ret != EOF );

	buf[sizeInBytes] = 0;

	*outBuffer = buf;
	*outSizeInBytes = sizeInBytes + 1;

	return 0;
}

int writeFile( const char* absPath, unsigned char* buf, size_t sizeInBytes )
{
    FILE* fp = _OpenFile( absPath, "wb" );
	if ( !fp )
	{
		return -1;
	}

	size_t written = fwrite( buf, 1, sizeInBytes, fp );
	fclose(fp);
	if ( written != sizeInBytes )
	{
		bxLogError( "Can't write requested number of bytes to file '%s'\n", absPath );
		return -1;
	}

	return (int)written;
}

int copyFile( const char* abs_dst_path, const char* abs_src_path )
{
	u8* buf = 0;
	size_t len = 0;
	i32 res = readFile( &buf, &len, abs_src_path );
	if( res == 0 )
	{
		writeFile( abs_dst_path, buf, len );
	}

    BX_FREE0( bxDefaultAllocator(), buf );
	return res;
}

int createDir( const char* abs_path )
{
	const int res = _mkdir( abs_path );
	return (res == ENOENT ) ? -1 : 0;
}

}//io


namespace bxFS
{

    
void File::release()
{
    BX_FREE( bxDefaultAllocator(), bin );
    bin = 0;
    size = 0;
}

}//fs

using namespace bxFS;
//////////////////////////////////////////////////////////////////////////
/// bxFileSystem
bxFileSystem::bxFileSystem()
	:_rootDir(0)
	,_rootDirLength(0)
{}

bxFileSystem::~bxFileSystem()
{
	SYS_ASSERT( _rootDir == 0 );
}

int bxFileSystem::startup( const char* rootDir )
{
	SYS_ASSERT( _rootDir == 0 );
	size_t rootDirLength = strlen( rootDir );

	SYS_ASSERT( rootDirLength > 0 );
	
	const char last_char = rootDir[rootDirLength-1];
	bool append_slash = last_char != '/';
	if( last_char == '\\' )
	{
		--rootDirLength;
		append_slash = true;
	}

	size_t toAlloc = rootDirLength + 1;
	if ( append_slash )
	{
		++toAlloc;
		++rootDirLength;
	}

    _rootDir = (char*)BX_MALLOC( bxDefaultAllocator(), toAlloc, 1 );
	_rootDirLength = (u32)rootDirLength;
	memset( _rootDir, 0, toAlloc );
	memcpy( _rootDir, rootDir, rootDirLength );
	
	if( append_slash )
	{
		_rootDir[rootDirLength-1] = '/';
	}

	return 0;
}

void bxFileSystem::shutdown()
{
    BX_FREE0( bxDefaultAllocator(), _rootDir );
    _rootDirLength = 0;
}

void bxFileSystem::absolutePath( bxFS::Path* absolutePath, const char* relativePath ) const
{
	const size_t relativePathLength = strlen( relativePath );
	SYS_ASSERT( ( relativePathLength + _rootDirLength ) <= Path::ePATH_LEN );
    sprintf_s( absolutePath->name, bxFS::Path::ePATH_LEN, "%s%s", _rootDir, relativePath );
    absolutePath->length = relativePathLength + _rootDirLength;
}

const bxFS::File bxFileSystem::readFile( const char* relativePath ) const
{
    bxFS::Path path;
    absolutePath( &path, relativePath );

    bxFS::File result;
    memset( &result, 0, sizeof(bxFS::File) );
    bxIO::readFile( &result.bin, &result.size, path.name );

	return result;
}

const bxFS::File bxFileSystem::readTextFile( const char* relativePath ) const
{
    bxFS::Path path;
    absolutePath( &path, relativePath );

    bxFS::File result;
    memset( &result, 0, sizeof(bxFS::File) );
	bxIO::readTextFile( &result.bin, &result.size, path.name );

	return result;
}

int bxFileSystem::writeFile( const char* relativePath, const unsigned char* data, size_t dataSize )
{
	Path path;
	absolutePath( &path, relativePath );

	const int result = bxIO::writeFile( path.name, (unsigned char*)data, dataSize );
	return result;
}

int bxFileSystem::createDir( const char* relativePath )
{
    Path path;
    absolutePath( &path, relativePath );

	return bxIO::createDir( path.name );
}

