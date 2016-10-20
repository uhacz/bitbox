#pragma once

#include <map>
#include <string>
#include <util/filesystem.h>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <D3Dcompiler.h>

#include <rdi/rdi_backend_dx11.h>

#include "shader_compiler_common.h"


namespace bx{ namespace tool{

class IncludeMap
{
public:
    IncludeMap();
    ~IncludeMap();

    bxFS::File get( const char* name );
    void set( const char* name, const bxFS::File& file );

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
    dx11ShaderInclude( bxFileSystem* fsys, IncludeMap* incmap );

    HRESULT __stdcall Open( D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes );
    HRESULT __stdcall Close( LPCVOID pData );

    IncludeMap* _inc_map;
    bxFileSystem* _fsys;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class dx11Compiler
{
    ID3DBlob* _CompileShader( int stage, const char* shader_source, const char* entry_point, const char** shader_macro );
    void _CreateResourceBindings( std::vector< ResourceBinding>* bindings, const ShaderReflection& reflection );

public:
    dx11Compiler();
    ~dx11Compiler();

    int Compile( CompiledShader* fx_bin, const SourceShader& fx_src, const char* source, bxFileSystem* fsys, int do_disasm );

    IncludeMap* _include_map = nullptr;
    dx11ShaderInclude* _include_interface = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int to_D3D_SHADER_MACRO_array( D3D_SHADER_MACRO* output, int output_capacity, const char** shader_macro );
DataBlob to_Blob( ID3DBlob* blob );
ID3DBlob* to_ID3DBlob( DataBlob blob );

}}///