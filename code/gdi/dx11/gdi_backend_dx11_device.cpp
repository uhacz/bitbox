#include "../gdi_backend.h"
#include "../gdi_shader_reflection.h"
#include <util/hash.h>
#include <util/memory.h>
#include <util/array_util.h>

#include "gdi_backend_dx11.h"
#include <d3d11shader.h>
#include <D3Dcompiler.h>
#include "DDSTextureLoader.h"

#include "gdi_backend_dx11_startup.h"

namespace bxGdi
{
    void _FetchShaderReflection( bxGdi::ShaderReflection* out, const void* code_blob, size_t code_blob_size, int stage )
    {
        ID3D11ShaderReflection* reflector = NULL;
        D3DReflect( code_blob, code_blob_size, IID_ID3D11ShaderReflection, (void**)&reflector );

        D3D11_SHADER_DESC sdesc;
        reflector->GetDesc( &sdesc );

        //out->cbuffers.resize( sdesc.ConstantBuffers );

        for ( u32 i = 0; i < sdesc.ConstantBuffers; ++i )
        {
            //ShaderCBufferDesc& cb = out->cbuffers[i];
            ID3D11ShaderReflectionConstantBuffer* cbuffer_reflector = reflector->GetConstantBufferByIndex(i);		

            D3D11_SHADER_BUFFER_DESC sb_desc;
            cbuffer_reflector->GetDesc( &sb_desc );

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
            cb.hashed_name = cb_hashed_name;
            cb.size = sb_desc.Size;
            cb.slot = bind_desc.BindPoint;
            cb.stage_mask = ( 1 << stage );
            //cb.variables.resize( sb_desc.Variables );
            //cb.num_variables = sb_desc.Variables;
            //cb.variables = (ShaderVariableDesc*)utils::memory_alloc( cb.num_variables * sizeof(ShaderVariableDesc) );

            for( u32 j = 0; j < sb_desc.Variables; ++j )
            {
                ID3D11ShaderReflectionVariable* var_reflector = cbuffer_reflector->GetVariableByIndex(j);
                D3D11_SHADER_VARIABLE_DESC sv_desc;
                var_reflector->GetDesc( &sv_desc );

                cb.variables.push_back( ShaderVariableDesc() );
                ShaderVariableDesc& vdesc = cb.variables.back();
                vdesc.hashed_name = simple_hash( sv_desc.Name );
                vdesc.offset = sv_desc.StartOffset;
                vdesc.size = sv_desc.Size;
            }
        }

        for ( u32 i = 0; i < sdesc.BoundResources; ++i )
        {
            D3D11_SHADER_INPUT_BIND_DESC rdesc;
            reflector->GetResourceBindingDesc( i, &rdesc );

            /// system variable
            if( rdesc.Name[0] == '_' ) 
                continue;

            const u32 hashed_name = simple_hash( rdesc.Name );


            if( rdesc.Type == D3D_SIT_TEXTURE )
            {
                ShaderTextureDesc* found = array::find( array::begin( out->textures ), array::end( out->textures ), ShaderReflection::FunctorTexture(hashed_name) );
                if( found != array::end( out->textures ) )
                {
                    found->stage_mask |= ( 1 << stage );
                    continue;
                }
                out->textures.push_back( ShaderTextureDesc() );
                ShaderTextureDesc& tdesc = out->textures.back();
                tdesc.hashed_name = simple_hash( rdesc.Name );
                tdesc.slot = rdesc.BindPoint;
                tdesc.stage_mask = ( 1 << stage );
                switch( rdesc.Dimension )
                {
                case D3D10_SRV_DIMENSION_TEXTURE1D:
                    tdesc.dimm = 1;
                    SYS_ASSERT( false && "not implemented" );
                    break;
                case D3D10_SRV_DIMENSION_TEXTURE2D:
                    tdesc.dimm = 2;
                    break;
                case D3D10_SRV_DIMENSION_TEXTURE3D:
                    tdesc.dimm = 3;
                    SYS_ASSERT( false && "not implemented" );
                    break;
                case D3D10_SRV_DIMENSION_TEXTURECUBE:
                    tdesc.dimm = 2;
                    tdesc.is_cubemap = 1;
                    SYS_ASSERT( false && "not implemented" );
                    break;
                default:
                    SYS_ASSERT( false && "not implemented" );
                    break;
                }
            }
            else if( rdesc.Type == D3D_SIT_SAMPLER )
            {
                ShaderSamplerDesc* found = array::find( array::begin( out->samplers ), array::end( out->samplers ), ShaderReflection::FunctorSampler(hashed_name) );
                if( found != array::end( out->samplers ) )
                {
                    found->stage_mask |= ( 1 << stage );
                    continue;
                }
                out->samplers.push_back( ShaderSamplerDesc() );
                ShaderSamplerDesc& sdesc = out->samplers.back();
                sdesc.hashed_name = simple_hash( rdesc.Name );
                sdesc.slot = rdesc.BindPoint;
                sdesc.stage_mask = ( 1 << stage );
            }
        }

        u16 input_mask = 0;
        for( u32 i = 0; i < sdesc.InputParameters; ++i )
        {
            D3D11_SIGNATURE_PARAMETER_DESC idesc;
            reflector->GetInputParameterDesc( i, &idesc );

            EVertexSlot slot = vertexSlotFromString( idesc.SemanticName );
            if( slot >= eSLOT_COUNT )
            {
                //log_error( "Unknown semantic: '%s'", idesc.SemanticName );
                continue;
            }
            input_mask |= ( 1 << (slot + idesc.SemanticIndex) );
        }
        out->input_mask = input_mask;
        reflector->Release();
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

    virtual bxGdiShader createShader( int stage, const char* shaderSource, const char* entryPoint, const char** shaderMacro, bxGdi::ShaderReflection* reflection = 0)
    {
        SYS_ASSERT( stage < bxGdi::eSTAGE_COUNT );
        const char* shaderModel[bxGdi::eSTAGE_COUNT] = 
        {
            "vs_4_0",
            "ps_4_0",
            //"gs_4_0",
            //"hs_5_0",
            //"ds_5_0",
            "cs_5_0"
        };
    
        D3D_SHADER_MACRO* ptr_macro_defs = 0;
        D3D_SHADER_MACRO macro_defs_array[bxGdi::cMAX_SHADER_MACRO+1];
        memset( macro_defs_array, 0, sizeof(macro_defs_array) );

        if( shaderMacro )
        {
            const int n_macro = bxGdi::to_D3D_SHADER_MACRO_array( macro_defs_array, bxGdi::cMAX_SHADER_MACRO+1, shaderMacro );
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
        if( !SUCCEEDED(hr) )
        {
            const char* error_string = (const char*)error_blob->GetBufferPointer();

            bxLogError( "Compile shader error:\n%s", error_string );
            error_blob->Release();
            SYS_ASSERT( false && "Shader compiler failed" );
        }

        bxGdiShader shader = createShader( stage, code_blob->GetBufferPointer(), code_blob->GetBufferSize(), reflection );
        code_blob->Release();

        return shader;

    }
    virtual bxGdiShader createShader( int stage, const void* codeBlob, size_t codeBlobSizee, bxGdi::ShaderReflection* reflection = 0  )
    {
        bxGdiShader shader;    
    
        ID3DBlob* inputSignature = 0;

        HRESULT hres;
        switch ( stage )
        {
        case bxGdi::eSTAGE_VERTEX:
            hres = _device->CreateVertexShader( codeBlob, codeBlobSizee, 0, &shader.dx.vertex );
            SYS_ASSERT( SUCCEEDED( hres ) );
            hres = D3DGetInputSignatureBlob( codeBlob, codeBlobSizee, &inputSignature );
            SYS_ASSERT( SUCCEEDED( hres ) );
            break;
        case bxGdi::eSTAGE_PIXEL:
            hres = _device->CreatePixelShader( codeBlob, codeBlobSizee, 0, &shader.dx.pixel );
            break;
        //case Pipeline::eSTAGE_GEOMETRY:
        //    hres = device::get(dev)->CreateGeometryShader( codeBlob, codeBlobSizee, 0, &tmp_shader.geometry );
        //    break;
        //case Pipeline::eSTAGE_HULL:
        //    hres = device::get(dev)->CreateHullShader( codeBlob, codeBlobSizee, 0, &tmp_shader.hull );
        //    break;
        //case Pipeline::eSTAGE_DOMAIN:
        //    hres = device::get(dev)->CreateDomainShader( codeBlob, codeBlobSizee, 0, &tmp_shader.domain );
        //    break;
        case bxGdi::eSTAGE_COMPUTE:
            hres = _device->CreateComputeShader( codeBlob, codeBlobSizee, 0, &shader.dx.compute );
            break;
        default:
            SYS_NOT_IMPLEMENTED;
            break;
        }

        SYS_ASSERT( SUCCEEDED( hres ) );

        if( reflection )
        {
            bxGdi::_FetchShaderReflection( reflection, codeBlob, codeBlobSizee, stage );
        }
    
        shader.dx.inputSignature = (void*)inputSignature;
        shader.stage = stage;

        return shader;
    }

    virtual bxGdiTexture createTexture( const void* dataBlob, size_t dataBlobSize ) 
    {
        ID3D11Resource* resource = 0;
	    ID3D11ShaderResourceView* srv = 0;
        //HRESULT hres = D3DX11CreateTextureFromMemory( device::get(dev), data_blob, data_blob_size, 0, 0, &resource, 0 );
	
	    HRESULT hres = DirectX::CreateDDSTextureFromMemory( _device, (const u8*)dataBlob, dataBlobSize, &resource, &srv );
        SYS_ASSERT( SUCCEEDED(hres) );

        ID3D11Texture2D* tex2D = (ID3D11Texture2D*)resource;
        D3D11_TEXTURE2D_DESC desc;
        tex2D->GetDesc( &desc );

        bxGdiTexture tex;
        tex.dx.resource = resource;
        tex.dx.viewSH = srv;

        tex.width = (u16)desc.Width;
        tex.height = (u16)desc.Height;
        tex.depth = 0;
        return tex;
    }
    virtual bxGdiTexture createTexture1D()
    {
        SYS_NOT_IMPLEMENTED;
        return bxGdiTexture();
    }
    virtual bxGdiTexture createTexture2D( int w, int h, int mips, bxGdiFormat format, unsigned bindFlags, unsigned cpuaFlags, const void* data )
    {
        const DXGI_FORMAT dx_format = bxGdi::to_DXGI_FORMAT( format.type, format.numElements, format.normalized, format.srgb );
        const u32 dx_bind_flags     = bxGdi::to_D3D11_BIND_FLAG( bindFlags );
        const u32 dx_cpua_flags     = bxGdi::to_D3D11_CPU_ACCESS_FLAG( cpuaFlags );

        D3D11_TEXTURE2D_DESC desc;
        memset( &desc, 0, sizeof(desc) );
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

        ID3D11Texture2D* tex2D = 0;
        HRESULT hres = _device->CreateTexture2D( &desc, 0, &tex2D );
        SYS_ASSERT( SUCCEEDED( hres ) );

        ID3D11ShaderResourceView* view_sh = 0;
        ID3D11RenderTargetView* view_rt = 0;
        ID3D11UnorderedAccessView* view_ua = 0;

        ID3D11RenderTargetView** rtv_mips = 0;

        if( dx_bind_flags & D3D11_BIND_SHADER_RESOURCE )
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
            memset( &srvdesc, 0, sizeof(srvdesc) );
            srvdesc.Format = dx_format;
            srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvdesc.Texture2D.MostDetailedMip = 0;
            srvdesc.Texture2D.MipLevels = desc.MipLevels;
            hres = _device->CreateShaderResourceView( tex2D, &srvdesc, &view_sh );
            SYS_ASSERT( SUCCEEDED(hres) );
        }

        if( dx_bind_flags & D3D11_BIND_RENDER_TARGET )
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvdesc;
            memset( &rtvdesc, 0, sizeof(rtvdesc) );
            rtvdesc.Format = dx_format;
            rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvdesc.Texture2D.MipSlice = 0;
            hres = _device->CreateRenderTargetView( tex2D, &rtvdesc, &view_rt );
            SYS_ASSERT( SUCCEEDED(hres) );
        }

        if( dx_bind_flags & D3D11_BIND_DEPTH_STENCIL )
        {
            bxLogError( "Depth stencil binding is not supported without depth texture format" );
        }

        if( dx_bind_flags & D3D11_BIND_UNORDERED_ACCESS )
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc;
            memset( &uavdesc, 0, sizeof(uavdesc) );
            uavdesc.Format = dx_format;
            uavdesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            uavdesc.Texture2D.MipSlice = 0;
            hres = _device->CreateUnorderedAccessView( tex2D, &uavdesc, &view_ua );
            SYS_ASSERT( SUCCEEDED(hres) );
        }

