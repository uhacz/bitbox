#include "fx_compiler.h"
#include "hwstate_util.h"

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <D3Dcompiler.h>

#include <libconfig/libconfig.h>

#include <util/filesystem.h>
#include <util/memory.h>

#define print_error( str, ... ) fprintf( stderr, str, __VA_ARGS__ )
#define print_info( str, ... ) fprintf( stdout, str, __VA_ARGS__ )

using namespace fxTool;
using namespace bx;

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

    fxTool::BinaryPass::Blob to_Blob( ID3DBlob* blob )
    {
        fxTool::BinaryPass::Blob b;
        if( blob )
        {
            b.ptr = blob->GetBufferPointer();
            b.size = blob->GetBufferSize();
            b.__priv = blob;
        }
        return b;
    }
    ID3DBlob* to_ID3DBlob( fxTool::BinaryPass::Blob blob )
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
    
    void _extract_hwstate( bxGdiHwStateDesc* hwstate, config_setting_t* hwstate_setting )
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
                hwstate->blend.color_mask = ColorMask::fromString( sval );

            if( _read_config_string( &sval, hwstate_setting, "blend_src_factor" ) )
            {
                hwstate->blend.srcFactor = BlendFactor::fromString( sval );
                hwstate->blend.srcFactorAlpha = BlendFactor::fromString( sval );
            }
            if( _read_config_string( &sval, hwstate_setting, "blend_dst_factor" ) )
            {
                hwstate->blend.dstFactor       = BlendFactor::fromString( sval );
                hwstate->blend.dstFactorAlpha = BlendFactor::fromString( sval );
            }

            if( _read_config_string( &sval, hwstate_setting, "blend_equation" ) )
                hwstate->blend.equation = BlendEquation::fromString( sval );
        }
        
        /// depth
        {
            if( _read_config_string( &sval, hwstate_setting, "depth_function" ) )
                hwstate->depth.function = DepthFunc::fromString( sval );
            
            if( _read_config_int( &ival, hwstate_setting, "depth_test" ) )
                hwstate->depth.test = (u8)ival;
         
            if( _read_config_int( &ival, hwstate_setting, "depth_write" ) )
                hwstate->depth.write = (u8)ival;
        }

        /// raster
        {
            if( _read_config_string( &sval, hwstate_setting, "cull_mode" ) )
                hwstate->raster.cullMode = Culling::fromString( sval );

            if( _read_config_string( &sval, hwstate_setting, "fill_mode" ) )
                hwstate->raster.fillMode = Fillmode::fromString( sval );
            if ( _read_config_int( &ival, hwstate_setting, "scissor" ) )
                hwstate->raster.scissor = ival;
        }


    }

    void _extract_passes( vector_t<ConfigPass>* out_passes, config_setting_t* cfg_passes )
    {
        if( !cfg_passes )
            return;

        const int n_passes = config_setting_length( cfg_passes );    
        for( int i = 0; i < n_passes; ++i )
        {
            config_setting_t* cfgpass = config_setting_get_elem( cfg_passes, i );

            const char* entry_points[gdi::eDRAW_STAGES_COUNT] = 
            {
                0,
                0,
            };

            const char* versions[gdi::eDRAW_STAGES_COUNT] = 
            {
                "vs_5_0",
                "ps_5_0",
            };

            ConfigPass::MacroDefine defs[gdi::cMAX_SHADER_MACRO+1];
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
            SYS_ASSERT( n_defs < gdi::cMAX_SHADER_MACRO );
            {
                defs[n_defs].name = cfgpass->name;
                defs[n_defs].def = "1";
                ++n_defs;
            }

            bxGdiHwStateDesc hwstate;
            config_setting_t* hwstate_setting = config_setting_get_member( cfgpass, "hwstate" );
            if( hwstate_setting )
            {
                _extract_hwstate( &hwstate, hwstate_setting );
            }

            config_setting_lookup_string( cfgpass, "vertex"  , &entry_points[gdi::eSTAGE_VERTEX] );
            config_setting_lookup_string( cfgpass, "pixel"   , &entry_points[gdi::eSTAGE_PIXEL] );
            //config_setting_lookup_string( cfgpass, "geometry", &entry_points[gdi::eSTAGE_GEOMETRY] );
            //config_setting_lookup_string( cfgpass, "domain"  , &entry_points[gdi::eSTAGE_DOMAIN] );
            //config_setting_lookup_string( cfgpass, "hull"    , &entry_points[gdi::eSTAGE_HULL] );

            config_setting_lookup_string( cfgpass, "vs_ver", &versions[gdi::eSTAGE_VERTEX] );
            config_setting_lookup_string( cfgpass, "ps_ver", &versions[gdi::eSTAGE_PIXEL] );
            //config_setting_lookup_string( cfgpass, "gs_ver", &versions[gdi::eSTAGE_GEOMETRY] );
            //config_setting_lookup_string( cfgpass, "ds_ver", &versions[gdi::eSTAGE_DOMAIN] );
            //config_setting_lookup_string( cfgpass, "hs_ver", &versions[gdi::eSTAGE_HULL] );

            out_passes->push_back( ConfigPass() );
            ConfigPass& p = out_passes->back();
            memset( &p, 0, sizeof( ConfigPass ) );
            p.hwstate = hwstate;

            p.name = cfgpass->name;
            SYS_STATIC_ASSERT( sizeof( p.defs ) == sizeof( defs ) );
            memcpy( p.defs, defs, sizeof( p.defs ) );

            for( int i = 0; i < gdi::eDRAW_STAGES_COUNT; ++i )
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
            std::cerr << "no header found" << std::endl;
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
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace fxTool
{
    BinaryPass::BinaryPass()
    {
        memset( bytecode, 0, sizeof(bytecode) );
        memset( disassembly, 0, sizeof( disassembly ) );
    }

    void release( FxBinaryDesc* fx_binary )
    {
        for( int ipass = 0; ipass < (int)fx_binary->passes.size(); ++ipass )
        {
            for( int j = 0; j < gdi::eDRAW_STAGES_COUNT; ++j )
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
}//


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class FxCompilerHelper
{
public:
    int _parse_file_path( bxFileSystem* fsys, std::string* filename, const char* file_path )
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

    int startup( const char* in_file_path, const char* output_dir )
    {
        int ires = 0;

        ires = _parse_file_path( &_in_fs, &_in_filename, in_file_path );
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

    void shutdown()
    {
        _in_file.release();
        _out_fs.shutdown();
        _in_fs.shutdown();
    }

    int write( const FxBinaryDesc& fx_binary )
    {
        _out_fs.createDir( "bin" );
        _out_fs.createDir( "asm" );
        
        print_info( "file: %s\n", _in_filename.c_str() );
        for( int ipass = 0; ipass < (int)fx_binary.passes.size(); ++ipass )
        {
            const BinaryPass& bin_pass = fx_binary.passes[ipass];

            for( int j = 0; j < gdi::eDRAW_STAGES_COUNT; ++j )
		    {
                if( !bin_pass.bytecode[j].ptr )
                    continue;

                std::string out_filename_base;
                out_filename_base.append( _out_filename );
                out_filename_base.append( "." );
                out_filename_base.append( bin_pass.name );
                out_filename_base.append( "." );
                out_filename_base.append( gdi::stageName[j] );
                
                {
                    std::string out_filename;
                    out_filename.append( "bin/" );
                    out_filename.append( out_filename_base );
                    _out_fs.writeFile( out_filename.c_str(), (const u8*)bin_pass.bytecode[j].ptr, bin_pass.bytecode[j].size );
                }

                print_info( "\t%s\n", out_filename_base.c_str() );

                if( bin_pass.disassembly[j].ptr )
                {
                    std::string out_filename;
                    out_filename.append( "asm/" );
                    out_filename.append( out_filename_base );
                    out_filename.append( ".asm" );

                    _out_fs.writeFile( out_filename.c_str(), (const u8*)bin_pass.disassembly[j].ptr, bin_pass.disassembly[j].size );
                }
                {
                    bxFS::Path src_path;
                    bxFS::Path dst_path;

                    _in_fs.absolutePath( &src_path, _in_filename.c_str() );
                    _out_fs.absolutePath( &dst_path, _in_filename.c_str() );

                    bxIO::copyFile( dst_path.name, src_path.name );

                }
            }
        }
        
        print_info( "\n" );

        return 0;
    }

    FxCompilerHelper()    {}
    ~FxCompilerHelper()   {}

    bxFileSystem _in_fs;
    bxFileSystem _out_fs;

    std::string _in_filename;
    std::string _out_filename;

    bxFS::File _in_file;
};


namespace fxTool{

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
        if ( IncludeType != D3D10_INCLUDE_SYSTEM && IncludeType != D3D10_INCLUDE_LOCAL )
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

    HRESULT __stdcall Close(LPCVOID pData)
    {
        return S_OK;
    }

    IncludeMap* _inc_map;
    bxFileSystem* _fsys;
};

//////////////////////////////////////////////////////////////////////////

class dx11Compiler : public Compiler
{
    ID3DBlob* _compile_shader( int stage, const char* shader_source, const char* entry_point, const char** shader_macro )
    {
        SYS_ASSERT( stage < gdi::eSTAGE_COUNT );
        const char* shader_model[gdi::eSTAGE_COUNT] = 
        {
            "vs_5_0",
            "ps_5_0",
            //"gs_5_0",
            //"hs_5_0",
            //"ds_5_0",
            "cs_5_0"
        };

        D3D_SHADER_MACRO* ptr_macro_defs = 0;
        D3D_SHADER_MACRO macro_defs_array[gdi::cMAX_SHADER_MACRO+1];
        memset( macro_defs_array, 0, sizeof(macro_defs_array) );

        if( shader_macro )
        {
            const int n_macro = to_D3D_SHADER_MACRO_array( macro_defs_array, gdi::cMAX_SHADER_MACRO+1, shader_macro );
            ptr_macro_defs = macro_defs_array;
        }

        ID3DBlob* code_blob = 0;
        ID3DBlob* error_blob = 0;
        HRESULT hr = D3DCompile( 
            shader_source, 
            strlen(shader_source), 
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

        if( !SUCCEEDED(hr) )
        {
            const char* error_string = (const char*)error_blob->GetBufferPointer();
            print_error( "%s\n", error_string );
            error_blob->Release();
        }
        return code_blob;
    }

public:
    dx11Compiler()
        : _include_map(0)
        , _include_interface(0)
    {}
    virtual ~dx11Compiler()
    {
        BX_DELETE( bxDefaultAllocator(), _include_interface );
        BX_DELETE( bxDefaultAllocator(), _include_map );
    }

    virtual int compile( FxBinaryDesc* fx_bin, const FxSourceDesc& fx_src, const char* source, bxFileSystem* fsys, int do_disasm )
    {
        if( !_include_map )
            _include_map = BX_NEW( bxDefaultAllocator(), IncludeMap );

        if( !_include_interface )
            _include_interface = BX_NEW( bxDefaultAllocator(), dx11ShaderInclude, fsys, _include_map );

        int error_counter = 0;

        const int num_passes = (int)fx_src.passes.size();
        for( int ipass = 0 ; ipass < num_passes; ++ipass )
        {
            const ConfigPass& pass = fx_src.passes[ipass];

            print_info( "\tcompiling pass: %s ...\n", pass.name );

            BinaryPass bin_pass;
            for( int j = 0; j < gdi::eDRAW_STAGES_COUNT; ++j )
            {
                if( !pass.entry_points[j] )
                    continue;
                
                ID3DBlob* code_blob = _compile_shader( j, source, pass.entry_points[j], (const char**)pass.defs );
                ID3DBlob* code_disasm = NULL;

                if( code_blob && do_disasm )
                {
                    HRESULT hr = D3DDisassemble( code_blob->GetBufferPointer(), code_blob->GetBufferSize(), D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS | D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, NULL, &code_disasm );
                    if( !SUCCEEDED( hr ) )
                    {
                        print_error( "D3DDisassemble failed\n" );
                    }
                }

                bin_pass.bytecode[j] = to_Blob( code_blob );
                bin_pass.disassembly[j] = to_Blob( code_disasm );
                bin_pass.hwstate = pass.hwstate;
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

    IncludeMap* _include_map;
    dx11ShaderInclude* _include_interface;
};
//////////////////////////////////////////////////////////////////////////
Compiler* Compiler::create( int gdi_type )
{
    (void)gdi_type;
    return BX_NEW( bxDefaultAllocator(), dx11Compiler );
    
    //switch( gdi_type )
    //{
    //    case eDX11:
    //    {
    //        return MAKE_NEW( util::default_allocator(), dx11Compiler );
    //    }
    //    default:
    //        SYS_NOT_IMPLEMENTED;
    //}
    
    //return 0;
}
void Compiler::release( Compiler*& compiler )
{
    BX_DELETE( bxDefaultAllocator(), compiler );
    compiler = 0;
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int compile( const char* input_file_path, const char* output_dir )
{
    FxCompilerHelper helper;
    int ires = helper.startup( input_file_path, output_dir );

    print_info( "%s...\n", input_file_path );

    if( ires == 0 )
    {
        Compiler* compiler = Compiler::create( 0 );
        
        FxSourceDesc fx_src;
        FxBinaryDesc fx_bin;

        ires = parse_header( &fx_src, helper._in_file.txt, (int)helper._in_file.size );
        if( ires == 0 )
        {
            ires = compiler->compile( &fx_bin, fx_src, helper._in_file.txt, &helper._in_fs, 1 );
        }

        if( ires == 0 )
        {
            helper.write( fx_bin );
        }

        release( &fx_bin );
        release( &fx_src );
        Compiler::release( compiler );
    }

    helper.shutdown();

    return ires;

}

}//
