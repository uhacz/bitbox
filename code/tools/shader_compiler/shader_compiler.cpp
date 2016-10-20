#include "shader_compiler.h"
#include "hardware_state_util.h"
#include "shader_file_writer.h"
#include "shader_compiler_dx11.h"

namespace bx{ namespace tool{
int ShaderCompilerCompile( const char* inputFile, const char* outputDir )
{
    ShaderFileWriter writer;
    int ires = writer.StartUp( inputFile, outputDir );

    print_info( "%s...\n", inputFile );

    if( ires == 0 )
    {
        dx11Compiler compiler;

        SourceShader source_shader;
        CompiledShader compiled_shader;
        
        bxFS::File inputFile = writer.InputFile();

        ires = ParseHeader( &source_shader, inputFile.txt, (int)inputFile.size );
        if( ires == 0 )
        {
            ires = compiler.Compile( &compiled_shader, source_shader, inputFile.txt, writer.InputFilesystem(), 1 );
        }

        if( ires == 0 )
        {
            DataBlob shader_blob = CreateShaderBlob( compiled_shader );
            writer.WriteBinary( shader_blob.ptr, shader_blob.size );
            Release( &shader_blob );

            for( const BinaryPass& bpass : compiled_shader.passes )
            {
                for( u32 i = 0; i < EStage::DRAW_STAGES_COUNT; ++i )
                {
                    const char* stageName = EStage::name[i];
                    writer.WtiteAssembly( bpass.name.c_str(), stageName, bpass.disassembly[i].ptr, bpass.disassembly[i].size );
                }
            }
        }

        Release( &compiled_shader );
        Release( &source_shader );
    }

    writer.ShutDown();

    return ires;
    
    return 0;
}

}}///
