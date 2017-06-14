#include <tools/shader_compiler/shader_compiler.h>
#include <string>
#include <iostream>
#include <util/memory.h>
#include "cmdline.h"

using namespace bx::tool;

//#define TOOL_TEST

int main( int argc, char** argv )
{
    bx::memory::StartUp();
#ifndef TOOL_TEST
    if( argc < 2 )
    {
        std::cerr << "Invalid number of arguments" << std::endl;
        return -2;
    }

    const int n_initial_args = 3;

    cmdline::parser cmd_line;
    cmd_line.add< std::string >( "tool", 't', "tool name", true );
    cmd_line.add< std::string >( "infile", 'i', "input file", true );
    cmd_line.add< std::string >( "outdir", 'D', "output directory", true );
    cmd_line.parse_check( argc, argv );
    
    int result = 0;
    const std::string& tool_name = cmd_line.get<std::string>( "tool" );
    if( tool_name == "shader_compiler")
    {
        const std::string& in_file = cmd_line.get<std::string>("infile");
        const std::string& out_dir = cmd_line.get<std::string>("outdir");
        result = ShaderCompilerCompile( in_file.c_str(), out_dir.c_str() ); 
    }
#else
    //const char in_file[] = "d:/dev/code/bitBox/code/shaders/shaders/test.hlsl";
    //const char out_dir[] = "d:/dev/code/bitBox/assets/shader/hlsl/";

    const char in_file[] = "d:/tmp/bitBox/code/shaders/shaders/test.hlsl";
    const char out_dir[] = "d:/tmp/bitBox/assets/shader/hlsl/";

    int result = ShaderCompilerCompile( in_file, out_dir );
#endif

    bx::memory::ShutDown();
    return result;
}


