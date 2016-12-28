#include "rdi_backend_dx11.h"
#include "rdi_shader_reflection.h"
//#include "DDSTextureLoader.h"
#include "DirectXTex/DirectXTex.h"

#include <util/memory.h>
#include <util/hash.h>
#include <util/common.h>
#include <util/array_util.h>
#include <D3Dcompiler.h>

namespace bx { namespace rdi {

ID3D11Device* g_device = nullptr;
CommandQueue* g_cmd_queue = nullptr;
void StartupDX11( uptr hWnd, int winWidth, int winHeight, int fullScreen )
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferCount = 1;
    sd.BufferDesc.Width = winWidth;
    sd.BufferDesc.Height = winHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = (HWND)hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = ( fullScreen ) ? FALSE : TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    //sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT flags = 0;//D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    IDXGISwapChain* dx11SwapChain = 0;
    ID3D11Device* dx11Dev = 0;
    ID3D11DeviceContext* dx11Ctx = 0;


    HRESULT hres = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        flags,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &dx11SwapChain,
        &dx11Dev,
        NULL,
        &dx11Ctx );

    if( FAILED( hres ) )
    {
        SYS_NOT_IMPLEMENTED;
    }

    g_device = dx11Dev;

    CommandQueue* cmd_queue = BX_NEW( bxDefaultAllocator(), CommandQueue );
    g_cmd_queue = cmd_queue;

    cmd_queue->_context = dx11Ctx;
    cmd_queue->_swapChain = dx11SwapChain;

    ID3D11Texture2D* backBuffer = NULL;
    hres = dx11SwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (LPVOID*)&backBuffer );
    SYS_ASSERT( SUCCEEDED( hres ) );

    ID3D11RenderTargetView* viewRT = 0;
    hres = dx11Dev->CreateRenderTargetView( backBuffer, NULL, &viewRT );
    SYS_ASSERT( SUCCEEDED( hres ) );
    cmd_queue->_mainFramebuffer = viewRT;

    D3D11_TEXTURE2D_DESC bbdesc;
    backBuffer->GetDesc( &bbdesc );
    cmd_queue->_mainFramebufferWidth = bbdesc.Width;
    cmd_queue->_mainFramebufferHeight = bbdesc.Height;

    backBuffer->Release();
}
void ShutdownDX11()
{
    g_cmd_queue->_mainFramebuffer->Release();
    g_cmd_queue->_context->Release();
    g_cmd_queue->_swapChain->Release();

    BX_DELETE0( bxDefaultAllocator(), g_cmd_queue );

    g_device->Release();
    g_device = nullptr;
}

void Dx11FetchShaderReflection( ShaderReflection* out, const void* code_blob, size_t code_blob_size, int stage )
{
    ID3D11ShaderReflection* reflector = NULL;
    D3DReflect( code_blob, code_blob_size, IID_ID3D11ShaderReflection, (void**)&reflector );

    D3D11_SHADER_DESC sdesc;
    reflector->GetDesc( &sdesc );

    //out->cbuffers.resize( sdesc.ConstantBuffers );

    for( u32 i = 0; i < sdesc.ConstantBuffers; ++i )
    {
        //ShaderCBufferDesc& cb = out->cbuffers[i];
        ID3D11ShaderReflectionConstantBuffer* cbuffer_reflector = reflector->GetConstantBufferByIndex( i );

        D3D11_SHADER_BUFFER_DESC sb_desc;
        cbuffer_reflector->GetDesc( &sb_desc );

        if( sb_desc.Name[0] == '_' )
            continue;

        D3D11_SHADER_INPUT_BIND_DESC bind_desc;
        reflector->GetResourceBindingDescByName( sb_desc.Name, &bind_desc );


        const u32 cb_hashed_name = simple_hash( sb_desc.Name );
        ShaderCBufferDesc* cbuffer_found = array::find( array::begin( out->cbuffers ), array::end( out->cbuffers ), ShaderReflection::FunctorCBuffer( cb_hashed_name ) );
        if( cbuffer_found != array::end( out->cbuffers ) )
        {
            cbuffer_found->stage_mask |= ( 1 << stage );
            continue;
        }

        out->cbuffers.push_back( ShaderCBufferDesc() );
        ShaderCBufferDesc& cb = out->cbuffers.back();
        cb.name = sb_desc.Name;
        cb.hashed_name = cb_hashed_name;
        cb.size = sb_desc.Size;
        cb.slot = bind_desc.BindPoint;
        cb.stage_mask = ( 1 << stage );
        //cb.variables.resize( sb_desc.Variables );
        //cb.num_variables = sb_desc.Variables;
        //cb.variables = (ShaderVariableDesc*)utils::memory_alloc( cb.num_variables * sizeof(ShaderVariableDesc) );

        for( u32 j = 0; j < sb_desc.Variables; ++j )
        {
            ID3D11ShaderReflectionVariable* var_reflector = cbuffer_reflector->GetVariableByIndex( j );
            ID3D11ShaderReflectionType* var_reflector_type = var_reflector->GetType();
            D3D11_SHADER_VARIABLE_DESC sv_desc;
            D3D11_SHADER_TYPE_DESC typeDesc;

            var_reflector->GetDesc( &sv_desc );
            var_reflector_type->GetDesc( &typeDesc );

            cb.variables.push_back( ShaderVariableDesc() );
            ShaderVariableDesc& vdesc = cb.variables.back();
            vdesc.name = sv_desc.Name;
            vdesc.hashed_name = simple_hash( sv_desc.Name );
            vdesc.offset = sv_desc.StartOffset;
            vdesc.size = sv_desc.Size;
            vdesc.type = EDataType::FindBaseType( typeDesc.Name );
        }
    }

    for( u32 i = 0; i < sdesc.BoundResources; ++i )
    {
        D3D11_SHADER_INPUT_BIND_DESC rdesc;
        reflector->GetResourceBindingDesc( i, &rdesc );

        /// system variable
        if( rdesc.Name[0] == '_' )
            continue;

        const u32 hashed_name = simple_hash( rdesc.Name );


        if( rdesc.Type == D3D_SIT_TEXTURE )
        {
            ShaderTextureDesc* found = array::find( array::begin( out->textures ), array::end( out->textures ), ShaderReflection::FunctorTexture( hashed_name ) );
            if( found != array::end( out->textures ) )
            {
                found->stage_mask |= ( 1 << stage );
                continue;
            }
            out->textures.push_back( ShaderTextureDesc() );
            ShaderTextureDesc& tdesc = out->textures.back();
            tdesc.name = rdesc.Name;
            tdesc.hashed_name = simple_hash( rdesc.Name );
            tdesc.slot = rdesc.BindPoint;
            tdesc.stage_mask = ( 1 << stage );
            switch( rdesc.Dimension )
            {
            case D3D_SRV_DIMENSION_TEXTURE1D:
                tdesc.dimm = 1;
                //SYS_ASSERT( false && "not implemented" );
                break;
            case D3D_SRV_DIMENSION_TEXTURE2D:
                tdesc.dimm = 2;
                break;
            case D3D_SRV_DIMENSION_TEXTURE3D:
                tdesc.dimm = 3;
                //SYS_ASSERT( false && "not implemented" );
                break;
            case D3D_SRV_DIMENSION_TEXTURECUBE:
                tdesc.dimm = 2;
                tdesc.is_cubemap = 1;
                //SYS_ASSERT( false && "not implemented" );
                break;
            default:
                SYS_ASSERT( false && "not implemented" );
                break;
            }
        }
        else if( rdesc.Type == D3D_SIT_SAMPLER )
        {
            ShaderSamplerDesc* found = array::find( array::begin( out->samplers ), array::end( out->samplers ), ShaderReflection::FunctorSampler( hashed_name ) );
            if( found != array::end( out->samplers ) )
            {
                found->stage_mask |= ( 1 << stage );
                continue;
            }
            out->samplers.push_back( ShaderSamplerDesc() );
            ShaderSamplerDesc& sdesc = out->samplers.back();
            sdesc.name = rdesc.Name;
            sdesc.hashed_name = simple_hash( rdesc.Name );
            sdesc.slot = rdesc.BindPoint;
            sdesc.stage_mask = ( 1 << stage );
        }
    }

    if( stage == EStage::VERTEX )
    {
        u16 input_mask = 0;
        VertexLayout& layout = out->vertex_layout;
        layout.count = 0;

        for( u32 i = 0; i < sdesc.InputParameters; ++i )
        {
            D3D11_SIGNATURE_PARAMETER_DESC idesc;
            reflector->GetInputParameterDesc( i, &idesc );

            EVertexSlot::Enum slot = EVertexSlot::FromString( idesc.SemanticName );
            if( slot >= EVertexSlot::COUNT )
            {
                //log_error( "Unknown semantic: '%s'", idesc.SemanticName );
                continue;
            }
            VertexBufferDesc& desc = layout.descs[layout.count++];

            input_mask |= ( 1 << ( slot + idesc.SemanticIndex ) );

            desc.slot = slot + idesc.SemanticIndex;
            desc.numElements = bitcount( (u32)idesc.Mask );
            desc.typeNorm = 0;
            if( idesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32 )
            {
                desc.dataType = EDataType::INT;
            }
            else if( idesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32 )
            {
                desc.dataType = EDataType::UINT;
            }
            else if( idesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32 )
            {
                desc.dataType = EDataType::FLOAT;
            }
            else
            {
                SYS_NOT_IMPLEMENTED;
            }
        }
        out->input_mask = input_mask;
    }
    reflector->Release();
}

}}///

