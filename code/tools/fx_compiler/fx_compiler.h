#pragma once

#include <gdi/gdi_backend.h>
#include <gdi/gdi_shader_reflection.h>
#include <util/array.h>
#include <util/vector.h>
#include <string>

class bxFileSystem;

namespace fxTool
{
    struct ConfigPass
    {
        struct MacroDefine
        {
            const char* name;
            const char* def;
        };
        const char* name;
        const char* entry_points[bx::gdi::eDRAW_STAGES_COUNT];
        const char* versions[bx::gdi::eDRAW_STAGES_COUNT];

        bx::gdi::HardwareStateDesc hwstate;
        MacroDefine defs[bx::gdi::cMAX_SHADER_MACRO+1];
    };
    struct BinaryPass
    {
        BinaryPass();

        struct Blob
        {
            Blob()
            : ptr(0)
            , size(0)
            , __priv(0)
            {}
            void*   ptr;
            size_t  size;
            void* __priv;
        };

        std::string name;
        bx::gdi::HardwareStateDesc hwstate = {};
        bx::gdi::VertexLayout vertex_layout = {};

        Blob bytecode[bx::gdi::eDRAW_STAGES_COUNT];
        Blob disassembly[bx::gdi::eDRAW_STAGES_COUNT];

        bx::gdi::ShaderReflection reflection;
    };

    struct FxSourceDesc
    {
        void* _internal_data;
        vector_t< ConfigPass > passes;
    };

    struct FxBinaryDesc
    {
        vector_t< BinaryPass > passes;
    };
    
    //////////////////////////////////////////////////////////////////////////
    ///
    void release( FxSourceDesc* fx_source );
    void release( FxBinaryDesc* fx_bin );
    int parse_header( FxSourceDesc* fx_src, const char* source, int source_size );
    //////////////////////////////////////////////////////////////////////////
    class Compiler
    {
    public:
        static Compiler* create( int gdi_type );
        static void release( Compiler*& compiler );
        virtual ~Compiler() {}

        virtual int compile( FxBinaryDesc* fx_bin, const FxSourceDesc& fx_src, const char* source, bxFileSystem* fsys, int do_disasm = 0 ) = 0;
    };
    //////////////////////////////////////////////////////////////////////////
    /// for offline compilation
    int compile( const char* input_file_path, const char* output_dir );
}//
