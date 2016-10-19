#include "shader_compiler.h"
#include "hardware_state_util.h"
#include "shader_file_writer.h"
#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <D3Dcompiler.h>

#include <libconfig/libconfig.h>
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
#include <rdi/rdi_backend_dx11.h>


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
    struct BinaryPass
    {
        struct Blob
        {
            void*   ptr = nullptr;
            size_t  size = 0;
            void* __priv = nullptr;
        };

        std::string name;
        rdi::HardwareStateDesc hwstate_desc = {};
        rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;
        rdi::VertexLayout vertex_layout = {};

        Blob bytecode   [rdi::EStage::DRAW_STAGES_COUNT] = {};
        Blob disassembly[rdi::EStage::DRAW_STAGES_COUNT] = {};

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
    void Release( SourceShader* fx_source );
    void Release( CompiledShader* fx_bin );
    int ParseHeader( SourceShader* fx_src, const char* source, int source_size );

#define print_error( str, ... ) fprintf( stderr, str, __VA_ARGS__ )
#define print_info( str, ... ) fprintf( stdout, str, __VA_ARGS__ )

namespace
{
    int to_D3D_SHADER_MACRO_array( D3D_SHADER_MACRO* output, int output_capacity, const char** shader_macro )
    {
        int counter = 0;
        while( *shader_macro )
        {
            SYS_ASSERT( counter < output_capacity );
            output[counter].Name = shader_macro[0];
            output[counter].Definition = shader_macro[1];
            //print_info( "%s, ", shader_macro[0] );
            ++counter;
            shader_macro += 2;
        }

        //print_info( "\n" );

        return counter;
    }

    BinaryPass::Blob to_Blob( ID3DBlob* blob )
    {
        BinaryPass::Blob b;
        if( blob )
        {
            b.ptr = blob->GetBufferPointer();
            b.size = blob->GetBufferSize();
            b.__priv = blob;
        }
        return b;
    }
    ID3DBlob* to_ID3DBlob( BinaryPass::Blob blob )
    {
        return (ID3DBlob*)blob.__priv;
    }

    bool _ReadConfigInt( int* value, config_setting_t* cfg, const char* name )
    {
        int ires = config_setting_lookup_int( cfg, name, value );
        const bool ok = ires == CONFIG_TRUE;
        if( !ok )
        {
            //print_error( "Invalid hwstate parameter: %s\n", name );
        }
        return ok;
    }
    bool _ReadConfigString( const char** value, config_setting_t* cfg, const char* name )
    {
        int ires = config_setting_lookup_string( cfg, name, value );
        const bool ok = ires == CONFIG_TRUE;
        if( !ok )
        {
            //print_error( "Invalid hwstate parameter: %s\n", name );
        }
        return ok;
    }
    
    void _ExtractHardwareStateDesc( rdi::HardwareStateDesc* hwstate, config_setting_t* hwstate_setting )
    {
        int ival = 0;
        const char* sval = 0;

        /// blend
        {
            if( _ReadConfigInt( &ival, hwstate_setting, "blend_enable" ) )
                hwstate->blend.enable = ival;

            if( _ReadConfigString( &sval, hwstate_setting, "color_mask" ) )
                hwstate->blend.color_mask = ColorMask::FromString( sval );

            if( _ReadConfigString( &sval, hwstate_setting, "blend_src_factor" ) )
            {
                hwstate->blend.srcFactor = BlendFactor::FromString( sval );
                hwstate->blend.srcFactorAlpha = BlendFactor::FromString( sval );
            }
            if( _ReadConfigString( &sval, hwstate_setting, "blend_dst_factor" ) )
            {
                hwstate->blend.dstFactor       = BlendFactor::FromString( sval );
                hwstate->blend.dstFactorAlpha = BlendFactor::FromString( sval );
            }

            if( _ReadConfigString( &sval, hwstate_setting, "blend_equation" ) )
                hwstate->blend.equation = BlendEquation::FromString( sval );
        }
        
        /// depth
        {
            if( _ReadConfigString( &sval, hwstate_setting, "depth_function" ) )
                hwstate->depth.function = DepthFunc::FromString( sval );
            
            if( _ReadConfigInt( &ival, hwstate_setting, "depth_test" ) )
                hwstate->depth.test = (u8)ival;
         
            if( _ReadConfigInt( &ival, hwstate_setting, "depth_write" ) )
                hwstate->depth.write = (u8)ival;
        }

        /// raster
        {
            if( _ReadConfigString( &sval, hwstate_setting, "cull_mode" ) )
                hwstate->raster.cullMode = Culling::FromString( sval );

            if( _ReadConfigString( &sval, hwstate_setting, "fill_mode" ) )
                hwstate->raster.fillMode = Fillmode::FromString( sval );
            if ( _ReadConfigInt( &ival, hwstate_setting, "scissor" ) )
                hwstate->raster.scissor = ival;
        }


    }

    void _ExtractPasses( std::vector<ConfigPass>* out_passes, config_setting_t* cfg_passes )
    {
        if( !cfg_passes )
            return;

        const int n_passes = config_setting_length( cfg_passes );    
        for( int i = 0; i < n_passes; ++i )
        {
            config_setting_t* cfgpass = config_setting_get_elem( cfg_passes, i );

            const char* entry_points[EStage::DRAW_STAGES_COUNT] = 
            {
                nullptr,
                nullptr,
            };

            const char* versions[EStage::DRAW_STAGES_COUNT] =
            {
                "vs_5_0",
                "ps_5_0",
            };

            ConfigPass::MacroDefine defs[rdi::cMAX_SHADER_MACRO+1];
            memset( defs, 0 , sizeof(defs) );
            config_setting_t* macro_setting = config_setting_get_member( cfgpass, "define" );
            int n_defs = 0;
            if( macro_setting )
            {
                n_defs = config_setting_length( macro_setting );
                for( int idef = 0; idef < n_defs; ++idef )
                {
                    config_setting_t* def = config_setting_get_elem( macro_setting, idef );
                    defs[idef].name = config_setting_name( def );
                    defs[idef].def = config_setting_get_string( def );
                }
            }
            SYS_ASSERT( n_defs < rdi::cMAX_SHADER_MACRO );
            {
                defs[n_defs].name = cfgpass->name;
                defs[n_defs].def = "1";
                ++n_defs;
            }

            HardwareStateDesc hwstate;
            config_setting_t* hwstate_setting = config_setting_get_member( cfgpass, "hwstate" );
            if( hwstate_setting )
            {
                _ExtractHardwareStateDesc( &hwstate, hwstate_setting );
            }

            config_setting_lookup_string( cfgpass, "vertex"  , &entry_points[EStage::VERTEX] );
            config_setting_lookup_string( cfgpass, "pixel"   , &entry_points[EStage::PIXEL] );

            config_setting_lookup_string( cfgpass, "vs_ver", &versions[EStage::VERTEX] );
            config_setting_lookup_string( cfgpass, "ps_ver", &versions[EStage::PIXEL] );

            out_passes->push_back( ConfigPass() );
            ConfigPass& p = out_passes->back();
            memset( &p, 0, sizeof( ConfigPass ) );
            p.hwstate = hwstate;

            p.name = cfgpass->name;
            SYS_STATIC_ASSERT( sizeof( p.defs ) == sizeof( defs ) );
            memcpy( p.defs, defs, sizeof( p.defs ) );

            for( int i = 0; i < EStage::DRAW_STAGES_COUNT; ++i )
            {
                p.entry_points[i] = entry_points[i];
                p.versions[i] = versions[i];
            }
        }
    }

    int _ReadHeader( SourceShader* fx_source, char* source )
    {
        const char header_end[] = "#~header";
        const size_t header_end_len = strlen( header_end );

        char* source_code = (char*)strstr( source, header_end );
        if( !source_code )
        {
            print_error( "no header found\n" );
            return -1;
        }

        source_code += header_end_len;
        SYS_ASSERT( source_code > source );

        const size_t header_len = ( (uptr)source_code - (uptr)source ) - header_end_len;

        std::string header = std::string( source, header_len );

        source[0] = '/';
        source[1] = '*';
        source_code[-1] = '/';
        source_code[-2] = '*';

        //FxSourceDesc fx_source;
        config_t* header_config = BX_ALLOCATE( bxDefaultAllocator(), config_t ); // (config_t*)util::default_allocator()->allocate( sizeof( config_t ), sizeof( void* ) );
        fx_source->_internal_data = header_config;

        config_init( header_config );

        int ires = 0;

        const int cfg_result = config_read_string( header_config, header.c_str() );
        if( cfg_result == CONFIG_TRUE )
        {
            config_setting_t* cfg_passes = config_lookup( header_config, "passes" );
            _ExtractPasses( &fx_source->passes, cfg_passes );        
        }
        else
        {
            print_error( "shader header parse error (line: %d) %s\n", config_error_line( header_config ), config_error_text( header_config ) );
            ires = -1;
        }

        return ires;
    }
}//

void Release( CompiledShader* fx_binary )
{
    for( int ipass = 0; ipass < (int)fx_binary->passes.size(); ++ipass )
    {
        for( int j = 0; j < EStage::DRAW_STAGES_COUNT; ++j )
        {
            ID3DBlob* code_blob = to_ID3DBlob( fx_binary->passes[ipass].bytecode[j] );
            ID3DBlob* disasm_blob = to_ID3DBlob( fx_binary->passes[ipass].disassembly[j] );

            if( code_blob )
                code_blob->Release();

            if( disasm_blob )
                disasm_blob->Release();

            DestroyResourceDescriptor( &fx_binary->passes[ipass].rdesc );
        }

        fx_binary->passes[ipass] = BinaryPass();
    }
}
void Release( SourceShader* fx_source )
{
    config_t* cfg = (config_t*)fx_source->_internal_data;
    config_destroy( cfg );
    BX_FREE0( bxDefaultAllocator(), cfg );
    fx_source->_internal_data = 0;
}
int ParseHeader( SourceShader* fx_src, const char* source, int source_size )
{
    (void)source_size;
    return _ReadHeader( fx_src, (char*)source );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class IncludeMap
{
public:
    IncludeMap() {}

    ~IncludeMap()
    {
        for( std::map< std::string, bxFS::File >::iterator it = _file_map.begin(); it != _file_map.end(); ++it )
        {
            it->second.release();
        }
        _file_map.clear();
    }

    bxFS::File get( const char* name )
    {
        bxFS::File result;

        std::map< std::string, bxFS::File >::iterator found = _file_map.find( name );
        if( found != _file_map.end() )
            result = found->second;

        return result;
    }

    void set( const char* name, const bxFS::File& file )
    {
        _file_map[name] = file;
    }

private:

    std::map< std::string, bxFS::File > _file_map;
};
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class dx11ShaderInclude : public ID3DInclude
{
public:
    dx11ShaderInclude( bxFileSystem* fsys, IncludeMap* incmap )
        : _inc_map( incmap ), _fsys( fsys )
    {}

    HRESULT __stdcall Open( D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes )
    {
        if( IncludeType != D3D10_INCLUDE_SYSTEM && IncludeType != D3D10_INCLUDE_LOCAL )
        {
            print_error( "dx11ShaderInclude error: only system and local includes (<file>, not \"file\") are available! Include '%s' failed\n", pFileName );
            return S_FALSE;
        }

        bxFS::File file = _inc_map->get( pFileName );
        if( !file.ok() )
        {
            file = _fsys->readFile( pFileName );
        }

        if( file.ok() )
        {
            _inc_map->set( pFileName, file );
            *ppData = file.txt;
            *pBytes = (UINT)file.size;

            return S_OK;
        }
        else
        {
            print_error( "dx11ShaderInclude error: Couldn't open include '%s'. Note that both local includes are treated as system includes and must be relative to shader root\n", pFileName );
            return S_FALSE;
        }
    }

    HRESULT __stdcall Close( LPCVOID pData )
    {
        return S_OK;
    }

    IncludeMap* _inc_map;
    bxFileSystem* _fsys;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class dx11Compiler
{
    ID3DBlob* _CompileShader( int stage, const char* shader_source, const char* entry_point, const char** shader_macro )
    {
        SYS_ASSERT( stage < EStage::DRAW_STAGES_COUNT );
        const char* shader_model[EStage::DRAW_STAGES_COUNT] =
        {
            "vs_5_0",
            "ps_5_0",
        };

        D3D_SHADER_MACRO* ptr_macro_defs = 0;
        D3D_SHADER_MACRO macro_defs_array[cMAX_SHADER_MACRO + 1];
        memset( macro_defs_array, 0, sizeof( macro_defs_array ) );

        if( shader_macro )
        {
            const int n_macro = to_D3D_SHADER_MACRO_array( macro_defs_array, cMAX_SHADER_MACRO + 1, shader_macro );
            ptr_macro_defs = macro_defs_array;
        }

        ID3DBlob* code_blob = 0;
        ID3DBlob* error_blob = 0;
        HRESULT hr = D3DCompile(
            shader_source,
            strlen( shader_source ),
            NULL,
            ptr_macro_defs,
            _include_interface,
            entry_point,
            shader_model[stage],
            0,
            0,
            &code_blob,
            &error_blob
            );

        if( !SUCCEEDED( hr ) )
        {
            const char* error_string = (const char*)error_blob->GetBufferPointer();
            print_error( "%s\n", error_string );
            error_blob->Release();
        }
        return code_blob;
    }

    void _CreateResourceBindings( std::vector< ResourceBinding>* bindings, const ShaderReflection& reflection )
    {
        for( const ShaderCBufferDesc& desc : reflection.cbuffers )
        {
            ResourceBinding b( desc.name.c_str(), EBindingType::UNIFORM );
            b.Slot( desc.slot ).StageMask( desc.stage_mask );
            bindings->push_back( b );
        }
        for( const ShaderTextureDesc& desc : reflection.textures )
        {
            ResourceBinding b( desc.name.c_str(), EBindingType::READ_ONLY );
            b.Slot( desc.slot ).StageMask( desc.stage_mask );
            bindings->push_back( b );
        }
        for( const ShaderSamplerDesc& desc : reflection.samplers )
        {
            ResourceBinding b( desc.name.c_str(), EBindingType::SAMPLER );
            b.Slot( desc.slot ).StageMask( desc.stage_mask );
            bindings->push_back( b );
        }
    }

public:
    virtual ~dx11Compiler()
    {
        BX_DELETE( bxDefaultAllocator(), _include_interface );
        BX_DELETE( bxDefaultAllocator(), _include_map );
    }

    int Compile( CompiledShader* fx_bin, const SourceShader& fx_src, const char* source, bxFileSystem* fsys, int do_disasm )
    {
        if( !_include_map )
            _include_map = BX_NEW( bxDefaultAllocator(), IncludeMap );

        if( !_include_interface )
            _include_interface = BX_NEW( bxDefaultAllocator(), dx11ShaderInclude, fsys, _include_map );

        int error_counter = 0;

        const int num_passes = (int)fx_src.passes.size();
        for( int ipass = 0; ipass < num_passes; ++ipass )
        {
            const ConfigPass& pass = fx_src.passes[ipass];

            print_info( "\tcompiling pass: %s ...\n", pass.name );

            BinaryPass bin_pass;
            for( int j = 0; j < EStage::DRAW_STAGES_COUNT; ++j )
            {
                if( !pass.entry_points[j] )
                    continue;

                ID3DBlob* code_blob = _CompileShader( j, source, pass.entry_points[j], (const char**)pass.defs );
                ID3DBlob* code_disasm = NULL;

                if( code_blob && do_disasm )
                {
                    HRESULT hr = D3DDisassemble( code_blob->GetBufferPointer(), code_blob->GetBufferSize(), D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS | D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, NULL, &code_disasm );
                    if( !SUCCEEDED( hr ) )
                    {
                        print_error( "D3DDisassemble failed\n" );
                    }
                }
                Dx11FetchShaderReflection( &bin_pass.reflection, code_blob->GetBufferPointer(), code_blob->GetBufferSize(), j );
                
                std::vector< ResourceBinding > resource_bindings;
                _CreateResourceBindings( &resource_bindings, bin_pass.reflection );
                ResourceLayout resource_layout;
                resource_layout.bindings = resource_bindings.data();
                resource_layout.num_bindings = (u32)resource_bindings.size();
                bin_pass.rdesc = CreateResourceDescriptor( resource_layout );

                bin_pass.bytecode[j] = to_Blob( code_blob );
                bin_pass.disassembly[j] = to_Blob( code_disasm );
                bin_pass.hwstate_desc = pass.hwstate;
                bin_pass.name = pass.name;
                error_counter += code_blob == NULL;
            }

            fx_bin->passes.push_back( bin_pass );
        }

        int ires = 0;
        if( error_counter > 0 )
        {
            ires = -1;
        }

        return ires;
    }

    IncludeMap* _include_map = nullptr;
    dx11ShaderInclude* _include_interface = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
            //helper.write( fx_bin );
            //helper.writeShaderModule( fx_bin );
        }

        Release( &compiled_shader );
        Release( &source_shader );
    }

    writer.ShutDown();

    return ires;
    
    return 0;
}

}}///
