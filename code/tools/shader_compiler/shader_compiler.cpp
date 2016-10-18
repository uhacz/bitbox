#include "shader_compiler.h"
#include "hardware_state_util.h"
#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <D3Dcompiler.h>

#include <libconfig/libconfig.h>
#include <string>
#include <vector>

#include <util/filesystem.h>
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/hash.h>

#include <rdi/rdi.h>

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
        rdi::HardwareState hwstate = {};
        rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;
        rdi::VertexLayout vertex_layout = {};

        Blob bytecode   [rdi::EStage::DRAW_STAGES_COUNT] = {};
        Blob disassembly[rdi::EStage::DRAW_STAGES_COUNT] = {};
    };

    struct FxSourceDesc
    {
        void* _internal_data;
        std::vector< ConfigPass > passes;
    };

    struct FxBinaryDesc
    {
        std::vector< BinaryPass > passes;
    };

    //////////////////////////////////////////////////////////////////////////
    ///
    void release( FxSourceDesc* fx_source );
    void release( FxBinaryDesc* fx_bin );
    int parse_header( FxSourceDesc* fx_src, const char* source, int source_size );

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

    bool _read_config_int( int* value, config_setting_t* cfg, const char* name )
    {
        int ires = config_setting_lookup_int( cfg, name, value );
        const bool ok = ires == CONFIG_TRUE;
        if( !ok )
        {
            //print_error( "Invalid hwstate parameter: %s\n", name );
        }
        return ok;
    }
    bool _read_config_string( const char** value, config_setting_t* cfg, const char* name )
    {
        int ires = config_setting_lookup_string( cfg, name, value );
        const bool ok = ires == CONFIG_TRUE;
        if( !ok )
        {
            //print_error( "Invalid hwstate parameter: %s\n", name );
        }
        return ok;
    }
    
    void _extract_hwstate( rdi::HardwareStateDesc* hwstate, config_setting_t* hwstate_setting )
    {
        //blend.enable = 0;
        //blend.color_mask = ColorMask::eALL;
        //blend.src_factor_alpha = BlendFactor::eONE;
        //blend.dst_factor_alpha = BlendFactor::eZERO;
        //blend.src_factor = BlendFactor::eONE;
        //blend.dst_factor = BlendFactor::eZERO;
        //blend.equation = BlendEquation::eADD;

        //depth.function = DepthFunc::eLEQUAL;
        //depth.test = 1;
        //depth.write = 1;

        //raster.cull_mode = Culling::eBACK;
        //raster.fill_mode = Fillmode::eSOLID;
        //raster.multisample = 0;
        //raster.antialiased_line = 0;
        //raster.scissor = 0;
        
        int ival = 0;
        const char* sval = 0;

        /// blend
        {
            if( _read_config_int( &ival, hwstate_setting, "blend_enable" ) )
                hwstate->blend.enable = ival;

            if( _read_config_string( &sval, hwstate_setting, "color_mask" ) )
                hwstate->blend.color_mask = ColorMask::FromString( sval );

            if( _read_config_string( &sval, hwstate_setting, "blend_src_factor" ) )
            {
                hwstate->blend.srcFactor = BlendFactor::FromString( sval );
                hwstate->blend.srcFactorAlpha = BlendFactor::FromString( sval );
            }
            if( _read_config_string( &sval, hwstate_setting, "blend_dst_factor" ) )
            {
                hwstate->blend.dstFactor       = BlendFactor::FromString( sval );
                hwstate->blend.dstFactorAlpha = BlendFactor::FromString( sval );
            }

            if( _read_config_string( &sval, hwstate_setting, "blend_equation" ) )
                hwstate->blend.equation = BlendEquation::FromString( sval );
        }
        
        /// depth
        {
            if( _read_config_string( &sval, hwstate_setting, "depth_function" ) )
                hwstate->depth.function = DepthFunc::FromString( sval );
            
            if( _read_config_int( &ival, hwstate_setting, "depth_test" ) )
                hwstate->depth.test = (u8)ival;
         
            if( _read_config_int( &ival, hwstate_setting, "depth_write" ) )
                hwstate->depth.write = (u8)ival;
        }

        /// raster
        {
            if( _read_config_string( &sval, hwstate_setting, "cull_mode" ) )
                hwstate->raster.cullMode = Culling::FromString( sval );

            if( _read_config_string( &sval, hwstate_setting, "fill_mode" ) )
                hwstate->raster.fillMode = Fillmode::FromString( sval );
            if ( _read_config_int( &ival, hwstate_setting, "scissor" ) )
                hwstate->raster.scissor = ival;
        }


    }

    void _extract_passes( std::vector<ConfigPass>* out_passes, config_setting_t* cfg_passes )
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
                _extract_hwstate( &hwstate, hwstate_setting );
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

    int _read_header( FxSourceDesc* fx_source, char* source )
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
            _extract_passes( &fx_source->passes, cfg_passes );        
        }
        else
        {
            print_error( "shader header parse error (line: %d) %s\n", config_error_line( header_config ), config_error_text( header_config ) );
            ires = -1;
        }

        return ires;
    }
}//

void release( FxBinaryDesc* fx_binary )
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
        }

        fx_binary->passes[ipass] = BinaryPass();

    }
}
void release( FxSourceDesc* fx_source )
{
    config_t* cfg = (config_t*)fx_source->_internal_data;
    config_destroy( cfg );
    BX_FREE0( bxDefaultAllocator(), cfg );
    fx_source->_internal_data = 0;
}
int parse_header( FxSourceDesc* fx_src, const char* source, int source_size )
{
    (void)source_size;
    return _read_header( fx_src, (char*)source );
}

int ShaderCompilerCompile( const char* inputFile, const char* outputDir )
{
    return 0;
}

}}///