namespace bx{ namespace rdi {
namespace frame
{

void Begin( CommandQueue** cmdQueue )
{
    cmdQueue[0] = g_cmd_queue;
}
void End( CommandQueue** cmdQueue )
{
    SYS_ASSERT( cmdQueue[0] == g_cmd_queue );
    cmdQueue[0] = nullptr;
}

}////

}}///


namespace bx{ namespace rdi {

namespace device
{
VertexBuffer CreateVertexBuffer  ( const VertexBufferDesc& desc, u32 numElements, const void* data )
{
    const u32 stride = desc.ByteWidth();
    //const u32 num_descs = desc.count;

    D3D11_BUFFER_DESC bdesc;
    memset( &bdesc, 0, sizeof( bdesc ) );
    bdesc.ByteWidth = numElements * stride;
    bdesc.Usage = ( data ) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = ( data ) ? 0 : D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA resource_data;
    resource_data.pSysMem = data;
    resource_data.SysMemPitch = 0;
    resource_data.SysMemSlicePitch = 0;

    D3D11_SUBRESOURCE_DATA* resource_data_pointer = ( data ) ? &resource_data : 0;

    ID3D11Buffer* buffer = 0;
    HRESULT hres = g_device->CreateBuffer( &bdesc, resource_data_pointer, &buffer );
    SYS_ASSERT( SUCCEEDED( hres ) );

    VertexBuffer vBuffer;
    vBuffer.buffer = buffer;
    vBuffer.numElements = numElements;
    vBuffer.desc = desc;
    return vBuffer;
}
IndexBuffer CreateIndexBuffer( EDataType::Enum dataType, u32 numElements, const void* data )
{
    const DXGI_FORMAT d11_data_type = to_DXGI_FORMAT( Format( dataType, 1 ) );
    const u32 stride = EDataType::stride[dataType];
    const u32 sizeInBytes = numElements * stride;

    D3D11_BUFFER_DESC bdesc;
    memset( &bdesc, 0, sizeof( bdesc ) );
    bdesc.ByteWidth = sizeInBytes;
    bdesc.Usage = ( data ) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
    bdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bdesc.CPUAccessFlags = ( data ) ? 0 : D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA resourceData;
    resourceData.pSysMem = data;
    resourceData.SysMemPitch = 0;
    resourceData.SysMemSlicePitch = 0;

    D3D11_SUBRESOURCE_DATA* resourceDataPointer = ( data ) ? &resourceData : 0;

    ID3D11Buffer* buffer = 0;
    HRESULT hres = g_device->CreateBuffer( &bdesc, resourceDataPointer, &buffer );
    SYS_ASSERT( SUCCEEDED( hres ) );

    IndexBuffer iBuffer;
    iBuffer.buffer = buffer;
    iBuffer.dataType = dataType;
    iBuffer.numElements = numElements;

    return iBuffer;
}
ConstantBuffer CreateConstantBuffer( u32 sizeInBytes, const void* data )
{
    D3D11_BUFFER_DESC bdesc;
    memset( &bdesc, 0, sizeof( bdesc ) );
    bdesc.ByteWidth = TYPE_ALIGN( sizeInBytes, 16 );
    bdesc.Usage = D3D11_USAGE_DEFAULT;
    bdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bdesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA resourceData;
    resourceData.pSysMem = data;
    resourceData.SysMemPitch = 0;
    resourceData.SysMemSlicePitch = 0;

    D3D11_SUBRESOURCE_DATA* resourceDataPointer = ( data ) ? &resourceData : 0;

    ID3D11Buffer* buffer = 0;
    HRESULT hres = g_device->CreateBuffer( &bdesc, resourceDataPointer, &buffer );
    SYS_ASSERT( SUCCEEDED( hres ) );

    ConstantBuffer b;
    b.buffer = buffer;
    b.size_in_bytes = sizeInBytes;
    return b;
}
BufferRO CreateBufferRO( int numElements, Format format, unsigned cpuAccessFlag, unsigned gpuAccessFlag )
{
    const u32 dxBindFlags = D3D11_BIND_SHADER_RESOURCE;
    const u32 dxCpuAccessFlag = to_D3D11_CPU_ACCESS_FLAG( cpuAccessFlag );

    D3D11_USAGE dxUsage = D3D11_USAGE_DEFAULT;
    if( gpuAccessFlag == EGpuAccess::READ && ( cpuAccessFlag & ECpuAccess::WRITE ) )
    {
        dxUsage = D3D11_USAGE_DYNAMIC;
    }

    const u32 formatByteWidth = format.ByteWidth();
    SYS_ASSERT( formatByteWidth > 0 );

    D3D11_BUFFER_DESC bdesc;
    memset( &bdesc, 0, sizeof( bdesc ) );
    bdesc.ByteWidth = numElements * formatByteWidth;
    bdesc.Usage = dxUsage;
    bdesc.BindFlags = dxBindFlags;
    bdesc.CPUAccessFlags = dxCpuAccessFlag;

    ID3D11Buffer* buffer = 0;
    HRESULT hres = g_device->CreateBuffer( &bdesc, 0, &buffer );
    SYS_ASSERT( SUCCEEDED( hres ) );

    ID3D11ShaderResourceView* viewSH = 0;

    //if( dxBindFlags & D3D11_BIND_SHADER_RESOURCE )
    {
        const DXGI_FORMAT dxFormat = to_DXGI_FORMAT( format );
        //const int formatByteWidth = formatByteWidth
        //SYS_ASSERT( formatByteWidth > 0 );

        D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
        memset( &srvdesc, 0, sizeof( srvdesc ) );
        srvdesc.Format = dxFormat;
        srvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srvdesc.BufferEx.FirstElement = 0;
        srvdesc.BufferEx.NumElements = numElements;
        srvdesc.BufferEx.Flags = 0;
        hres = g_device->CreateShaderResourceView( buffer, &srvdesc, &viewSH );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    BufferRO b;
    b.buffer = buffer;
    b.viewSH = viewSH;
    b.sizeInBytes = bdesc.ByteWidth;
    b.format = format;
    return b;
}

//Shader CreateShader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, ShaderReflection* reflection )
//{
//    SYS_ASSERT( stage < EStage::COUNT );
//    const char* shaderModel[EStage::COUNT] =
//    {
//        "vs_4_0",
//        "ps_4_0",
//        //"gs_4_0",
//        //"hs_5_0",
//        //"ds_5_0",
//        "cs_5_0"
//    };
//
//    D3D_SHADER_MACRO* ptr_macro_defs = 0;
//    D3D_SHADER_MACRO macro_defs_array[cMAX_SHADER_MACRO + 1];
//    memset( macro_defs_array, 0, sizeof( macro_defs_array ) );
//
//    if( shaderMacro )
//    {
//        const int n_macro = to_D3D_SHADER_MACRO_array( macro_defs_array, cMAX_SHADER_MACRO + 1, shaderMacro );
//        ptr_macro_defs = macro_defs_array;
//    }
//
//    ID3DBlob* code_blob = 0;
//    ID3DBlob* error_blob = 0;
//    HRESULT hr = D3DCompile(
//        shaderSource,
//        strlen( shaderSource ),
//        0,
//        ptr_macro_defs,
//        0,
//        entryPoint,
//        shaderModel[stage],
//        0,
//        0,
//        &code_blob,
//        &error_blob
//        );
//
//    if( !SUCCEEDED( hr ) )
//    {
//        const char* error_string = (const char*)error_blob->GetBufferPointer();
//
//        bxLogError( "Compile shader error:\n%s", error_string );
//        error_blob->Release();
//        SYS_ASSERT( false && "Shader compiler failed" );
//    }
//
//    Shader sh = CreateShader( stage, code_blob->GetBufferPointer(), code_blob->GetBufferSize(), reflection );
//    code_blob->Release();
//
//    return sh;
//}
//Shader CreateShader( int stage, const void* codeBlob, size_t codeBlobSize, ShaderReflection* reflection )
//{
//    Shader sh = {};
//
//    ID3DBlob* inputSignature = 0;
//
//    HRESULT hres;
//    switch( stage )
//    {
//    case EStage::VERTEX:
//        hres = g_device->CreateVertexShader( codeBlob, codeBlobSize, 0, &sh.vertex );
//        SYS_ASSERT( SUCCEEDED( hres ) );
//        hres = D3DGetInputSignatureBlob( codeBlob, codeBlobSize, &inputSignature );
//        SYS_ASSERT( SUCCEEDED( hres ) );
//        break;
//    case EStage::PIXEL:
//        hres = g_device->CreatePixelShader( codeBlob, codeBlobSize, 0, &sh.pixel );
//        break;
//    case EStage::COMPUTE:
//        hres = g_device->CreateComputeShader( codeBlob, codeBlobSize, 0, &sh.compute );
//        break;
//    default:
//        SYS_NOT_IMPLEMENTED;
//        break;
//    }
//
//    SYS_ASSERT( SUCCEEDED( hres ) );
//
//    if( reflection )
//    {
//        Dx11FetchShaderReflection( reflection, codeBlob, codeBlobSize, stage );
//        if( stage == EStage::VERTEX )
//        {
//            sh.vertexInputMask = reflection->input_mask;
//        }
//    }
//
//    sh.inputSignature = (void*)inputSignature;
//    sh.stage = stage;
//
//    return sh;
//}
ShaderPass device::CreateShaderPass( const ShaderPassCreateInfo& info )
{
    ShaderPass pass = {};

    HRESULT hres;
    if( info.vertex_bytecode && info.vertex_bytecode_size )
    {
        hres = g_device->CreateVertexShader( info.vertex_bytecode, info.vertex_bytecode_size, nullptr, &pass.vertex );
        SYS_ASSERT( SUCCEEDED( hres ) );

        ID3DBlob* blob = nullptr;
        hres = D3DGetInputSignatureBlob( info.vertex_bytecode, info.vertex_bytecode_size, &blob );
        SYS_ASSERT( SUCCEEDED( hres ) );
        pass.input_signature = (void*)blob;
        if( info.reflection )
        {
            Dx11FetchShaderReflection( info.reflection, info.vertex_bytecode, info.vertex_bytecode_size, EStage::VERTEX );
            pass.vertex_input_mask = info.reflection->input_mask;
        }
    }
    if( info.pixel_bytecode && info.pixel_bytecode_size )
    {
        hres = g_device->CreatePixelShader( info.pixel_bytecode, info.pixel_bytecode_size, nullptr, &pass.pixel );
        SYS_ASSERT( SUCCEEDED( hres ) );
        if( info.reflection )
        {
            Dx11FetchShaderReflection( info.reflection, info.pixel_bytecode, info.pixel_bytecode_size, EStage::PIXEL );
        }
    }
    return pass;
}

TextureRO CreateTextureFromDDS( const void* dataBlob, size_t dataBlobSize )
{
    ID3D11Resource* resource = 0;
    ID3D11ShaderResourceView* srv = 0;

    DirectX::ScratchImage scratch_img = {};
    DirectX::TexMetadata tex_metadata = {};

    HRESULT hres = DirectX::LoadFromDDSMemory( dataBlob, dataBlobSize, DirectX::DDS_FLAGS_NONE, &tex_metadata, scratch_img );
    SYS_ASSERT( SUCCEEDED( hres ) );
    
    TextureRO tex;
    hres = DirectX::CreateTexture( g_device, scratch_img.GetImages(), scratch_img.GetImageCount(), tex_metadata, &tex.resource );
    SYS_ASSERT( SUCCEEDED( hres ) );

    hres = DirectX::CreateShaderResourceView( g_device, scratch_img.GetImages(), scratch_img.GetImageCount(), tex_metadata, &tex.viewSH );
    SYS_ASSERT( SUCCEEDED( hres ) );

    tex.info.width  = (u16)tex_metadata.width;
    tex.info.height = (u16)tex_metadata.height;
    tex.info.depth  = (u16)tex_metadata.depth;
    tex.info.mips   = (u8)tex_metadata.mipLevels;
    
    return tex;
}
TextureRO device::CreateTextureFromHDR( const void* dataBlob, size_t dataBlobSize )
{
    ID3D11Resource* resource = 0;
    ID3D11ShaderResourceView* srv = 0;

    DirectX::ScratchImage scratch_img = {};
    DirectX::TexMetadata tex_metadata = {};

    HRESULT hres = DirectX::LoadFromHDRMemory( dataBlob, dataBlobSize, &tex_metadata, scratch_img );
    SYS_ASSERT( SUCCEEDED( hres ) );

    TextureRO tex;
    hres = DirectX::CreateTexture( g_device, scratch_img.GetImages(), scratch_img.GetImageCount(), tex_metadata, &tex.resource );
    SYS_ASSERT( SUCCEEDED( hres ) );

    hres = DirectX::CreateShaderResourceView( g_device, scratch_img.GetImages(), scratch_img.GetImageCount(), tex_metadata, &tex.viewSH );
    SYS_ASSERT( SUCCEEDED( hres ) );

    tex.info.width = (u16)tex_metadata.width;
    tex.info.height = (u16)tex_metadata.height;
    tex.info.depth = (u16)tex_metadata.depth;
    tex.info.mips = (u8)tex_metadata.mipLevels;
    return tex;
}


TextureRW CreateTexture1D( int w, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data )
{
    const DXGI_FORMAT dx_format = to_DXGI_FORMAT( format );
    const u32 dx_bind_flags = to_D3D11_BIND_FLAG( bindFlags );
    const u32 dx_cpua_flags = to_D3D11_CPU_ACCESS_FLAG( cpuaFlags );

    D3D11_TEXTURE1D_DESC desc;
    memset( &desc, 0, sizeof( desc ) );
    desc.Width = w;
    desc.MipLevels = mips;
    desc.ArraySize = 1;
    desc.Format = dx_format;

    //desc.SampleDesc.Count = 1;
    //desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = dx_bind_flags;
    desc.CPUAccessFlags = dx_cpua_flags;

    if( desc.MipLevels > 1 )
    {
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }
    D3D11_SUBRESOURCE_DATA* subResourcePtr = 0;
    D3D11_SUBRESOURCE_DATA subResource;
    memset( &subResource, 0, sizeof( D3D11_SUBRESOURCE_DATA ) );
    if( data )
    {
        subResource.pSysMem = data;
        subResource.SysMemPitch = w * format.ByteWidth();
        subResource.SysMemSlicePitch = 0;
        subResourcePtr = &subResource;
    }

    ID3D11Texture1D* tex1D = 0;
    HRESULT hres = g_device->CreateTexture1D( &desc, subResourcePtr, &tex1D );
    SYS_ASSERT( SUCCEEDED( hres ) );

    ID3D11ShaderResourceView* view_sh = 0;
    ID3D11RenderTargetView* view_rt = 0;
    ID3D11UnorderedAccessView* view_ua = 0;

    ID3D11RenderTargetView** rtv_mips = 0;

    if( dx_bind_flags & D3D11_BIND_SHADER_RESOURCE )
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
        memset( &srvdesc, 0, sizeof( srvdesc ) );
        srvdesc.Format = dx_format;
        srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
        srvdesc.Texture1D.MostDetailedMip = 0;
        srvdesc.Texture1D.MipLevels = desc.MipLevels;
        hres = g_device->CreateShaderResourceView( tex1D, &srvdesc, &view_sh );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    if( dx_bind_flags & D3D11_BIND_RENDER_TARGET )
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvdesc;
        memset( &rtvdesc, 0, sizeof( rtvdesc ) );
        rtvdesc.Format = dx_format;
        rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
        rtvdesc.Texture1D.MipSlice = 0;
        hres = g_device->CreateRenderTargetView( tex1D, &rtvdesc, &view_rt );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    if( dx_bind_flags & D3D11_BIND_DEPTH_STENCIL )
    {
        bxLogError( "Depth stencil binding is not supported without depth texture format" );
    }

    if( dx_bind_flags & D3D11_BIND_UNORDERED_ACCESS )
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc;
        memset( &uavdesc, 0, sizeof( uavdesc ) );
        uavdesc.Format = dx_format;
        uavdesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavdesc.Texture1D.MipSlice = 0;
        hres = g_device->CreateUnorderedAccessView( tex1D, &uavdesc, &view_ua );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    TextureRW tex;
    tex.texture1D = tex1D;
    tex.viewSH = view_sh;
    tex.viewUA = view_ua;
    tex.viewRT = view_rt;
    tex.info.width = w;
    tex.info.height = 1;
    tex.info.depth = 1;
    tex.info.mips = mips;
    tex.info.format = format;
    return tex;
}
TextureRW CreateTexture2D( int w, int h, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data )
{
    const DXGI_FORMAT dx_format = to_DXGI_FORMAT( format );
    const u32 dx_bind_flags = to_D3D11_BIND_FLAG( bindFlags );
    const u32 dx_cpua_flags = to_D3D11_CPU_ACCESS_FLAG( cpuaFlags );

    D3D11_TEXTURE2D_DESC desc;
    memset( &desc, 0, sizeof( desc ) );
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = mips;
    desc.ArraySize = 1;
    desc.Format = dx_format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = dx_bind_flags;
    desc.CPUAccessFlags = dx_cpua_flags;

    if( desc.MipLevels > 1 )
    {
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    D3D11_SUBRESOURCE_DATA* subResourcePtr = 0;
    D3D11_SUBRESOURCE_DATA subResource;
    memset( &subResource, 0, sizeof( D3D11_SUBRESOURCE_DATA ) );
    if( data )
    {
        subResource.pSysMem = data;
        subResource.SysMemPitch = w * format.ByteWidth();
        subResource.SysMemSlicePitch = 0;
        subResourcePtr = &subResource;
    }

    ID3D11Texture2D* tex2D = 0;
    HRESULT hres = g_device->CreateTexture2D( &desc, subResourcePtr, &tex2D );
    SYS_ASSERT( SUCCEEDED( hres ) );

    ID3D11ShaderResourceView* view_sh = 0;
    ID3D11RenderTargetView* view_rt = 0;
    ID3D11UnorderedAccessView* view_ua = 0;

    ID3D11RenderTargetView** rtv_mips = 0;

    if( dx_bind_flags & D3D11_BIND_SHADER_RESOURCE )
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
        memset( &srvdesc, 0, sizeof( srvdesc ) );
        srvdesc.Format = dx_format;
        srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvdesc.Texture2D.MostDetailedMip = 0;
        srvdesc.Texture2D.MipLevels = desc.MipLevels;
        hres = g_device->CreateShaderResourceView( tex2D, &srvdesc, &view_sh );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    if( dx_bind_flags & D3D11_BIND_RENDER_TARGET )
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvdesc;
        memset( &rtvdesc, 0, sizeof( rtvdesc ) );
        rtvdesc.Format = dx_format;
        rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvdesc.Texture2D.MipSlice = 0;
        hres = g_device->CreateRenderTargetView( tex2D, &rtvdesc, &view_rt );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    if( dx_bind_flags & D3D11_BIND_DEPTH_STENCIL )
    {
        bxLogError( "Depth stencil binding is not supported without depth texture format" );
    }

    if( dx_bind_flags & D3D11_BIND_UNORDERED_ACCESS )
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc;
        memset( &uavdesc, 0, sizeof( uavdesc ) );
        uavdesc.Format = dx_format;
        uavdesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavdesc.Texture2D.MipSlice = 0;
        hres = g_device->CreateUnorderedAccessView( tex2D, &uavdesc, &view_ua );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    TextureRW tex;
    tex.texture2D = tex2D;
    tex.viewSH = view_sh;
    tex.viewUA = view_ua;
    tex.viewRT = view_rt;
    tex.info.width = w;
    tex.info.height = h;
    tex.info.depth = 1;
    tex.info.mips = mips;
    tex.info.format = format;
    return tex;
}
TextureDepth CreateTexture2Ddepth( int w, int h, int mips, EDataType::Enum dataType )
{
    const DXGI_FORMAT dx_format = to_DXGI_FORMAT( Format( dataType, 1 ) );
    const u32 dx_bind_flags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE; // bx::gdi::to_D3D11_BIND_FLAG( bindFlags );

    SYS_ASSERT( isDepthFormat( dx_format ) );

    DXGI_FORMAT srv_format;
    DXGI_FORMAT tex_format;
    switch( dx_format )
    {
    case DXGI_FORMAT_D16_UNORM:
        tex_format = DXGI_FORMAT_R16_TYPELESS;
        srv_format = DXGI_FORMAT_R16_UNORM;
        break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        tex_format = DXGI_FORMAT_R24G8_TYPELESS;
        srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        break;
    case DXGI_FORMAT_D32_FLOAT:
        tex_format = DXGI_FORMAT_R32_TYPELESS;
        srv_format = DXGI_FORMAT_R32_FLOAT;
        break;
    default:
        SYS_ASSERT( false );
        break;
    }

    D3D11_TEXTURE2D_DESC desc;
    memset( &desc, 0, sizeof( desc ) );
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = mips;
    desc.ArraySize = 1;
    desc.Format = tex_format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = dx_bind_flags;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* tex2d = 0;
    HRESULT hres = g_device->CreateTexture2D( &desc, 0, &tex2d );
    SYS_ASSERT( SUCCEEDED( hres ) );

    ID3D11ShaderResourceView* view_sh = 0;
    ID3D11DepthStencilView* view_ds = 0;

    if( dx_bind_flags & D3D11_BIND_SHADER_RESOURCE )
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
        memset( &srvdesc, 0, sizeof( srvdesc ) );
        srvdesc.Format = srv_format;
        srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvdesc.Texture2D.MostDetailedMip = 0;
        srvdesc.Texture2D.MipLevels = mips;
        hres = g_device->CreateShaderResourceView( tex2d, &srvdesc, &view_sh );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    if( dx_bind_flags & D3D11_BIND_DEPTH_STENCIL )
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvdesc;
        memset( &dsvdesc, 0, sizeof( dsvdesc ) );
        dsvdesc.Format = dx_format;
        dsvdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvdesc.Texture2D.MipSlice = 0;
        hres = g_device->CreateDepthStencilView( tex2d, &dsvdesc, &view_ds );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    TextureDepth tex = {};
    tex.texture2D = tex2d;
    tex.viewDS = view_ds;
    tex.viewSH = view_sh;
    tex.info.width = w;
    tex.info.height = h;
    tex.info.depth = 1;
    tex.info.mips = mips;
    tex.info.format = Format( dataType, 1 );

    return tex;
}
Sampler CreateSampler( const SamplerDesc& desc )
{
    D3D11_SAMPLER_DESC dxDesc;
    dxDesc.Filter = ( desc.depthCmpMode == ESamplerDepthCmp::NONE ) ? filters[desc.filter] : comparisionFilters[desc.filter];
    dxDesc.ComparisonFunc = comparision[desc.depthCmpMode];

    dxDesc.AddressU = addressMode[desc.addressU];
    dxDesc.AddressV = addressMode[desc.addressV];
    dxDesc.AddressW = addressMode[desc.addressT];

    dxDesc.MaxAnisotropy = ( hasAniso( ( ESamplerFilter::Enum )desc.filter ) ) ? desc.aniso : 1;

    {
        dxDesc.BorderColor[0] = 0.f;
        dxDesc.BorderColor[1] = 0.f;
        dxDesc.BorderColor[2] = 0.f;
        dxDesc.BorderColor[3] = 0.f;
    }
    dxDesc.MipLODBias = 0.f;
    dxDesc.MinLOD = 0;
    dxDesc.MaxLOD = D3D11_FLOAT32_MAX;
    //desc.MaxLOD = ( SamplerFilter::has_mipmaps((SamplerFilter::Enum)state.filter) ) ? D3D11_FLOAT32_MAX : 0;

    ID3D11SamplerState* dxState = 0;
    HRESULT hres = g_device->CreateSamplerState( &dxDesc, &dxState );
    SYS_ASSERT( SUCCEEDED( hres ) );

    Sampler result;
    result.state = dxState;
    return result;
}

namespace 
{
    ID3D11InputLayout* _CreateInputLayoutInternal( const VertexBufferDesc* blocks, int nblocks, const void* signature )
    {
        const int MAX_IEDESCS = cMAX_VERTEX_BUFFERS;
        SYS_ASSERT( nblocks < MAX_IEDESCS );

        D3D11_INPUT_ELEMENT_DESC d3d_iedescs[MAX_IEDESCS];
        for( int iblock = 0; iblock < nblocks; ++iblock )
        {
            const VertexBufferDesc block = blocks[iblock];
            const Format format = Format( ( EDataType::Enum )block.dataType, block.numElements ).Normalized( block.typeNorm );
            D3D11_INPUT_ELEMENT_DESC& d3d_desc = d3d_iedescs[iblock];

            d3d_desc.SemanticName = EVertexSlot::name[block.slot];
            d3d_desc.SemanticIndex = EVertexSlot::semanticIndex[block.slot];
            d3d_desc.Format = to_DXGI_FORMAT( format );
            d3d_desc.InputSlot = iblock;
            d3d_desc.AlignedByteOffset = 0;
            d3d_desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            d3d_desc.InstanceDataStepRate = 0;
        }

        ID3D11InputLayout *dxLayout = 0;
        ID3DBlob* signatureBlob = (ID3DBlob*)signature;
        HRESULT hres = g_device->CreateInputLayout( d3d_iedescs, nblocks, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), &dxLayout );
        SYS_ASSERT( SUCCEEDED( hres ) );

        return dxLayout;
    }
}

InputLayout CreateInputLayout( const VertexBufferDesc* blocks, int nblocks, Shader vertexShader )
{
    SYS_ASSERT( vertexShader.stage == EStage::VERTEX );
    InputLayout iLay;
    iLay.layout = _CreateInputLayoutInternal( blocks, nblocks, vertexShader.inputSignature );
    return iLay;
}
InputLayout device::CreateInputLayout( const VertexLayout vertexLayout, ShaderPass shaderPass )
{
    InputLayout iLay;
    iLay.layout = _CreateInputLayoutInternal( vertexLayout.descs, vertexLayout.count, shaderPass.input_signature );
    return iLay;
}


//BlendState  CreateBlendState( HardwareStateDesc::Blend state )
//{
//    D3D11_BLEND_DESC bdesc;
//    memset( &bdesc, 0, sizeof( bdesc ) );
//
//    bdesc.AlphaToCoverageEnable = FALSE;
//    bdesc.IndependentBlendEnable = FALSE;
//    bdesc.RenderTarget[0].BlendEnable    = state.enable;
//    bdesc.RenderTarget[0].SrcBlend       = blendFactor[state.srcFactor];
//    bdesc.RenderTarget[0].DestBlend      = blendFactor[state.dstFactor];
//    bdesc.RenderTarget[0].BlendOp        = blendEquation[state.equation];
//    bdesc.RenderTarget[0].SrcBlendAlpha  = blendFactor[state.srcFactorAlpha];
//    bdesc.RenderTarget[0].DestBlendAlpha = blendFactor[state.dstFactorAlpha];
//    bdesc.RenderTarget[0].BlendOpAlpha   = blendEquation[state.equation];
//
//    u8 mask = 0;
//    mask |= ( state.color_mask & EColorMask::RED   ) ? D3D11_COLOR_WRITE_ENABLE_RED : 0;
//    mask |= ( state.color_mask & EColorMask::GREEN ) ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0;
//    mask |= ( state.color_mask & EColorMask::BLUE  ) ? D3D11_COLOR_WRITE_ENABLE_BLUE : 0;
//    mask |= ( state.color_mask & EColorMask::ALPHA ) ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0;
//
//    bdesc.RenderTarget[0].RenderTargetWriteMask = mask;
//
//    ID3D11BlendState* dx_state = 0;
//    HRESULT hres = g_device->CreateBlendState( &bdesc, &dx_state );
//    SYS_ASSERT( SUCCEEDED( hres ) );
//
//    BlendState result;
//    result.state = dx_state;
//    return result;
//}
//DepthState  CreateDepthState( HardwareStateDesc::Depth state )
//{
//    D3D11_DEPTH_STENCIL_DESC dsdesc;
//    memset( &dsdesc, 0, sizeof( dsdesc ) );
//
//    dsdesc.DepthEnable = state.test;
//    dsdesc.DepthWriteMask = ( state.write ) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
//    dsdesc.DepthFunc = depthCmpFunc[state.function];
//
//    dsdesc.StencilEnable = FALSE;
//    dsdesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
//    dsdesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
//
//    dsdesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
//    dsdesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
//    dsdesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
//    dsdesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
//
//    dsdesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
//    dsdesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
//    dsdesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
//    dsdesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
//
//    ID3D11DepthStencilState* dx_state = 0;
//    HRESULT hres = g_device->CreateDepthStencilState( &dsdesc, &dx_state );
//    SYS_ASSERT( SUCCEEDED( hres ) );
//
//    DepthState result;
//    result.state = dx_state;
//    return result;
//}
//RasterState CreateRasterState( HardwareStateDesc::Raster state )
//{
//    D3D11_RASTERIZER_DESC rdesc;
//    memset( &rdesc, 0, sizeof( rdesc ) );
//
//    rdesc.FillMode = fillMode[state.fillMode];
//    rdesc.CullMode = cullMode[state.cullMode];
//    rdesc.FrontCounterClockwise = TRUE;
//    rdesc.DepthBias = 0;
//    rdesc.SlopeScaledDepthBias = 0.f;
//    rdesc.DepthBiasClamp = 0.f;
//    rdesc.DepthClipEnable = TRUE;
//    rdesc.ScissorEnable = state.scissor;
//    rdesc.MultisampleEnable = FALSE;
//    rdesc.AntialiasedLineEnable = state.antialiasedLine;
//
//    ID3D11RasterizerState* dx_state = 0;
//    HRESULT hres = g_device->CreateRasterizerState( &rdesc, &dx_state );
//    SYS_ASSERT( SUCCEEDED( hres ) );
//
//    RasterState result;
//    result.state = dx_state;
//    return result;
//}

HardwareState device::CreateHardwareState( HardwareStateDesc desc )
{
    HardwareState hwstate = {};
    {
        HardwareStateDesc::Blend state = desc.blend;
        D3D11_BLEND_DESC bdesc;
        memset( &bdesc, 0, sizeof( bdesc ) );

        bdesc.AlphaToCoverageEnable = FALSE;
        bdesc.IndependentBlendEnable = FALSE;
        bdesc.RenderTarget[0].BlendEnable = state.enable;
        bdesc.RenderTarget[0].SrcBlend = blendFactor[state.srcFactor];
        bdesc.RenderTarget[0].DestBlend = blendFactor[state.dstFactor];
        bdesc.RenderTarget[0].BlendOp = blendEquation[state.equation];
        bdesc.RenderTarget[0].SrcBlendAlpha = blendFactor[state.srcFactorAlpha];
        bdesc.RenderTarget[0].DestBlendAlpha = blendFactor[state.dstFactorAlpha];
        bdesc.RenderTarget[0].BlendOpAlpha = blendEquation[state.equation];

        u8 mask = 0;
        mask |= ( state.color_mask & EColorMask::RED ) ? D3D11_COLOR_WRITE_ENABLE_RED : 0;
        mask |= ( state.color_mask & EColorMask::GREEN ) ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0;
        mask |= ( state.color_mask & EColorMask::BLUE ) ? D3D11_COLOR_WRITE_ENABLE_BLUE : 0;
        mask |= ( state.color_mask & EColorMask::ALPHA ) ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0;

        bdesc.RenderTarget[0].RenderTargetWriteMask = mask;

        ID3D11BlendState* dx_state = 0;
        HRESULT hres = g_device->CreateBlendState( &bdesc, &hwstate.blend );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    {
        HardwareStateDesc::Depth state = desc.depth;
        D3D11_DEPTH_STENCIL_DESC dsdesc;
        memset( &dsdesc, 0, sizeof( dsdesc ) );

        dsdesc.DepthEnable = state.test;
        dsdesc.DepthWriteMask = ( state.write ) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsdesc.DepthFunc = depthCmpFunc[state.function];

        dsdesc.StencilEnable = FALSE;
        dsdesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
        dsdesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

        dsdesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        dsdesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        dsdesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsdesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

        dsdesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        dsdesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        dsdesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsdesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

        ID3D11DepthStencilState* dx_state = 0;
        HRESULT hres = g_device->CreateDepthStencilState( &dsdesc, &hwstate.depth );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    {
        HardwareStateDesc::Raster state = desc.raster;
        D3D11_RASTERIZER_DESC rdesc;
        memset( &rdesc, 0, sizeof( rdesc ) );

        rdesc.FillMode = fillMode[state.fillMode];
        rdesc.CullMode = cullMode[state.cullMode];
        rdesc.FrontCounterClockwise = TRUE;
        rdesc.DepthBias = 0;
        rdesc.SlopeScaledDepthBias = 0.f;
        rdesc.DepthBiasClamp = 0.f;
        rdesc.DepthClipEnable = TRUE;
        rdesc.ScissorEnable = state.scissor;
        rdesc.MultisampleEnable = FALSE;
        rdesc.AntialiasedLineEnable = state.antialiasedLine;

        ID3D11RasterizerState* dx_state = 0;
        HRESULT hres = g_device->CreateRasterizerState( &rdesc, &hwstate.raster );
        SYS_ASSERT( SUCCEEDED( hres ) );
    }

    return hwstate;
}



template< class T >
static inline void releaseSafe( T*& obj )
{
    if( obj )
    {
        obj->Release();
        obj = nullptr;
    }
}

void DestroyVertexBuffer  ( VertexBuffer* id )
{
    releaseSafe( id->buffer );
}
void DestroyIndexBuffer( IndexBuffer* id )
{
    releaseSafe( id->buffer );
}
void DestroyInputLayout( InputLayout * id )
{
    releaseSafe( id->layout );
}
void DestroyConstantBuffer( ConstantBuffer* id )
{
    releaseSafe( id->buffer );
}
void DestroyBufferRO( BufferRO* id )
{
    releaseSafe( id->viewSH );
    releaseSafe( id->buffer );
}
void DestroyShader( Shader* id )
{
    releaseSafe( id->object );

    if( id->inputSignature )
    {
        ID3DBlob* blob = (ID3DBlob*)id->inputSignature;
        blob->Release();
        id->inputSignature = 0;
    }
}
void DestroyShaderPass( ShaderPass* id )
{
    releaseSafe( id->vertex );
    releaseSafe( id->pixel );
    {
        ID3DBlob* blob = (ID3DBlob*)id->input_signature;
        blob->Release();
        id->input_signature = nullptr;
    }
}

void DestroyTexture( TextureRO* id )
{
    releaseSafe( id->viewSH );
    releaseSafe( id->resource );
}
void DestroyTexture( TextureRW* id )
{
    releaseSafe( id->viewSH );
    releaseSafe( id->viewRT );
    releaseSafe( id->viewUA );
    releaseSafe( id->resource );
}
void DestroyTexture( TextureDepth* id )
{
    releaseSafe( id->viewDS );
    releaseSafe( id->viewSH );
    releaseSafe( id->viewUA );
    releaseSafe( id->resource );
}
void DestroySampler( Sampler* id )
{
    releaseSafe( id->state );
}
void DestroyBlendState( BlendState* id )
{
    releaseSafe( id->state );
}
void DestroyDepthState( DepthState* id )
{
    releaseSafe( id->state );
}
void DestroyRasterState( RasterState * id )
{
    releaseSafe( id->state );
}
void DestroyHardwareState( HardwareState* id )
{
    releaseSafe( id->raster );
    releaseSafe( id->depth );
    releaseSafe( id->blend );
}


}///

}}///


namespace bx { namespace rdi {

namespace context
{
void SetViewport( CommandQueue* cmdq, Viewport vp )
{
    D3D11_VIEWPORT dxVp;

    dxVp.Width = (FLOAT)vp.w;
    dxVp.Height = (FLOAT)vp.h;
    dxVp.MinDepth = 0.0f;
    dxVp.MaxDepth = 1.0f;
    dxVp.TopLeftX = (FLOAT)vp.x;
    dxVp.TopLeftY = (FLOAT)vp.y;

    cmdq->dx11()->RSSetViewports( 1, &dxVp );
}
void SetVertexBuffers( CommandQueue* cmdq, VertexBuffer* vbuffers, unsigned start, unsigned n )
{
    ID3D11Buffer* buffers[cMAX_VERTEX_BUFFERS];
    unsigned strides[cMAX_VERTEX_BUFFERS];
    unsigned offsets[cMAX_VERTEX_BUFFERS];
    memset( buffers, 0, sizeof( buffers ) );
    memset( strides, 0, sizeof( strides ) );
    memset( offsets, 0, sizeof( offsets ) );

    for( unsigned i = 0; i < n; ++i )
    {
        if( !vbuffers[i].buffer )
            continue;

        const VertexBuffer& buffer = vbuffers[i];
        strides[i] = buffer.desc.ByteWidth();
        buffers[i] = buffer.buffer;
    }
    cmdq->dx11()->IASetVertexBuffers( start, n, buffers, strides, offsets );
}
void SetIndexBuffer( CommandQueue* cmdq, IndexBuffer ibuffer )
{
    DXGI_FORMAT format = ( ibuffer.buffer ) ? to_DXGI_FORMAT( Format( (EDataType::Enum)ibuffer.dataType, 1 ) ) : DXGI_FORMAT_UNKNOWN;
    cmdq->dx11()->IASetIndexBuffer( ibuffer.buffer, format, 0 );
}
void SetShaderPrograms( CommandQueue* cmdq, Shader* shaders, int n )
{
    for( int i = 0; i < n; ++i )
    {
        Shader& shader = shaders[i];
        if( !shader.id )
            continue;

        switch( shader.stage )
        {
        case EStage::VERTEX:
            {
                cmdq->dx11()->VSSetShader( shader.vertex, 0, 0 );
                break;
            }
        case EStage::PIXEL:
            {
                cmdq->dx11()->PSSetShader( shader.pixel, 0, 0 );
                break;
            }
        case EStage::COMPUTE:
            {
                cmdq->dx11()->CSSetShader( shader.compute, 0, 0 );
                break;
            }
        default:
            {
                SYS_ASSERT( false );
                break;
            }
        }
    }
}
void SetShaderPass( CommandQueue* cmdq, ShaderPass pass )
{
    cmdq->dx11()->VSSetShader( pass.vertex, 0, 0 );
    cmdq->dx11()->PSSetShader( pass.pixel, 0, 0 );
}
void SetShader( CommandQueue* cmdq, Shader shader, EStage::Enum stage )
{
    switch( stage )
    {
    case EStage::VERTEX:
        cmdq->dx11()->VSSetShader( shader.vertex, nullptr, 0 );
        break;
    case EStage::PIXEL:
        cmdq->dx11()->PSSetShader( shader.pixel, nullptr, 0 );
        break;
    case EStage::COMPUTE:
        cmdq->dx11()->CSSetShader( shader.compute, nullptr, 0 );
        break;
    default:
        SYS_NOT_IMPLEMENTED;
    }
}


void SetInputLayout( CommandQueue* cmdq, InputLayout ilay )
{
    cmdq->dx11()->IASetInputLayout( ilay.layout );
}

void SetCbuffers( CommandQueue* cmdq, ConstantBuffer* cbuffers, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = cMAX_CBUFFERS;
    SYS_ASSERT( n <= SLOT_COUNT );
    ID3D11Buffer* buffers[SLOT_COUNT];
    memset( buffers, 0, sizeof( buffers ) );

    for( unsigned i = 0; i < n; ++i )
    {
        buffers[i] = cbuffers[i].buffer;
    }

    if( stageMask & EStage::VERTEX_MASK )
        cmdq->dx11()->VSSetConstantBuffers( startSlot, n, buffers );

    if( stageMask & EStage::PIXEL_MASK )
        cmdq->dx11()->PSSetConstantBuffers( startSlot, n, buffers );

    if( stageMask & EStage::COMPUTE_MASK )
        cmdq->dx11()->CSSetConstantBuffers( startSlot, n, buffers );
}
void SetResourcesRO( CommandQueue* cmdq, ResourceRO* resources, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = cMAX_RESOURCES_RO;
    SYS_ASSERT( n <= SLOT_COUNT );

    ID3D11ShaderResourceView* views[SLOT_COUNT];
    memset( views, 0, sizeof( views ) );

    if( resources )
    {
        for( unsigned i = 0; i < n; ++i )
        {
            views[i] = resources[i].viewSH;
        }
    }

    if( stageMask & EStage::VERTEX_MASK )
        cmdq->dx11()->VSSetShaderResources( startSlot, n, views );

    if( stageMask & EStage::PIXEL_MASK )
        cmdq->dx11()->PSSetShaderResources( startSlot, n, views );

    if( stageMask & EStage::COMPUTE_MASK )
        cmdq->dx11()->CSSetShaderResources( startSlot, n, views );
}
void SetResourcesRW( CommandQueue* cmdq, ResourceRW* resources, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = cMAX_RESOURCES_RW;
    SYS_ASSERT( n <= SLOT_COUNT );

    ID3D11UnorderedAccessView* views[SLOT_COUNT];
    UINT initial_count[SLOT_COUNT] = {};

    memset( views, 0, sizeof( views ) );

    if( resources )
    {
        for( unsigned i = 0; i < n; ++i )
        {
            views[i] = resources[i].viewUA;
        }
    }

    if( stageMask & EStage::VERTEX_MASK || EStage::PIXEL_MASK )
    {
        bxLogError( "ResourceRW can be set only in compute stage" );
    }

    if( stageMask & EStage::COMPUTE_MASK )
        cmdq->dx11()->CSSetUnorderedAccessViews( startSlot, n, views, initial_count );
}
void SetSamplers( CommandQueue* cmdq, Sampler* samplers, unsigned startSlot, unsigned n, unsigned stageMask )
{
    const int SLOT_COUNT = cMAX_SAMPLERS;
    SYS_ASSERT( n <= SLOT_COUNT );
    ID3D11SamplerState* resources[SLOT_COUNT];

    for( unsigned i = 0; i < n; ++i )
    {
        resources[i] = samplers[i].state;
    }

    if( stageMask & EStage::VERTEX_MASK )
        cmdq->dx11()->VSSetSamplers( startSlot, n, resources );

    if( stageMask & EStage::PIXEL_MASK )
        cmdq->dx11()->PSSetSamplers( startSlot, n, resources );

    if( stageMask & EStage::COMPUTE_MASK )
        cmdq->dx11()->CSSetSamplers( startSlot, n, resources );
}

void SetDepthState( CommandQueue* cmdq, DepthState state )
{
    cmdq->dx11()->OMSetDepthStencilState( state.state, 0 );
}
void SetBlendState( CommandQueue* cmdq, BlendState state )
{
    const float bfactor[4] = { 0.f, 0.f, 0.f, 0.f };
    cmdq->dx11()->OMSetBlendState( state.state, bfactor, 0xFFFFFFFF );
}
void SetRasterState( CommandQueue* cmdq, RasterState state )
{
    cmdq->dx11()->RSSetState( state.state );
}
void SetHardwareState( CommandQueue* cmdq, HardwareState hwstate )
{
    cmdq->dx11()->OMSetDepthStencilState( hwstate.depth, 0 );
    
    const float bfactor[4] = { 0.f, 0.f, 0.f, 0.f };
    cmdq->dx11()->OMSetBlendState( hwstate.blend, bfactor, 0xFFFFFFFF );
    
    cmdq->dx11()->RSSetState( hwstate.raster );
}


void SetScissorRects( CommandQueue* cmdq, const Rect* rects, int n )
{
    const int cMAX_RECTS = 4;
    D3D11_RECT dx11Rects[cMAX_RECTS];
    SYS_ASSERT( n <= cMAX_RECTS );

    for( int i = 0; i < cMAX_RECTS; ++i )
    {
        D3D11_RECT& r = dx11Rects[i];
        r.left = rects[i].left;
        r.top = rects[i].top;
        r.right = rects[i].right;
        r.bottom = rects[i].bottom;
    }

    cmdq->dx11()->RSSetScissorRects( n, dx11Rects );
}
void SetTopology( CommandQueue* cmdq, int topology )
{
    cmdq->dx11()->IASetPrimitiveTopology( topologyMap[topology] );
}

void ChangeToMainFramebuffer( CommandQueue* cmdq )
{
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    vp.Width = (float)cmdq->_mainFramebufferWidth;
    vp.Height = (float)cmdq->_mainFramebufferHeight;

    cmdq->dx11()->OMSetRenderTargets( 1, &cmdq->_mainFramebuffer, 0 );
    cmdq->dx11()->RSSetViewports( 1, &vp );
}
void ChangeRenderTargets( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex, bool changeViewport )
{
    const int SLOT_COUNT = cMAX_RENDER_TARGETS;
    SYS_ASSERT( nColor < SLOT_COUNT );

    ID3D11RenderTargetView *rtv[SLOT_COUNT] = {};
    ID3D11DepthStencilView *dsv = depthTex.viewDS;

    for( u32 i = 0; i < nColor; ++i )
    {
        rtv[i] = colorTex[i].viewRT;
    }

    cmdq->dx11()->OMSetRenderTargets( SLOT_COUNT, rtv, dsv );

    if( changeViewport )
    {
        Viewport vp;
        vp.x = 0;
        vp.y = 0;
        vp.w = ( colorTex ) ? colorTex->info.width  : depthTex.info.width;
        vp.h = ( colorTex ) ? colorTex->info.height : depthTex.info.height;
        SetViewport( cmdq, vp );
    }
}

unsigned char* _MapResource( ID3D11DeviceContext* ctx, ID3D11Resource* resource, D3D11_MAP dxMapType, int offsetInBytes )
{
    D3D11_MAPPED_SUBRESOURCE mappedRes;
    mappedRes.pData = NULL;
    mappedRes.RowPitch = 0;
    mappedRes.DepthPitch = 0;

    ctx->Map( resource, 0, dxMapType, 0, &mappedRes );
    return (u8*)mappedRes.pData + offsetInBytes;
}

unsigned char* Map( CommandQueue* cmdq, Resource resource, int offsetInBytes, int mapType )
{
    const D3D11_MAP dxMapType = ( mapType == EMapType::WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
    return _MapResource( cmdq->dx11(), resource.resource, dxMapType, offsetInBytes );
}
void Unmap( CommandQueue* cmdq, Resource resource )
{
    cmdq->dx11()->Unmap( resource.resource, 0 );
}

unsigned char* Map( CommandQueue* cmdq, VertexBuffer vbuffer, int firstElement, int numElements, int mapType )
{
    SYS_ASSERT( (u32)( firstElement + numElements ) <= vbuffer.numElements );

    const int offsetInBytes = firstElement * vbuffer.desc.ByteWidth();
    const D3D11_MAP dxMapType = ( mapType == EMapType::WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;

    return _MapResource( cmdq->dx11(), vbuffer.resource, dxMapType, offsetInBytes );
}
unsigned char* Map( CommandQueue* cmdq, IndexBuffer ibuffer, int firstElement, int numElements, int mapType )
{
    SYS_ASSERT( (u32)( firstElement + numElements ) <= ibuffer.numElements );
    SYS_ASSERT( ibuffer.dataType == EDataType::USHORT || ibuffer.dataType == EDataType::UINT );

    const int offsetInBytes = firstElement * EDataType::stride[ibuffer.dataType];
    const D3D11_MAP dxMapType = ( mapType == EMapType::WRITE ) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;

    return _MapResource( cmdq->dx11(), ibuffer.resource, dxMapType, offsetInBytes );
}

void UpdateCBuffer( CommandQueue* cmdq, ConstantBuffer cbuffer, const void* data )
{
    cmdq->dx11()->UpdateSubresource( cbuffer.resource, 0, NULL, data, 0, 0 );
}
void UpdateTexture( CommandQueue* cmdq, TextureRW texture, const void* data )
{
    const u32 formatWidth = texture.info.format.ByteWidth();
    const u32 srcRowPitch = formatWidth * texture.info.width;
    const u32 srcDepthPitch = srcRowPitch * texture.info.height;
    cmdq->dx11()->UpdateSubresource( texture.resource, 0, NULL, data, srcRowPitch, srcDepthPitch );
}

void Draw( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex )
{
    cmdq->dx11()->Draw( numVertices, startIndex );
}
void DrawIndexed( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned baseVertex )
{
    cmdq->dx11()->DrawIndexed( numIndices, startIndex, baseVertex );
}
void DrawInstanced( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex, unsigned numInstances )
{
    cmdq->dx11()->DrawInstanced( numVertices, numInstances, startIndex, 0 );
}
void DrawIndexedInstanced( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned numInstances, unsigned baseVertex )
{
    cmdq->dx11()->DrawIndexedInstanced( numIndices, numInstances, startIndex, baseVertex, 0 );
}


void ClearState( CommandQueue* cmdq )
{
    cmdq->dx11()->ClearState();
}
void ClearBuffers( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex, const float rgbad[5], int flag_color, int flag_depth )
{
    const int SLOT_COUNT = cMAX_RENDER_TARGETS;
    SYS_ASSERT( nColor < SLOT_COUNT );

    ID3D11RenderTargetView *rtv[SLOT_COUNT] = {};
    ID3D11DepthStencilView *dsv = depthTex.viewDS;
    for( u32 i = 0; i < nColor; ++i )
    {
        rtv[i] = colorTex[i].viewRT;
    }

    if( ( ( !colorTex || !nColor ) && !depthTex.resource ) && flag_color )
    {
        cmdq->dx11()->ClearRenderTargetView( cmdq->_mainFramebuffer, rgbad );
    }
    else
    {
        if( flag_depth && dsv )
        {
            cmdq->dx11()->ClearDepthStencilView( dsv, D3D11_CLEAR_DEPTH, rgbad[4], 0 );
        }

        if( flag_color && nColor )
        {
            for( unsigned i = 0; i < nColor; ++i )
            {
                cmdq->dx11()->ClearRenderTargetView( rtv[i], rgbad );
            }
        }
    }
}
void ClearDepthBuffer( CommandQueue* cmdq, TextureDepth depthTex, float clearValue )
{
    cmdq->dx11()->ClearDepthStencilView( depthTex.viewDS, D3D11_CLEAR_DEPTH, clearValue, 0 );
}
void ClearColorBuffers( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, float r, float g, float b, float a )
{
    const float rgba[4] = { r, g, b, a };

    const int SLOT_COUNT = cMAX_RENDER_TARGETS;
    SYS_ASSERT( nColor < SLOT_COUNT );
    
    for( unsigned i = 0; i < nColor; ++i )
    {
        cmdq->dx11()->ClearRenderTargetView( colorTex[i].viewRT, rgba );
    }

}




void Swap( CommandQueue* cmdq )
{
    cmdq->_swapChain->Present( 1, 0 );
}
void GenerateMipmaps( CommandQueue* cmdq, TextureRW texture )
{
    cmdq->dx11()->GenerateMips( texture.viewSH );
}
TextureRW GetBackBufferTexture( CommandQueue* cmdq )
{
    TextureRW tex;
    tex.id = 0;
    tex.viewRT = cmdq->_mainFramebuffer;
    tex.info.width = cmdq->_mainFramebufferWidth;
    tex.info.height = cmdq->_mainFramebufferHeight;

    return tex;
}
}///

}}///
