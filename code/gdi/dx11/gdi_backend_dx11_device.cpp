#include "../gdi_backend.h"
#include <util/pool_allocator.h>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <D3Dcompiler.h>
#include "DDSTextureLoader.h"

namespace bxGdi
{
    static DXGI_FORMAT to_DXGI_FORMAT( int dtype, int num_elements, int norm = 0, int srgb = 0 )
    {
        DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
        if ( dtype == eTYPE_BYTE )
        {
            if      ( num_elements == 1 ) result = DXGI_FORMAT_R8_SINT;
            else if ( num_elements == 2 ) result = DXGI_FORMAT_R8G8_SINT;
            else if ( num_elements == 4 ) result = DXGI_FORMAT_R8G8B8A8_SINT;
        }
        else if ( dtype == eTYPE_UBYTE )
        {
            if( norm )
            {
                if      ( num_elements == 1 ) result = DXGI_FORMAT_R8_UNORM;
                else if ( num_elements == 2 ) result = DXGI_FORMAT_R8G8_UNORM;
                else if ( num_elements == 4 ) 
                {
                    if( srgb )
                        result = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                    else
                        result = DXGI_FORMAT_R8G8B8A8_UNORM;
                }
            }
            else if ( num_elements == 1 ) result = DXGI_FORMAT_R8_UINT;
            else if ( num_elements == 2 ) result = DXGI_FORMAT_R8G8_UINT;
            else if ( num_elements == 4 ) result = DXGI_FORMAT_R8G8B8A8_UINT;
        }
        else if ( dtype == eTYPE_SHORT )
        {
            if      ( num_elements == 1 ) result = DXGI_FORMAT_R16_SINT;
            else if ( num_elements == 4 ) result = DXGI_FORMAT_R16G16B16A16_SINT;
        }
        else if ( dtype == eTYPE_USHORT )
        {
            if      ( num_elements == 1 ) result = DXGI_FORMAT_R16_UINT;
            else if ( num_elements == 4 ) result = DXGI_FORMAT_R16G16B16A16_UINT;
        }
        else if ( dtype == eTYPE_INT )
        {
            if      ( num_elements == 1 ) result = DXGI_FORMAT_R32_SINT;        
            else if ( num_elements == 2 ) result = DXGI_FORMAT_R32G32_SINT;     
            else if ( num_elements == 3 ) result = DXGI_FORMAT_R32G32B32_SINT;
            else if ( num_elements == 4 ) result = DXGI_FORMAT_R32G32B32A32_SINT;
        }
        else if ( dtype == eTYPE_UINT )
        {
            if      ( num_elements == 1 ) result = DXGI_FORMAT_R32_UINT;        
            else if ( num_elements == 2 ) result = DXGI_FORMAT_R32G32_UINT;     
            else if ( num_elements == 3 ) result = DXGI_FORMAT_R32G32B32_UINT;
            else if ( num_elements == 4 ) result = DXGI_FORMAT_R32G32B32A32_UINT;
        }
        else if ( dtype == eTYPE_FLOAT )
        {
            if      ( num_elements == 1 ) result = DXGI_FORMAT_R32_FLOAT;
            else if ( num_elements == 2 ) result = DXGI_FORMAT_R32G32_FLOAT;
            else if ( num_elements == 3 ) result = DXGI_FORMAT_R32G32B32_FLOAT;
            else if ( num_elements == 4 ) result = DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
        else if ( dtype == eTYPE_DOUBLE )
        {

        }
        else if ( dtype == eTYPE_DEPTH16 )
        {
            result = DXGI_FORMAT_D16_UNORM;
        }
        else if ( dtype == eTYPE_DEPTH24_STENCIL8 )
        {
            result = DXGI_FORMAT_D24_UNORM_S8_UINT;
        }
        else if ( dtype == eTYPE_DEPTH32F )
        {
            result = DXGI_FORMAT_D32_FLOAT;
        }

        SYS_ASSERT( result != DXGI_FORMAT_UNKNOWN );
        return result;
    }
    inline bool is_depth_format( DXGI_FORMAT format )
    {
        return format == DXGI_FORMAT_D16_UNORM || format == DXGI_FORMAT_D24_UNORM_S8_UINT || format == DXGI_FORMAT_D32_FLOAT;
    }

    static u32 to_D3D11_BIND_FLAG( u32 bind_flags )
    {
        u32 result = 0;
        if( bind_flags & eBIND_VERTEX_BUFFER	 ) result |= D3D11_BIND_VERTEX_BUFFER;
        if( bind_flags & eBIND_INDEX_BUFFER	 ) result |= D3D11_BIND_INDEX_BUFFER;     
        if( bind_flags & eBIND_CONSTANT_BUFFER ) result |= D3D11_BIND_CONSTANT_BUFFER;  
        if( bind_flags & eBIND_SHADER_RESOURCE ) result |= D3D11_BIND_SHADER_RESOURCE;  
        if( bind_flags & eBIND_STREAM_OUTPUT   ) result |= D3D11_BIND_STREAM_OUTPUT;    
        if( bind_flags & eBIND_RENDER_TARGET   ) result |= D3D11_BIND_RENDER_TARGET;    
        if( bind_flags & eBIND_DEPTH_STENCIL   ) result |= D3D11_BIND_DEPTH_STENCIL;    
        if( bind_flags & eBIND_UNORDERED_ACCESS) result |= D3D11_BIND_UNORDERED_ACCESS;    
        return result;
    }

    static u32 to_D3D11_CPU_ACCESS_FLAG( u32 cpua_flags )
    {
        u32 result = 0;
        if( cpua_flags & eCPU_READ  ) result |= D3D11_CPU_ACCESS_READ;
        if( cpua_flags & eCPU_WRITE ) result |= D3D11_CPU_ACCESS_WRITE;
        return result;
    }
}///


struct bxGdiDeviceBackend_dx11 : public bxGdiDeviceBackend
{
    virtual bxGdiVertexBuffer createVertexBuffer( const bxGdiVertexStreamDesc& desc, u32 numElements, const void* data )
    {
        const u32 MAX_DESCS = 16;
        D3D11_INPUT_ELEMENT_DESC descs[MAX_DESCS];
        memset( descs, 0, sizeof(descs) );

        int stream_stride = 0;
        for( int i = 0; i < desc.count; ++i )
        {
            const bxGdiVertexStreamBlock& block = desc.blocks[i];
            D3D11_INPUT_ELEMENT_DESC& d11_desc = descs[i];

            d11_desc.SemanticName = bxGdi::slotName[block.slot];
            d11_desc.SemanticIndex = bxGdi::slotSemanticIndex[block.slot];
            d11_desc.Format = bxGdi::to_DXGI_FORMAT( block.dataType, block.numElements );
            d11_desc.InputSlot = block.slot;
            d11_desc.AlignedByteOffset = stream_stride;
            d11_desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            d11_desc.InstanceDataStepRate = 0;

            stream_stride += bxGdi::blockStride( block );//vertex_stream_block_stride( desc, i );
        }

        const u32 stride = bxGdi::streamStride( desc );
        const u32 num_descs = desc.count;

        D3D11_BUFFER_DESC bdesc;
        memset( &bdesc, 0, sizeof(bdesc) );
        bdesc.ByteWidth = numElements * stride;
        bdesc.Usage = (data) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
        bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bdesc.CPUAccessFlags = ( data ) ? 0 : D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA resource_data;
        resource_data.pSysMem = data;
        resource_data.SysMemPitch = 0;
        resource_data.SysMemSlicePitch = 0;

        D3D11_SUBRESOURCE_DATA* resource_data_pointer = (data) ? &resource_data : 0;

        ID3D11Buffer* buffer = 0;
        HRESULT hres = _device->CreateBuffer( &bdesc, resource_data_pointer, &buffer );
        SYS_ASSERT( SUCCEEDED( hres ) );

        bxGdiVertexBuffer vBuffer;
        vBuffer.dx11Buffer = buffer;
        vBuffer.numElements = numElements;
        vBuffer.desc = desc;
        return vBuffer;
    }
    virtual bxGdiIndexBuffer createIndexBuffer( int dataType, u32 numElements, const void* data )
    {
        const DXGI_FORMAT d11_data_type = bxGdi::to_DXGI_FORMAT( dataType, 1 );
        const u32 stride = bxGdi::typeStride[ dataType ];
        const u32 sizeInBytes = numElements * stride;

        D3D11_BUFFER_DESC bdesc;
        memset( &bdesc, 0, sizeof(bdesc) );
        bdesc.ByteWidth = sizeInBytes;
        bdesc.Usage = (data) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
        bdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bdesc.CPUAccessFlags = ( data ) ? 0 : D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA resourceData;
        resourceData.pSysMem = data;
        resourceData.SysMemPitch = 0;
        resourceData.SysMemSlicePitch = 0;

        D3D11_SUBRESOURCE_DATA* resourceDataPointer = (data) ? &resourceData : 0;

        ID3D11Buffer* buffer = 0;
        HRESULT hres = _device->CreateBuffer( &bdesc, resourceDataPointer, &buffer );
        SYS_ASSERT( SUCCEEDED( hres ) );

        bxGdiIndexBuffer iBuffer;
        iBuffer.dx11Buffer = buffer;
        iBuffer.dataType = dataType;
        iBuffer.numElements = numElements;

        return iBuffer;
    }
    virtual bxGdiBuffer createBuffer( u32 sizeInBytes, u32 bindFlags )
    {
        const u32 dxBindFlags = bxGdi::to_D3D11_BIND_FLAG( bindFlags );

        D3D11_BUFFER_DESC bdesc;
        memset( &bdesc, 0, sizeof(bdesc) );
        bdesc.ByteWidth = sizeInBytes;
        bdesc.Usage = D3D11_USAGE_DEFAULT;
        bdesc.BindFlags = dxBindFlags;
        bdesc.CPUAccessFlags = 0;

        ID3D11Buffer* buffer;
        HRESULT hres = _device->CreateBuffer( &bdesc, 0, &buffer );
        SYS_ASSERT( SUCCEEDED( hres ) );

        if( dxBindFlags & D3D11_BIND_SHADER_RESOURCE )
        {
        }
        if( dxBindFlags & D3D11_BIND_UNORDERED_ACCESS )
        {
        }

        bxGdiBuffer b;
        b.dx11Buffer = buffer;
        b.sizeInBytes = sizeInBytes;
        b.bindFlags = bindFlags;

        return b;
    }

