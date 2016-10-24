#include <tools/shader_compiler/shader_compiler.h>

using namespace bx::tool;

int main( int argc, char** argv )
{
    const char in_file[] = "d:/dev/code/bitBox/code/shaders/hlsl/deffered.hlsl";
    const char out_dir[] = "d:/dev/code/bitBox/assets/shader/hlsl/";

    ShaderCompilerCompile( in_file, out_dir );


    return 0;
}


