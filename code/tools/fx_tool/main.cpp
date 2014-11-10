#include <tools/fx_compiler/fx_compiler.h>
#include <iostream>

int main( int argc, char** argv )
{
    
    if( argc < 2 )
    {
        std::cerr << "invalid arguments!" << std::endl;
        std::cout << "usage: fx_compiler.exe [input_file] [output_dir (optional)]" << std::endl;
        return -1;
    }
    
    //const char* input_file = "d:/dev/code/bitBox/code/shaders/hlsl/test.hlsl";// argv[1];
    //const char* output_dir = "d:/dev/code/bitBox/assets/shader/hlsl/"; // ( argc > 2 ) ? argv[2] : 0;

    const char* input_file = argv[1];
    const char* output_dir = ( argc > 2 ) ? argv[2] : 0;

    
    int ires = fxTool::compile( input_file, output_dir );

    if( ires < 0 )
    {
        std::cerr << "Fx compile failed!" << std::endl;
    }

    //system( "PAUSE" );

    return ires;
}