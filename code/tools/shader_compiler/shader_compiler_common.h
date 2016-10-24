#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <map>

#include <util/filesystem.h>
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/hash.h>

#include <rdi/rdi.h>
#include <rdi/rdi_shader_reflection.h>

namespace bx{
using namespace rdi;

namespace tool{

    struct ConfigPass
    {
        struct MacroDefine
        {
            const char* name = nullptr;
            const char* def = nullptr;
        };
        const char* name = nullptr;
        const char* entry_points[rdi::EStage::DRAW_STAGES_COUNT] = {};
        const char* versions[rdi::EStage::DRAW_STAGES_COUNT] = {};

        rdi::HardwareStateDesc hwstate = {};
        MacroDefine defs[rdi::cMAX_SHADER_MACRO + 1] = {};
    };

    struct DataBlob
    {
        void*   ptr = nullptr;
        size_t  size = 0;
        void* __priv = nullptr;
    };

    struct BinaryPass
    {
        std::string name;
        rdi::HardwareStateDesc hwstate_desc = {};
        rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;
        u32 rdesc_mem_size = 0;
        
        DataBlob bytecode[rdi::EStage::DRAW_STAGES_COUNT] = {};
        DataBlob disassembly[rdi::EStage::DRAW_STAGES_COUNT] = {};

        ShaderReflection reflection = {};
    };

    struct SourceShader
    {
        void* _internal_data;
        std::vector< ConfigPass > passes;
    };

    struct CompiledShader
    {
        std::vector< BinaryPass > passes;
    };

    //////////////////////////////////////////////////////////////////////////
    ///
    void Release( DataBlob* blob );
    void Release( SourceShader* fx_source );
    void Release( CompiledShader* fx_bin );
    

    //////////////////////////////////////////////////////////////////////////
    ///
    int ParseHeader( SourceShader* fx_src, const char* source, int source_size );
    DataBlob CreateShaderBlob( const CompiledShader& compiled );


#define print_error( str, ... ) fprintf( stderr, str, __VA_ARGS__ )
#define print_info( str, ... ) fprintf( stdout, str, __VA_ARGS__ )

}}///
