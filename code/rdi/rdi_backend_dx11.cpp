#include "rdi_backend_dx11.h"
#include "rdi_shader_reflection.h"

#include <util/memory.h>
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
    const u32 stride = typeStride[dataType];
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
ConstantBuffer CreateConstantBuffer( u32 sizeInBytes )
{
    D3D11_BUFFER_DESC bdesc;
    memset( &bdesc, 0, sizeof( bdesc ) );
    bdesc.ByteWidth = TYPE_ALIGN( sizeInBytes, 16 );
    bdesc.Usage = D3D11_USAGE_DEFAULT;
    bdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bdesc.CPUAccessFlags = 0;

    ID3D11Buffer* buffer = 0;
    HRESULT hres = g_device->CreateBuffer( &bdesc, 0, &buffer );
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

Shader CreateShader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, ShaderReflection* reflection )
{
    SYS_ASSERT( stage < EStage::COUNT );
    const char* shaderModel[EStage::COUNT] =
    {
        "vs_4_0",
        "ps_4_0",
        //"gs_4_0",
        //"hs_5_0",
        //"ds_5_0",
        "cs_5_0"
    };

    D3D_SHADER_MACRO* ptr_macro_defs = 0;
    D3D_SHADER_MACRO macro_defs_array[cMAX_SHADER_MACRO + 1];
    memset( macro_defs_array, 0, sizeof( macro_defs_array ) );

    if( shaderMacro )
    {
        const int n_macro = to_D3D_SHADER_MACRO_array( macro_defs_array, cMAX_SHADER_MACRO + 1, shaderMacro );
        ptr_macro_defs = macro_defs_array;
    }

    ID3DBlob* code_blob = 0;
    ID3DBlob* error_blob = 0;
    HRESULT hr = D3DCompile(
        shaderSource,
        strlen( shaderSource ),
        0,
        ptr_macro_defs,
        0,
        entryPoint,
        shaderModel[stage],
        0,
        0,
        &code_blob,
        &error_blob
        );

    if( !SUCCEEDED( hr ) )
    {
        const char* error_string = (const char*)error_blob->GetBufferPointer();

        bxLogError( "Compile shader error:\n%s", error_string );
        error_blob->Release();
        SYS_ASSERT( false && "Shader compiler failed" );
    }

    Shader sh = CreateShader( stage, code_blob->GetBufferPointer(), code_blob->GetBufferSize(), reflection );
    code_blob->Release();

    return sh;
}
Shader CreateShader( int stage, const void* codeBlob, size_t codeBlobSizee, ShaderReflection* reflection )
{
    Shader sh = {};

    ID3DBlob* inputSignature = 0;

    HRESULT hres;
    switch( stage )
    {
    case EStage::VERTEX:
        hres = g_device->CreateVertexShader( codeBlob, codeBlobSizee, 0, &sh.vertex );
        SYS_ASSERT( SUCCEEDED( hres ) );
        hres = D3DGetInputSignatureBlob( codeBlob, codeBlobSizee, &inputSignature );
        SYS_ASSERT( SUCCEEDED( hres ) );
        break;
    case EStage::PIXEL:
        hres = g_device->CreatePixelShader( codeBlob, codeBlobSizee, 0, &sh.pixel );
        break;
    case EStage::COMPUTE:
        hres = g_device->CreateComputeShader( codeBlob, codeBlobSizee, 0, &sh.compute );
        break;
    default:
        SYS_NOT_IMPLEMENTED;
        break;
    }

    SYS_ASSERT( SUCCEEDED( hres ) );

    if( reflection )
    {
        Dx11FetchShaderReflection( reflection, codeBlob, codeBlobSizee, stage );
        if( stage == EStage::VERTEX )
        {
            sh.vertexInputMask = reflection->input_mask;
        }
    }

    sh.inputSignature = (void*)inputSignature;
    sh.stage = stage;

    return sh;
}

TextureRO    CreateTexture       ( const void* dataBlob, size_t dataBlobSize );
TextureRW    CreateTexture1D     ( int w, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
TextureRW    CreateTexture2D     ( int w, int h, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
TextureDepth CreateTexture2Ddepth( int w, int h, int mips, EDataType::Enum dataType );
Sampler      CreateSampler       ( const SamplerDesc& desc );

InputLayout CreateInputLayout( const VertexBufferDesc* blocks, int nblocks, Shader vertex_shader );
BlendState  CreateBlendState( HardwareStateDesc::Blend blend );
DepthState  CreateDepthState( HardwareStateDesc::Depth depth );
RasterState CreateRasterState( HardwareStateDesc::Raster raster );

void DestroyVertexBuffer  ( VertexBuffer* id );
void DestroyIndexBuffer   ( IndexBuffer* id );
void DestroyInputLayout   ( InputLayout * id );
void DestroyConstantBuffer( ConstantBuffer* id );
void DestroyBufferRO      ( BufferRO* id );
void DestroyShader        ( Shader* id );
void DestroyTexture       ( TextureRO* id );
void DestroyTexture       ( TextureRW* id );
void DestroyTexture       ( TextureDepth* id );
void DestroySampler       ( Sampler* id );
void DestroyBlendState    ( BlendState* id );
void DestroyDepthState    ( DepthState* id );
void DestroyRasterState   ( RasterState * id );
}///


}}///