    virtual bxGdiShader createShader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, bxGdiShaderReflection* reflection = 0)
    {}
    virtual bxGdiShader createShader( int stage, const void* codeBlob, size_t codeBlobSizee, bxGdiShaderReflection* reflection = 0  )
    {}

    virtual bxGdiTexture createTexture( const void* dataBlob, size_t dataBlobSize ) 
    {}
    virtual bxGdiTexture createTexture1D()
    {}
    virtual bxGdiTexture createTexture2D()
    {}
    virtual bxGdiTexture createTexture3D()
    {}
    virtual bxGdiTexture createTextureCube()
    {}

    virtual bxGdiInputLayout createInputLayout( const bxGdiVertexStreamDesc* descs, int ndescs, bxGdiShader vertex_shader )
    {}
    virtual bxGdiBlendState  createBlendState()
    {}
    virtual bxGdiDepthState  createDepthState()
    {}
    virtual bxGdiRasterState createRasterState()
    {}

    virtual void releaseVertexBuffer( bxGdiVertexBuffer* id )
    {}
    virtual void releaseIndexBuffer( bxGdiIndexBuffer* id )
    {}
    virtual void releaseBuffer( bxGdiBuffer* id )
    {}
    virtual void releaseShader( bxGdiShader* id )
    {}
    virtual void releaseTexture( bxGdiTexture* id )
    {}
    virtual void releaseInputLayout( bxGdiInputLayout * id )
    {}
    virtual void releaseBlendState ( bxGdiBlendState  * id )
    {}
    virtual void releaseDepthState ( bxGdiDepthState  * id )
    {}
    virtual void releaseRasterState( bxGdiRasterState * id )
    {}

    ID3D11Device* _device;


    //bxPoolAllocator _textureAllocator;
    //bxPoolAllocator _shaderAllocator;
};