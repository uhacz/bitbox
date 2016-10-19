#include "shader_file_writer.h"
#include <iostream>


namespace bx{ namespace tool{

int ShaderFileWriter::StartUp( const char* in_file_path, const char* output_dir )
{
    int ires = 0;

    ires = _ParseFilePath( &_in_fs, &_in_filename, in_file_path );
    if( ires < 0 )
        return ires;

    _out_filename = _in_filename;
    _out_filename.erase( _out_filename.find_last_of( '.' ) );

    if( output_dir )
    {
        _out_fs.startup( output_dir );
    }
    else
    {
        _out_fs.startup( _in_fs.rootDir() );
    }

    _in_file = _in_fs.readTextFile( _in_filename.c_str() );
    if( !_in_file.ok() )
    {
        std::cerr << "cannot read file: " << _in_filename << std::endl;
        return -1;
    }

    return 0;
}

void ShaderFileWriter::ShutDown()
{
    _in_file.release();
    _out_fs.shutdown();
    _in_fs.shutdown();
}

int ShaderFileWriter::WriteBinary( const void* data, size_t dataSize )
{
    _out_fs.createDir( "bin" );

    std::string out_filename;
    out_filename.append( "bin/" );
    out_filename.append( _out_filename );
    out_filename.append( ".shader" );

    return _out_fs.writeFile( out_filename.c_str(), (const unsigned char*)data, dataSize );
}

int ShaderFileWriter::WtiteAssembly( const char* passName, const char* stageName, const void* data, size_t dataSize )
{
    _out_fs.createDir( "asm" );

    std::string out_filename_base;
    out_filename_base.append( _out_filename );
    out_filename_base.append( "." );
    out_filename_base.append( passName );
    out_filename_base.append( "." );
    out_filename_base.append( stageName );

    std::string out_filename;
    out_filename.append( "asm/" );
    out_filename.append( out_filename_base );
    out_filename.append( ".asm" );

    return _out_fs.writeFile( out_filename.c_str(), (const unsigned char*)data, dataSize );
}

//////////////////////////////////////////////////////////////////////////
int ShaderFileWriter::_ParseFilePath( bxFileSystem* fsys, std::string* filename, const char* file_path )
{
    std::string file_str( file_path );

    for( std::string::iterator it = file_str.begin(); it != file_str.end(); ++it )
    {
        if( *it == '\\' )
            *it = '/';
    }

    const size_t slash_pos = file_str.find_last_of( '/' );
    if( slash_pos == std::string::npos )
    {
        std::cerr << "input path not valid\n";
        return -1;
    }

    std::string root_dir = file_str.substr( 0, slash_pos + 1 );
    std::string file_name = file_str.substr( slash_pos + 1 );

    int ires = fsys->startup( root_dir.c_str() );
    if( ires < 0 )
    {
        std::cerr << "root dir not valid\n";
        return -1;
    }

    filename->assign( file_name );

    return 0;
}

}
}///
