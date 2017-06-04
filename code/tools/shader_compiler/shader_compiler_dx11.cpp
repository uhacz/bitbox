#include "shader_compiler_dx11.h"

namespace bx{namespace tool{

IncludeMap::IncludeMap()
{}

IncludeMap::~IncludeMap()
{
    for( std::map< std::string, bxFS::File >::iterator it = _file_map.begin(); it != _file_map.end(); ++it )
    {
        it->second.release();
    }
    _file_map.clear();
}

bxFS::File IncludeMap::get( const char* name )
{
    bxFS::File result;

    std::map< std::string, bxFS::File >::iterator found = _file_map.find( name );
    if( found != _file_map.end() )
        result = found->second;

    return result;
}

void IncludeMap::set( const char* name, const bxFS::File& file )
{
    _file_map[name] = file;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

dx11ShaderInclude::dx11ShaderInclude( bxFileSystem* fsys, IncludeMap* incmap ) 
    : _inc_map( incmap ), _fsys( fsys )
{

}

HRESULT __stdcall dx11ShaderInclude::Open( D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes )
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

HRESULT __stdcall dx11ShaderInclude::Close( LPCVOID pData )
{
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
ID3DBlob* dx11Compiler::_CompileShader( int stage, const char* shader_source, const char* entry_point, const char** shader_macro )
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

void dx11Compiler::_CreateResourceBindings( std::vector< ResourceBinding>* bindings, const ShaderReflection& reflection )
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

dx11Compiler::dx11Compiler()
{}

dx11Compiler::~dx11Compiler()
{
    BX_DELETE( bxDefaultAllocator(), _include_interface );
    BX_DELETE( bxDefaultAllocator(), _include_map );
}

int dx11Compiler::Compile( CompiledShader* fx_bin, const SourceShader& fx_src, const char* source, bxFileSystem* fsys, int do_disasm )
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

        print_info( "\tcompiling pass: %s ... \n", pass.name );

        BinaryPass bin_pass;
        for( int j = 0; j < EStage::DRAW_STAGES_COUNT; ++j )
        {
            if( !pass.entry_points[j] )
                continue;

            ID3DBlob* code_blob = _CompileShader( j, source, pass.entry_points[j], (const char**)pass.defs );
            ID3DBlob* code_disasm = NULL;

            if( !code_blob )
            {
                exit( -3 );
            }

            if( code_blob && do_disasm )
            {
                HRESULT hr = D3DDisassemble( code_blob->GetBufferPointer(), code_blob->GetBufferSize(), D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS | D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, NULL, &code_disasm );
                if( !SUCCEEDED( hr ) )
                {
                    print_error( "D3DDisassemble failed\n" );
                }
            }


            print_info( "\t\t%s shader: %s (%llu B)\n", rdi::EStage::name[j], pass.entry_points[j], code_blob->GetBufferSize() );

            Dx11FetchShaderReflection( &bin_pass.reflection, code_blob->GetBufferPointer(), code_blob->GetBufferSize(), j );

            bin_pass.bytecode[j] = to_Blob( code_blob );
            bin_pass.disassembly[j] = to_Blob( code_disasm );
            bin_pass.hwstate_desc = pass.hwstate;
            bin_pass.name = pass.name;
            error_counter += code_blob == NULL;
        }

        {
            std::vector< ResourceBinding > resource_bindings;
            _CreateResourceBindings( &resource_bindings, bin_pass.reflection );
            ResourceLayout resource_layout;
            resource_layout.bindings = resource_bindings.data();
            resource_layout.num_bindings = (u32)resource_bindings.size();
            bin_pass.rdesc = CreateResourceDescriptor( resource_layout );

            ResourceDescriptorMemoryRequirments rdesc_mem_req = CalculateResourceDescriptorMemoryRequirments( resource_layout );
            bin_pass.rdesc_mem_size = rdesc_mem_req.Total();
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
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

DataBlob to_Blob( ID3DBlob* blob )
{
    DataBlob b = {};
    if( blob )
    {
        b.ptr = blob->GetBufferPointer();
        b.size = blob->GetBufferSize();
        b.__priv = blob;
    }
    return b;
}

ID3DBlob* to_ID3DBlob( DataBlob blob )
{
    return (ID3DBlob*)blob.__priv;
}
void Release( DataBlob* blob )
{
    ID3DBlob* dx11_blob = to_ID3DBlob( *blob );
    if( dx11_blob )
    {
        dx11_blob->Release();
    }
    else
    {
        BX_FREE( bxDefaultAllocator(), blob->ptr );
    }


    blob[0] = {};
}

}}///