        bxGdiTexture tex;
        tex.dx._2D = tex2D;
        tex.dx.viewSH = view_sh;
        tex.dx.viewRT = view_rt;
        tex.dx.viewUA = view_ua;
        tex.dx.viewDS = 0;
        tex.width = w;
        tex.height = h;
        tex.depth = 0;
        tex.format = format;
        return tex;
    }
    virtual bxGdiTexture createTexture2Ddepth( int w, int h, int mips, bxGdi::EDataType dataType, unsigned bindFlags )
    {
        const DXGI_FORMAT dx_format = bxGdi::to_DXGI_FORMAT( dataType, 1 );
        const u32 dx_bind_flags     = bxGdi::to_D3D11_BIND_FLAG( bindFlags );

        SYS_ASSERT( bxGdi::isDepthFormat( dx_format ) );
    
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
            SYS_ASSERT(false);
            break;
        }

        D3D11_TEXTURE2D_DESC desc;
        memset( &desc, 0, sizeof(desc) );
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
        HRESULT hres = _device->CreateTexture2D( &desc, 0, &tex2d );
        SYS_ASSERT( SUCCEEDED(hres) );

        ID3D11ShaderResourceView* view_sh = 0;
        ID3D11DepthStencilView* view_ds = 0;

        if( dx_bind_flags & D3D11_BIND_SHADER_RESOURCE )
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
            memset( &srvdesc, 0, sizeof(srvdesc) );
            srvdesc.Format = srv_format;
            srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvdesc.Texture2D.MostDetailedMip = 0;
            srvdesc.Texture2D.MipLevels = mips;
            hres = _device->CreateShaderResourceView( tex2d, &srvdesc, &view_sh );
            SYS_ASSERT( SUCCEEDED(hres) );
        }

        if( dx_bind_flags & D3D11_BIND_DEPTH_STENCIL )
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvdesc;
            memset( &dsvdesc, 0, sizeof(dsvdesc) );
            dsvdesc.Format = dx_format;
            dsvdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvdesc.Texture2D.MipSlice = 0;
            hres = _device->CreateDepthStencilView( tex2d, &dsvdesc, &view_ds );
            SYS_ASSERT( SUCCEEDED(hres) );
        }

        bxGdiTexture tex;
        tex.dx._2D = tex2d;
        tex.dx.viewDS = view_ds;
        tex.dx.viewSH = view_sh;
        tex.dx.viewRT = 0;
        tex.dx.viewUA = 0;
        tex.width = w;
        tex.height = h;
        tex.depth = 0;
        tex.format = bxGdiFormat( dataType, 1 );

        return tex;
    }
    virtual bxGdiTexture createTexture3D()
    {
        SYS_NOT_IMPLEMENTED;
        return bxGdiTexture();
    }
    virtual bxGdiTexture createTextureCube()
    {
        SYS_NOT_IMPLEMENTED;
        return bxGdiTexture();
    }
    
    virtual bxGdiSampler createSampler( const bxGdiSamplerDesc& desc )
    {
        D3D11_SAMPLER_DESC dxDesc;
        dxDesc.Filter = ( desc.depthCmpMode == bxGdi::eDEPTH_CMP_NONE ) ? bxGdi::filters[desc.filter] : bxGdi::comparisionFilters[desc.filter];
        dxDesc.ComparisonFunc = bxGdi::comparision[desc.depthCmpMode];

        dxDesc.AddressU = bxGdi::addressMode[desc.addressU];
        dxDesc.AddressV = bxGdi::addressMode[desc.addressV];
        dxDesc.AddressW = bxGdi::addressMode[desc.addressT];

        dxDesc.MaxAnisotropy = ( bxGdi::hasAniso((bxGdi::ESamplerFilter)desc.filter) ) ? desc.aniso: 1;

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
        HRESULT hres = _device->CreateSamplerState( &dxDesc, &dxState );
        SYS_ASSERT( SUCCEEDED( hres ) );

        bxGdiSampler result;
        result.dx.state = dxState;
        return result;
    }
    
    virtual bxGdiInputLayout createInputLayout( const bxGdiVertexStreamDesc* descs, int ndescs, bxGdiShader vertexShader )
    {
        SYS_ASSERT( vertexShader.stage == bxGdi::eSTAGE_VERTEX );

        const int MAX_IEDESCS = bxGdi::cMAX_VERTEX_BUFFERS;
        D3D11_INPUT_ELEMENT_DESC d3d_iedescs[MAX_IEDESCS];
        int num_d3d_iedescs = 0;
        for( int idesc = 0; idesc < ndescs; ++idesc )
        {
            const bxGdiVertexStreamDesc desc = descs[idesc];
            int offset = 0;
            for( int iblock = 0; iblock < desc.count; ++iblock )
            {
                SYS_ASSERT( num_d3d_iedescs < MAX_IEDESCS );

                bxGdiVertexStreamBlock block = desc.blocks[iblock];
                D3D11_INPUT_ELEMENT_DESC& d3d_desc = d3d_iedescs[num_d3d_iedescs++];

                d3d_desc.SemanticName = bxGdi::slotName[block.slot];
                d3d_desc.SemanticIndex = bxGdi::slotSemanticIndex[block.slot];
                d3d_desc.Format = bxGdi::to_DXGI_FORMAT( block.dataType, block.numElements );
                d3d_desc.InputSlot = idesc;
                d3d_desc.AlignedByteOffset = offset;
                d3d_desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                d3d_desc.InstanceDataStepRate = 0;
                offset += bxGdi::blockStride( desc.blocks[iblock] );
            }
        }

        ID3D11InputLayout *dxLayout = 0;
        ID3DBlob* signatureBlob = (ID3DBlob*)vertexShader.dx.inputSignature;
        HRESULT hres = _device->CreateInputLayout( d3d_iedescs, num_d3d_iedescs, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), &dxLayout );
        SYS_ASSERT( SUCCEEDED( hres ) );

        bxGdiInputLayout iLay;
        iLay.dx.lay = dxLayout;

        return iLay;
    }
    virtual bxGdiBlendState createBlendState( bxGdiHwStateDesc::Blend state )
    {
        D3D11_BLEND_DESC bdesc;
        memset( &bdesc, 0, sizeof(bdesc) );

        bdesc.AlphaToCoverageEnable = FALSE;
        bdesc.IndependentBlendEnable = FALSE;
        bdesc.RenderTarget[0].BlendEnable = state.enable;
        bdesc.RenderTarget[0].SrcBlend  = bxGdi::blendFactor[state.srcFactor];
        bdesc.RenderTarget[0].DestBlend = bxGdi::blendFactor[state.dstFactor];
        bdesc.RenderTarget[0].BlendOp   = bxGdi::blendEquation[state.equation];
        bdesc.RenderTarget[0].SrcBlendAlpha  = bxGdi::blendFactor[state.srcFactorAlpha];
        bdesc.RenderTarget[0].DestBlendAlpha = bxGdi::blendFactor[state.dstFactorAlpha];
        bdesc.RenderTarget[0].BlendOpAlpha   = bxGdi::blendEquation[state.equation];

        u8 mask = 0;
        mask |= (state.color_mask & bxGdi::eCOLOR_MASK_RED)	    ? D3D11_COLOR_WRITE_ENABLE_RED : 0;
        mask |= (state.color_mask & bxGdi::eCOLOR_MASK_GREEN)	? D3D11_COLOR_WRITE_ENABLE_GREEN : 0;
        mask |= (state.color_mask & bxGdi::eCOLOR_MASK_BLUE)	? D3D11_COLOR_WRITE_ENABLE_BLUE : 0;
        mask |= (state.color_mask & bxGdi::eCOLOR_MASK_ALPHA)	? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0;

        bdesc.RenderTarget[0].RenderTargetWriteMask = mask;

        ID3D11BlendState* dx_state = 0;
        HRESULT hres = _device->CreateBlendState( &bdesc, &dx_state );
        SYS_ASSERT( SUCCEEDED( hres ) );

        bxGdiBlendState result;
        result.dx.state = dx_state;
        return result;
    }
    virtual bxGdiDepthState createDepthState( bxGdiHwStateDesc::Depth state )
    {
        D3D11_DEPTH_STENCIL_DESC dsdesc;
        memset( &dsdesc, 0, sizeof(dsdesc) );

        dsdesc.DepthEnable = state.test;
        dsdesc.DepthWriteMask = ( state.write ) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsdesc.DepthFunc = bxGdi::depthCmpFunc[state.function];

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
        HRESULT hres = _device->CreateDepthStencilState( &dsdesc, &dx_state );
        SYS_ASSERT( SUCCEEDED( hres ) );
        
        bxGdiDepthState result;
        result.dx.state = dx_state;
        return result;
    }
    virtual bxGdiRasterState createRasterState( bxGdiHwStateDesc::Raster state )
    {
        D3D11_RASTERIZER_DESC rdesc;
        memset( &rdesc, 0, sizeof(rdesc) );

        rdesc.FillMode = bxGdi::fillMode[state.fillMode];
        rdesc.CullMode = bxGdi::cullMode[state.cullMode];
        rdesc.FrontCounterClockwise = TRUE;
        rdesc.DepthBias = 0;
        rdesc.SlopeScaledDepthBias = 0.f;
        rdesc.DepthBiasClamp = 0.f;
        rdesc.DepthClipEnable = TRUE;
        rdesc.ScissorEnable = state.scissor;
        rdesc.MultisampleEnable = FALSE;
        rdesc.AntialiasedLineEnable = state.antialiasedLine;

        ID3D11RasterizerState* dx_state = 0;
        HRESULT hres = _device->CreateRasterizerState( &rdesc, &dx_state );
        SYS_ASSERT( SUCCEEDED( hres ) );

        bxGdiRasterState result;
        result.dx.state = dx_state;
        return result;
    }

