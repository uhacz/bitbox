#pragma once

#include <dxgi.h>
#include <d3d11.h>
#include "rdi_backend.h"

#include <util/debug.h>

namespace bx{ namespace rdi {

static const D3D11_FILL_MODE fillMode[] =
{
    D3D11_FILL_SOLID,
    D3D11_FILL_WIREFRAME,
};
static const D3D11_CULL_MODE cullMode[] =
{
    D3D11_CULL_NONE,
    D3D11_CULL_BACK,
    D3D11_CULL_FRONT,
};
static const D3D11_COMPARISON_FUNC depthCmpFunc[] =
{
    D3D11_COMPARISON_NEVER,
    D3D11_COMPARISON_LESS,
    D3D11_COMPARISON_EQUAL,
    D3D11_COMPARISON_LESS_EQUAL,
    D3D11_COMPARISON_GREATER,
    D3D11_COMPARISON_NOT_EQUAL,
    D3D11_COMPARISON_GREATER_EQUAL,
    D3D11_COMPARISON_ALWAYS,
};
static const D3D11_BLEND blendFactor[] =
{
    D3D11_BLEND_ZERO,
    D3D11_BLEND_ONE,
    D3D11_BLEND_SRC_COLOR,
    D3D11_BLEND_INV_SRC_COLOR,
    D3D11_BLEND_DEST_COLOR,
    D3D11_BLEND_INV_DEST_COLOR,
    D3D11_BLEND_SRC_ALPHA,
    D3D11_BLEND_INV_SRC_ALPHA,
    D3D11_BLEND_DEST_ALPHA,
    D3D11_BLEND_INV_DEST_ALPHA,
    D3D11_BLEND_SRC_ALPHA_SAT,
};

static const D3D11_BLEND_OP blendEquation[] =
{
    D3D11_BLEND_OP_ADD,
    D3D11_BLEND_OP_SUBTRACT,
    D3D11_BLEND_OP_REV_SUBTRACT,
    D3D11_BLEND_OP_MIN,
    D3D11_BLEND_OP_MAX,
};

static const D3D11_TEXTURE_ADDRESS_MODE addressMode[] =
{
    D3D11_TEXTURE_ADDRESS_WRAP,
    D3D11_TEXTURE_ADDRESS_CLAMP,
    D3D11_TEXTURE_ADDRESS_BORDER,
};
static const D3D11_FILTER filters[] =
{
    D3D11_FILTER_MIN_MAG_MIP_POINT,
    D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
    D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
    D3D11_FILTER_MIN_MAG_MIP_LINEAR,
    D3D11_FILTER_ANISOTROPIC,
    D3D11_FILTER_ANISOTROPIC,
};

static const D3D11_FILTER comparisionFilters[] =
{
    D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
    D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
    D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
    D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
    D3D11_FILTER_COMPARISON_ANISOTROPIC,
    D3D11_FILTER_COMPARISON_ANISOTROPIC,
};

static const D3D11_COMPARISON_FUNC comparision[] =
{
    D3D11_COMPARISON_ALWAYS, //eNONE = 0,
    D3D11_COMPARISON_GREATER, //eGREATER,
    D3D11_COMPARISON_GREATER_EQUAL, //eGEQUAL,
    D3D11_COMPARISON_LESS, //eLESS,
    D3D11_COMPARISON_LESS_EQUAL, //eLEQUAL,
    D3D11_COMPARISON_ALWAYS, //eALWAYS,
    D3D11_COMPARISON_NEVER, //eNEVER,
};
static const D3D_PRIMITIVE_TOPOLOGY topologyMap[] =
{
    D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
    D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
    D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
};

inline DXGI_FORMAT to_DXGI_FORMAT( Format fmt )
{
    const int dtype = fmt.type;
    const int num_elements = fmt.numElements;
    const int norm = fmt.normalized;
    const int srgb = fmt.srgb;

    DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
    if( dtype == EDataType::BYTE )
    {
        if( num_elements == 1 ) result = DXGI_FORMAT_R8_SINT;
        else if( num_elements == 2 ) result = DXGI_FORMAT_R8G8_SINT;
        else if( num_elements == 4 ) result = DXGI_FORMAT_R8G8B8A8_SINT;
    }
    else if( dtype == EDataType::UBYTE )
    {
        if( norm )
        {
            if( num_elements == 1 ) result = DXGI_FORMAT_R8_UNORM;
            else if( num_elements == 2 ) result = DXGI_FORMAT_R8G8_UNORM;
            else if( num_elements == 4 )
            {
                if( srgb )
                    result = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                else
                    result = DXGI_FORMAT_R8G8B8A8_UNORM;
            }
        }
        else if( num_elements == 1 ) result = DXGI_FORMAT_R8_UINT;
        else if( num_elements == 2 ) result = DXGI_FORMAT_R8G8_UINT;
        else if( num_elements == 4 ) result = DXGI_FORMAT_R8G8B8A8_UINT;
    }
    else if( dtype == EDataType::SHORT )
    {
        if( num_elements == 1 ) result = DXGI_FORMAT_R16_SINT;
        else if( num_elements == 4 ) result = DXGI_FORMAT_R16G16B16A16_SINT;
    }
    else if( dtype == EDataType::USHORT )
    {
        if( num_elements == 1 ) result = DXGI_FORMAT_R16_UINT;
        else if( num_elements == 4 ) result = DXGI_FORMAT_R16G16B16A16_UINT;
    }
    else if( dtype == EDataType::INT )
    {
        if( num_elements == 1 ) result = DXGI_FORMAT_R32_SINT;
        else if( num_elements == 2 ) result = DXGI_FORMAT_R32G32_SINT;
        else if( num_elements == 3 ) result = DXGI_FORMAT_R32G32B32_SINT;
        else if( num_elements == 4 ) result = DXGI_FORMAT_R32G32B32A32_SINT;
    }
    else if( dtype == EDataType::UINT )
    {
        if( num_elements == 1 ) result = DXGI_FORMAT_R32_UINT;
        else if( num_elements == 2 ) result = DXGI_FORMAT_R32G32_UINT;
        else if( num_elements == 3 ) result = DXGI_FORMAT_R32G32B32_UINT;
        else if( num_elements == 4 ) result = DXGI_FORMAT_R32G32B32A32_UINT;
    }
    else if( dtype == EDataType::FLOAT )
    {
        if( num_elements == 1 ) result = DXGI_FORMAT_R32_FLOAT;
        else if( num_elements == 2 ) result = DXGI_FORMAT_R32G32_FLOAT;
        else if( num_elements == 3 ) result = DXGI_FORMAT_R32G32B32_FLOAT;
        else if( num_elements == 4 ) result = DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    else if( dtype == EDataType::DOUBLE )
    {

    }
    else if( dtype == EDataType::DEPTH16 )
    {
        result = DXGI_FORMAT_D16_UNORM;
    }
    else if( dtype == EDataType::DEPTH24_STENCIL8 )
    {
        result = DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
    else if( dtype == EDataType::DEPTH32F )
    {
        result = DXGI_FORMAT_D32_FLOAT;
    }

    SYS_ASSERT( result != DXGI_FORMAT_UNKNOWN );
    return result;
}
inline bool isDepthFormat( DXGI_FORMAT format )
{
    return format == DXGI_FORMAT_D16_UNORM || format == DXGI_FORMAT_D24_UNORM_S8_UINT || format == DXGI_FORMAT_D32_FLOAT;
}

inline u32 to_D3D11_BIND_FLAG( u32 bind_flags )
{
    u32 result = 0;
    if( bind_flags & EBindMask::VERTEX_BUFFER    ) result |= D3D11_BIND_VERTEX_BUFFER;
    if( bind_flags & EBindMask::INDEX_BUFFER     ) result |= D3D11_BIND_INDEX_BUFFER;
    if( bind_flags & EBindMask::CONSTANT_BUFFER  ) result |= D3D11_BIND_CONSTANT_BUFFER;
    if( bind_flags & EBindMask::SHADER_RESOURCE  ) result |= D3D11_BIND_SHADER_RESOURCE;
    if( bind_flags & EBindMask::STREAM_OUTPUT    ) result |= D3D11_BIND_STREAM_OUTPUT;
    if( bind_flags & EBindMask::RENDER_TARGET    ) result |= D3D11_BIND_RENDER_TARGET;
    if( bind_flags & EBindMask::DEPTH_STENCIL    ) result |= D3D11_BIND_DEPTH_STENCIL;
    if( bind_flags & EBindMask::UNORDERED_ACCESS ) result |= D3D11_BIND_UNORDERED_ACCESS;
    return result;
}

inline u32 to_D3D11_CPU_ACCESS_FLAG( u32 cpua_flags )
{
    u32 result = 0;
    if( cpua_flags & ECpuAccess::READ ) result |= D3D11_CPU_ACCESS_READ;
    if( cpua_flags & ECpuAccess::WRITE ) result |= D3D11_CPU_ACCESS_WRITE;
    return result;
}

inline int to_D3D_SHADER_MACRO_array( D3D_SHADER_MACRO* output, int output_capacity, const char** shader_macro )
{
    int counter = 0;
    while( *shader_macro )
    {
        SYS_ASSERT( counter < output_capacity );
        output[counter].Name = shader_macro[0];
        output[counter].Definition = shader_macro[1];

        ++counter;
        shader_macro += 2;
    }
    return counter;
}

//////////////////////////////////////////////////////////////////////////
struct CommandQueue
{
    IDXGISwapChain*		 _swapChain = nullptr;
    ID3D11DeviceContext* _context = nullptr;

    ID3D11RenderTargetView* _mainFramebuffer = nullptr;
    u32 _mainFramebufferWidth = 0;
    u32 _mainFramebufferHeight = 0;

    ID3D11DeviceContext* dx11() { return _context; }
};

//////////////////////////////////////////////////////////////////////////
void Dx11FetchShaderReflection( ShaderReflection* out, const void* code_blob, size_t code_blob_size, int stage );
void StartupDX11( uptr hWnd, int winWidth, int winHeight, int fullScreen );
void ShutdownDX11();
}}///