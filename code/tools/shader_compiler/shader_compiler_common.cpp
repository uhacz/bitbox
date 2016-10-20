#include "shader_compiler_common.h"

#include <libconfig/libconfig.h>
#include "hardware_state_util.h"

namespace bx{ namespace tool{

static bool _ReadConfigInt( int* value, config_setting_t* cfg, const char* name )
{
    int ires = config_setting_lookup_int( cfg, name, value );
    const bool ok = ires == CONFIG_TRUE;
    if( !ok )
    {
        //print_error( "Invalid hwstate parameter: %s\n", name );
    }
    return ok;
}
static bool _ReadConfigString( const char** value, config_setting_t* cfg, const char* name )
{
    int ires = config_setting_lookup_string( cfg, name, value );
    const bool ok = ires == CONFIG_TRUE;
    if( !ok )
    {
        //print_error( "Invalid hwstate parameter: %s\n", name );
    }
    return ok;
}

static void _ExtractHardwareStateDesc( rdi::HardwareStateDesc* hwstate, config_setting_t* hwstate_setting )
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
            hwstate->blend.dstFactor = BlendFactor::FromString( sval );
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
        if( _ReadConfigInt( &ival, hwstate_setting, "scissor" ) )
            hwstate->raster.scissor = ival;
    }


}

static void _ExtractPasses( std::vector<ConfigPass>* out_passes, config_setting_t* cfg_passes )
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

        ConfigPass::MacroDefine defs[rdi::cMAX_SHADER_MACRO + 1];
        memset( defs, 0, sizeof( defs ) );
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

        config_setting_lookup_string( cfgpass, "vertex", &entry_points[EStage::VERTEX] );
        config_setting_lookup_string( cfgpass, "pixel", &entry_points[EStage::PIXEL] );

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

static int _ReadHeader( SourceShader* fx_source, char* source )
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



void Release( CompiledShader* fx_binary )
{
    for( int ipass = 0; ipass < (int)fx_binary->passes.size(); ++ipass )
    {
        for( int j = 0; j < EStage::DRAW_STAGES_COUNT; ++j )
        {
            Release( &fx_binary->passes[ipass].bytecode[j] );
            Release( &fx_binary->passes[ipass].disassembly[j] );
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

DataBlob CreateShaderBlob( const CompiledShader& compiled )
{
    const u32 num_passes = (u32)compiled.passes.size();

    u32 mem_size = 0;
    mem_size += sizeof( rdi::ShaderFile );
    mem_size += sizeof( rdi::ShaderFile::Pass ) * ( num_passes - 1 );

    u32 bytecode_size = 0;
    u32 resource_desc_size = 0;

    for( u32 ipass = 0; ipass < num_passes; ++ipass )
    {
        const BinaryPass& binPass = compiled.passes[ipass];
        for( u32 istage = 0; istage < EStage::DRAW_STAGES_COUNT; ++istage )
        {
            bytecode_size += (u32)binPass.bytecode[istage].size;
        }
        resource_desc_size += binPass.rdesc_mem_size;
    }
    mem_size += bytecode_size;

    void* mem = BX_MALLOC( bxDefaultAllocator(), mem_size, 4 );
    memset( mem, 0x00, mem_size );

    ShaderFile* shader_file = new(mem) ShaderFile();
    shader_file->num_passes = num_passes;

    u8* data_begin = (u8*)( shader_file + 1 ) + ( sizeof( ShaderFile::Pass ) * (num_passes - 1) );
    u8* data_current = data_begin;

    for( u32 ipass = 0; ipass < num_passes; ++ipass )
    {
        const BinaryPass& bin_pass = compiled.passes[ipass];
        ShaderFile::Pass& file_pass = shader_file->passes[ipass];

        file_pass.hashed_name = ShaderFileNameHash( bin_pass.name.c_str(), shader_file->version );
        file_pass.hw_state_desc = bin_pass.hwstate_desc;
        file_pass.vertex_layout = bin_pass.vertex_layout;
        
        memcpy( data_current, bin_pass.rdesc, bin_pass.rdesc_mem_size );
        file_pass.offset_resource_descriptor = TYPE_POINTER_GET_OFFSET( &file_pass.offset_resource_descriptor, data_current );
        file_pass.size_resource_descriptor = bin_pass.rdesc_mem_size;
        data_current += bin_pass.rdesc_mem_size;

        {
            const DataBlob blob = bin_pass.bytecode[EStage::VERTEX];
            memcpy( data_current, blob.ptr, blob.size );
            file_pass.offset_bytecode_vertex = TYPE_POINTER_GET_OFFSET( &file_pass.offset_bytecode_vertex, data_current );
            file_pass.size_bytecode_vertex = (u32)blob.size;
            data_current += blob.size;
        }
        {
            const DataBlob blob = bin_pass.bytecode[EStage::PIXEL];
            memcpy( data_current, blob.ptr, blob.size );
            file_pass.offset_bytecode_pixel = TYPE_POINTER_GET_OFFSET( &file_pass.offset_bytecode_pixel, data_current );
            file_pass.size_bytecode_pixel = (u32)blob.size;
            data_current += blob.size;
        }
    };

    SYS_ASSERT( (uptr)( (u8*)mem + mem_size ) == (uptr)( data_current ) );

    DataBlob blob = {};
    blob.ptr = mem;
    blob.size = mem_size;
    return blob;
}

}}///