#define BX_RELEASE_DX_RESOURCE_SAFE0( resource ) if( resource ) { resource->Release(); resource = 0; }

    virtual void releaseVertexBuffer( bxGdiVertexBuffer* id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx11Buffer );
    }
    virtual void releaseIndexBuffer( bxGdiIndexBuffer* id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx11Buffer );
    }
    virtual void releaseBuffer( bxGdiBuffer* id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx11Buffer );
    }
    virtual void releaseShader( bxGdiShader* id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.object );

        if( id->dx.inputSignature )
        {
            ID3DBlob* blob = (ID3DBlob*)id->dx.inputSignature;
            blob->Release();
            id->dx.inputSignature = 0;
        }
    }
    virtual void releaseTexture( bxGdiTexture* id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.resource );
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.viewSH );
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.viewRT );
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.viewDS );
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.viewUA );
    }
    virtual void releaseSampler( bxGdiSampler* id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.state );
    }
    virtual void releaseInputLayout( bxGdiInputLayout * id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.lay );
    }
    virtual void releaseBlendState ( bxGdiBlendState  * id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.state );
    }
    virtual void releaseDepthState ( bxGdiDepthState  * id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.state );
    }
    virtual void releaseRasterState( bxGdiRasterState * id )
    {
        BX_RELEASE_DX_RESOURCE_SAFE0( id->dx.state );
    }

    bxGdiDeviceBackend_dx11()
        : _device(0)
    {}

    virtual ~bxGdiDeviceBackend_dx11()
    {
        _device->Release();
    }

    ID3D11Device* _device;
};

namespace bxGdi
{
    bxGdiDeviceBackend* newDeveice_dx11( ID3D11Device* deviceDx11 )
    {
        bxGdiDeviceBackend_dx11* dev = BX_NEW( bxDefaultAllocator(), bxGdiDeviceBackend_dx11 );
        dev->_device = deviceDx11;
        return dev;
    }

    int startup_dx11( bxGdiDeviceBackend** dev, uptr hWnd, int winWidth, int winHeight, int fullScreen )
    {
        DXGI_SWAP_CHAIN_DESC sd;
	    ZeroMemory( &sd, sizeof(sd) );
	    sd.BufferCount = 1;
	    sd.BufferDesc.Width = winWidth;
	    sd.BufferDesc.Height = winHeight;
	    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	    sd.BufferDesc.RefreshRate.Numerator = 0;
	    sd.BufferDesc.RefreshRate.Denominator = 0;
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
		    return -1;
	    }

        dev[0] = newDeveice_dx11( dx11Dev );
        dev[0]->ctx = newContext_dx11( dx11Ctx, dx11SwapChain );

        return 0;
    }
}///
