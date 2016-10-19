#pragma once

#include <util/filesystem.h>
#include <string>

namespace bx{ namespace tool{

class ShaderFileWriter
{
public:
    int StartUp( const char* in_file_path, const char* output_dir );
    void ShutDown();

    int WriteBinary( const void* data, size_t dataSize );
    int WtiteAssembly( const char* passName, const char* stageName, const void* data, size_t dataSize );

    bxFS::File InputFile() { return _in_file; }
    bxFileSystem* InputFilesystem() { return &_in_fs; }

private:
    int _ParseFilePath( bxFileSystem* fsys, std::string* filename, const char* file_path );

private:
    bxFileSystem _in_fs;
    bxFileSystem _out_fs;

    std::string _in_filename;
    std::string _out_filename;

    bxFS::File _in_file;
};

}}///
