#include "rdi.h"
#include "rdi_shader_reflection.h"

#include <util/hash.h>
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/common.h>
#include <util/poly/poly_shape.h>

#include <resource_manager/resource_manager.h>

#include <algorithm>

namespace bx { namespace rdi {
namespace utils
{
    inline bxAllocator* getAllocator( bxAllocator* defaultAllocator )
    {
        return ( defaultAllocator ) ? defaultAllocator : bxDefaultAllocator();
    }
}



}
}///


namespace bx{ namespace rdi{



u32 ShaderFileNameHash( const char* name, u32 version )
{
    return murmur3_hash32( name, (u32)strlen( name ), version );
}
ShaderFile* ShaderFileLoad( const char* filename, ResourceManager* resourceManager )
{
    ResourceLoadResult resource = resourceManager->loadResource( filename, EResourceFileType::BINARY );
    return (ShaderFile*)resource.ptr;
}

void ShaderFileUnload( ShaderFile** sfile, ResourceManager* resourceManager )
{
    resourceManager->unloadResource( (ResourcePtr*)sfile );
}

u32 ShaderFileFindPass( const ShaderFile* sfile, const char* passName )
{
    const u32 hashed_name = ShaderFileNameHash( passName, sfile->version );
    for( u32 i = 0; i < sfile->num_passes; ++i )
    {
        if( hashed_name == sfile->passes[i].hashed_name )
            return i;
    }
    return UINT32_MAX;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct ShaderObject
{
    ShaderPass pass;
    HardwareState hardware_state;
    InputLayout input_layout;
    ResourceDescriptor resource_desc;
};
void ShaderObjectCreate( ShaderObject* shaderObj, const ShaderFile* shaderFile, const char* passName, HardwareStateDesc* hwStateDescOverride )
{
    const u32 pass_index = ShaderFileFindPass( shaderFile, passName );
    SYS_ASSERT( pass_index != UINT32_MAX );

    const ShaderFile::Pass& pass = shaderFile->passes[pass_index];
    ShaderPassCreateInfo pass_create_info = {};
    pass_create_info.vertex_bytecode = TYPE_OFFSET_GET_POINTER( void, shaderFile->passes[pass_index].offset_bytecode_vertex );
    pass_create_info.vertex_bytecode_size = pass.size_bytecode_vertex;
    pass_create_info.pixel_bytecode = TYPE_OFFSET_GET_POINTER( void, shaderFile->passes[pass_index].offset_bytecode_pixel );
    pass_create_info.pixel_bytecode_size = pass.size_bytecode_pixel;
    pass_create_info.reflection = nullptr; // &shader_reflection;
    shaderObj->pass = device::CreateShaderPass( pass_create_info );
    shaderObj->pass.vertex_input_mask = pass.vertex_layout.InputMask();
    
    shaderObj->input_layout = device::CreateInputLayout( pass.vertex_layout, shaderObj->pass );
    const HardwareStateDesc* hw_state_desc = ( hwStateDescOverride ) ? hwStateDescOverride : &pass.hw_state_desc;
    shaderObj->hardware_state = device::CreateHardwareState( *hw_state_desc );

    if( pass.offset_resource_descriptor )
    {
        void* src_resource_desc_ptr = TYPE_OFFSET_GET_POINTER( void, pass.offset_resource_descriptor );
        void* resource_desc_memory = BX_MALLOC( bxDefaultAllocator(), pass.size_resource_descriptor, 16 );
        memcpy( resource_desc_memory, src_resource_desc_ptr, pass.size_resource_descriptor );
        shaderObj->resource_desc = (ResourceDescriptor)resource_desc_memory;
    }
}
void ShaderObjectDestroy( ShaderObject* shaderObj )
{
    BX_FREE0( bxDefaultAllocator(), shaderObj->resource_desc );
    device::DestroyHardwareState( &shaderObj->hardware_state );
    device::DestroyInputLayout( &shaderObj->input_layout );
    device::DestroyShaderPass( &shaderObj->pass );
}
void ShaderObjectBind( CommandQueue* cmdq, const ShaderObject& shaderObj, bool bindResources )
{
    context::SetShaderPass( cmdq, shaderObj.pass );
    context::SetInputLayout( cmdq, shaderObj.input_layout );
    context::SetHardwareState( cmdq, shaderObj.hardware_state );
    if( shaderObj.resource_desc && bindResources )
    {
        BindResources( cmdq, shaderObj.resource_desc );
    }
}


struct PipelineImpl
{
    ShaderObject   shader_object = {};
    ETopology::Enum topology = ETopology::TRIANGLES;
};
Pipeline CreatePipeline( const PipelineDesc& desc, bxAllocator* allocator /*= nullptr */ )
{
    PipelineImpl* impl = (PipelineImpl*)BX_NEW( utils::getAllocator( allocator ), PipelineImpl );
    ShaderObjectCreate( &impl->shader_object, desc.shader_file, desc.shader_pass_name, desc.hw_state_desc_override );
    impl->topology = desc.topology;

    return impl;
}

void DestroyPipeline( Pipeline* pipeline, bxAllocator* allocator /*= nullptr */ )
{
    if( !pipeline[0] )
        return;

    PipelineImpl* impl = pipeline[0];
    ShaderObjectDestroy( &impl->shader_object );
    BX_DELETE0( utils::getAllocator( allocator ), pipeline[0] );
}

void BindPipeline( CommandQueue* cmdq, Pipeline pipeline, bool bindResources )
{
    ShaderObjectBind( cmdq, pipeline->shader_object, bindResources );
    context::SetTopology( cmdq, pipeline->topology );
}

ResourceDescriptor GetResourceDescriptor( Pipeline pipeline )
{
    return pipeline->shader_object.resource_desc;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RenderTargetImpl
{
    TextureRW color_textures[cMAX_RENDER_TARGETS] = {};
    TextureDepth depth_texture;
    u32 num_color_textures = 0;
};
RenderTarget CreateRenderTarget( const RenderTargetDesc& desc, bxAllocator* allocator /*= nullptr */ )
{
    RenderTargetImpl* impl = BX_NEW( utils::getAllocator( allocator ), RenderTargetImpl );
    for( u32 i = 0; i < desc.num_color_textures; ++i )
    {
        impl->color_textures[i] = device::CreateTexture2D( desc.width, desc.height, desc.mips, desc.color_texture_formats[i], EBindMask::RENDER_TARGET | EBindMask::SHADER_RESOURCE, 0, nullptr );
    }
    impl->num_color_textures = desc.num_color_textures;

    if( desc.depth_texture_type != EDataType::UNKNOWN )
    {
        impl->depth_texture = device::CreateTexture2Ddepth( desc.width, desc.height, desc.mips, desc.depth_texture_type );
    }

    return impl;
    
}

void DestroyRenderTarget( RenderTarget* renderTarget, bxAllocator* allocator /*= nullptr */ )
{
    if( renderTarget[0] == nullptr )
        return;

    RenderTargetImpl* impl = renderTarget[0];

    if( impl->depth_texture.id )
    {
        device::DestroyTexture( &impl->depth_texture );
    }

    for( u32 i = 0; i < impl->num_color_textures; ++i )
    {
        device::DestroyTexture( &impl->color_textures[i] );
    }

    BX_DELETE0( utils::getAllocator( allocator ), renderTarget[0] );
}


void ClearRenderTarget( CommandQueue* cmdq, RenderTarget rtarget, float r, float g, float b, float a, float d )
{
    float rgbad[5] = { r, g, b, a, d };
    int clear_depth = ( d >= 0.f ) ? 1 : 0;
    rdi::context::ClearBuffers( cmdq, rtarget->color_textures, rtarget->num_color_textures, rtarget->depth_texture, rgbad, 1, clear_depth );
}

void ClearRenderTargetDepth( CommandQueue* cmdq, RenderTarget rtarget, float d )
{
    float rgbad[5] = { 0.f, 0.f, 0.f, 0.f, d };
    rdi::context::ClearBuffers( cmdq, nullptr, 0, rtarget->depth_texture, rgbad, 0, 1 );
}

void BindRenderTarget( CommandQueue* cmdq, RenderTarget renderTarget, const std::initializer_list<u8>& colorTextureIndices, bool useDepth )
{
    SYS_ASSERT( (u32)colorTextureIndices.size() < cMAX_RENDER_TARGETS );
    
    TextureRW color[cMAX_RENDER_TARGETS] = {};
    u8 color_array_index = 0;
    for( u8 i : colorTextureIndices )
    {
        SYS_ASSERT( i < renderTarget->num_color_textures );
        color[color_array_index++] = renderTarget->color_textures[i];
    }

    TextureDepth depth = {};
    if( useDepth )
    {
        depth = renderTarget->depth_texture;
    }

    context::ChangeRenderTargets( cmdq, color, (u32)colorTextureIndices.size(), depth );
}

void BindRenderTarget( CommandQueue* cmdq, RenderTarget renderTarget )
{
    context::ChangeRenderTargets( cmdq, renderTarget->color_textures, renderTarget->num_color_textures, renderTarget->depth_texture );
}

TextureRW GetTexture( RenderTarget rtarget, u32 index )
{
    SYS_ASSERT( index < rtarget->num_color_textures );
    return rtarget->color_textures[index];
}

TextureDepth GetTextureDepth( RenderTarget rtarget )
{
    return rtarget->depth_texture;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct ResourceDescriptorImpl
{
    struct Binding
    {
        u8 binding_type = EBindingType::_COUNT_;
        u8 slot = 0;
        u16 stage_mask = 0;
        u32 data_offset = 0;
    };

    const u32* HashedNames() const { return (u32*)( this + 1 ); }
    const Binding* Bindings() const { return (Binding*)(HashedNames() + count); }
    u8* Data() { return (u8*)( Bindings() + count ); }

    
    u32 count = 0;
};
namespace
{
    static const u32 _resource_size[EBindingType::_COUNT_] =
    {
        sizeof( ResourceRO ),     //eRESOURCE_TYPE_READ_ONLY,
        sizeof( ResourceRW ),     //eRESOURCE_TYPE_READ_WRITE,
        sizeof( ConstantBuffer ), //eRESOURCE_TYPE_UNIFORM,
        sizeof( Sampler ),        //eRESOURCE_TYPE_SAMPLER,
    };
}///
ResourceDescriptorMemoryRequirments CalculateResourceDescriptorMemoryRequirments( const ResourceLayout& layout )
{
    ResourceDescriptorMemoryRequirments requirments = {};

    requirments.descriptor_size = sizeof( ResourceDescriptorImpl );
    requirments.descriptor_size += layout.num_bindings * sizeof( u32 ); // hashed names
    requirments.descriptor_size += layout.num_bindings * sizeof( ResourceDescriptorImpl::Binding );

    for( u32 i = 0; i < layout.num_bindings; ++i )
    {
        const ResourceBinding binding = layout.bindings[i];
        requirments.data_size += _resource_size[binding.type];
    }
    return requirments;
}

ResourceDescriptor CreateResourceDescriptor( const ResourceLayout& layout, bxAllocator* allocator /*= nullptr */ )
{
    ResourceDescriptorMemoryRequirments mem_req = CalculateResourceDescriptorMemoryRequirments( layout );

    const u32 mem_size = mem_req.Total();
    u8* mem = (u8*)BX_MALLOC( utils::getAllocator( allocator ), mem_size, 16 );
    memset( mem, 0x00, mem_size );

    ResourceDescriptorImpl* impl = (ResourceDescriptorImpl*)mem;
    impl->count = layout.num_bindings;
    u32* hashed_names = (u32*)impl->HashedNames();
    ResourceDescriptorImpl::Binding* bindings = ( ResourceDescriptorImpl::Binding* )impl->Bindings();

    SYS_ASSERT( (uptr)( bindings + impl->count ) == (uptr)( (u8*)mem + mem_req.descriptor_size ) );

    for( u32 i = 0; i < impl->count; ++i )
    {
        const ResourceBinding& src_bind = layout.bindings[i];
        ResourceDescriptorImpl::Binding& dst_bind = bindings[i];
        dst_bind.binding_type = src_bind.type;
        dst_bind.slot = src_bind.slot;
        dst_bind.stage_mask = src_bind.stage_mask;
        dst_bind.data_offset = GenerateResourceHashedName( src_bind.name );;
    }

    struct BindingCmp
    {
        inline bool operator()( ResourceDescriptorImpl::Binding a, ResourceDescriptorImpl::Binding b )
        {
            const u32 keya = u32( a.binding_type ) << 24 | u32( a.slot ) << 16 | a.stage_mask;
            const u32 keyb = u32( b.binding_type ) << 24 | u32( b.slot ) << 16 | b.stage_mask;
            return keya < keyb;
        }
    };
    std::sort( bindings, bindings + impl->count, BindingCmp() );
    
    u32 data_offset = 0;
    for( u32 i = 0; i < impl->count; ++i )
    {
        hashed_names[i] = bindings[i].data_offset;
        bindings[i].data_offset = data_offset;
        data_offset += _resource_size[bindings[i].binding_type];
    }

    SYS_ASSERT( data_offset == mem_req.data_size );
    SYS_ASSERT( (uptr)( impl->Data() + data_offset ) == (uptr)( (u8*)mem + mem_size ) );

#ifdef _DEBUG
    for( u32 i = 0; i < impl->count; ++i )
    {
        for( u32 j = i + 1; j < impl->count; ++j )
        {
            SYS_ASSERT( hashed_names[i] != hashed_names[j] );
        }
    }
#endif

    return impl;
}

void DestroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator /*= nullptr */ )
{
    BX_DELETE0( utils::getAllocator( allocator ), rdesc[0] );
}

u32 GenerateResourceHashedName( const char* name )
{
    return murmur3_hash32( name, (u32)strlen( name ), bxTag32( "RDES" ) );
}

namespace
{
    u32 _FindResource( const ResourceDescriptorImpl* impl, const char* name )
    {
        const u32 hashed_name = GenerateResourceHashedName( name );
        const u32* resource_hashed_names = impl->HashedNames();
        for( u32 i = 0; i < impl->count; ++i )
        {
            if( hashed_name == resource_hashed_names[i] )
                return i;
        }
        return UINT32_MAX;
    }

    template< EBindingType::Enum B, typename T >
    void _SetResource1( ResourceDescriptorImpl* impl, u32 index, const T* resourcePtr )
    {
        SYS_ASSERT( index < impl->count );
        const ResourceDescriptorImpl::Binding& binding = impl->Bindings()[index];
        SYS_ASSERT( binding.binding_type == B );

        T* resource_data = (T*)(impl->Data() + binding.data_offset);
        resource_data[0] = *resourcePtr;
    }
    template< EBindingType::Enum B, typename T >
    bool _SetResource2( ResourceDescriptorImpl* impl, const char* name, const T* resourcePtr )
    {
        u32 index = _FindResource( impl, name );
        if( index == UINT32_MAX )
        {
            bxLogError( "Resource '%s' not found in descriptor", name );
            return false;
        }

        _SetResource1<B>( impl, index, resourcePtr );
        return true;
    }
}

u32 FindResource( ResourceDescriptor rdesc, const char* name )
{
    return _FindResource( rdesc, name );
}

void SetResourceROByIndex( ResourceDescriptor rdesc, u32 index, const ResourceRO* resource )
{
    _SetResource1<EBindingType::READ_ONLY>( rdesc, index, resource );
}

void SetResourceRWByIndex( ResourceDescriptor rdesc, u32 index, const ResourceRW* resource )
{
    _SetResource1<EBindingType::READ_WRITE>( rdesc, index, resource );
}

void SetConstantBufferByIndex( ResourceDescriptor rdesc, u32 index, const ConstantBuffer* cbuffer )
{
    _SetResource1<EBindingType::UNIFORM>( rdesc, index, cbuffer );
}

void SetSamplerByIndex( ResourceDescriptor rdesc, u32 index, const Sampler* sampler )
{
    _SetResource1<EBindingType::SAMPLER>( rdesc, index, sampler );
}

bool SetResourceRO( ResourceDescriptor rdesc, const char* name, const ResourceRO* resource )
{
    return _SetResource2<EBindingType::READ_ONLY>( rdesc, name, resource );
}
bool SetResourceRW( ResourceDescriptor rdesc, const char* name, const ResourceRW* resource )
{
    return _SetResource2<EBindingType::READ_WRITE>( rdesc, name, resource );
}
bool SetConstantBuffer( ResourceDescriptor rdesc, const char* name, const ConstantBuffer* cbuffer )
{
    return _SetResource2<EBindingType::UNIFORM>( rdesc, name, cbuffer );
}
bool SetSampler( ResourceDescriptor rdesc, const char* name, const Sampler* sampler )
{
    return _SetResource2<EBindingType::SAMPLER>( rdesc, name, sampler );
}

bool ClearResource( CommandQueue* cmdq, ResourceDescriptor rdesc, const char* name )
{
    u32 index = _FindResource( rdesc, name );
    if( index == UINT32_MAX )
    {
        bxLogError( "Resource '%s' not found in descriptor", name );
        return false;
    }

    const ResourceDescriptorImpl::Binding& binding = rdesc->Bindings()[index];
    switch( binding.binding_type )
    {
    case EBindingType::READ_ONLY:
        {
            ResourceRO null_resource = {};
            context::SetResourcesRO( cmdq, &null_resource, binding.slot, 1, binding.stage_mask );
        }break;
    case EBindingType::READ_WRITE:
        {
            ResourceRW null_resource = {};
            context::SetResourcesRW( cmdq, &null_resource, binding.slot, 1, binding.stage_mask );
        }break;
    case EBindingType::UNIFORM:
        {
            ConstantBuffer null_resource = {};
            context::SetCbuffers( cmdq, &null_resource, binding.slot, 1, binding.stage_mask );
        }break;
    case EBindingType::SAMPLER:
        {
            Sampler null_resource = {};
            context::SetSamplers( cmdq, &null_resource, binding.slot, 1, binding.stage_mask );
        }break;
    default:
        SYS_NOT_IMPLEMENTED;
        break;
    }
    return true;
}

void BindResources( CommandQueue* cmdq, ResourceDescriptor rdesc )
{
    const ResourceDescriptorImpl::Binding* bindings = rdesc->Bindings();
    u8* data = rdesc->Data();
    const u32 n = rdesc->count;

    for( u32 i = 0; i < n; ++i )
    {
        const ResourceDescriptorImpl::Binding b = bindings[i];
        u8* resource_data = data + b.data_offset;
        switch( b.binding_type )
        {
        case EBindingType::READ_ONLY:
            {
                ResourceRO* r = (ResourceRO*)resource_data;
                context::SetResourcesRO( cmdq, r, b.slot, 1, b.stage_mask );
            }break;
        case EBindingType::READ_WRITE:
            {
                ResourceRW* r = (ResourceRW*)resource_data;
                context::SetResourcesRW( cmdq, r, b.slot, 1, b.stage_mask );
            }break;
        case EBindingType::UNIFORM:
            {
                ConstantBuffer* r = (ConstantBuffer*)resource_data;
                context::SetCbuffers( cmdq, r, b.slot, 1, b.stage_mask );
            }break;
        case EBindingType::SAMPLER:
            {
                Sampler* r = (Sampler*)resource_data;
                context::SetSamplers( cmdq, r, b.slot, 1, b.stage_mask );
            }break;
        default:
            SYS_NOT_IMPLEMENTED;
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RenderSourceImpl
{
    u16 num_vertex_buffers = 0;
    u8 num_draw_ranges = 0;
    u8 has_shared_index_buffer = 0;
    IndexBuffer index_buffer;
    VertexBuffer* vertex_buffers = nullptr;
    RenderSourceRange* draw_ranges = nullptr;
};

RenderSource CreateRenderSource( const RenderSourceDesc& desc, bxAllocator* allocator /*= nullptr */ )
{
    const u32 num_streams = desc.vertex_layout.count;
    const u32 num_draw_ranges = maxOfPair( 1u, desc.num_draw_ranges );

    u32 mem_size = sizeof( RenderSourceImpl );
    mem_size += (num_streams)* sizeof( VertexBuffer );
    mem_size += num_draw_ranges * sizeof( RenderSourceRange );

    void* mem = BX_MALLOC( utils::getAllocator( allocator ), mem_size, ALIGNOF( RenderSourceImpl ) );
    memset( mem, 0x00, mem_size );

    bxBufferChunker chunker( mem, mem_size );
    RenderSourceImpl* impl = chunker.add< RenderSourceImpl >();
    impl->vertex_buffers = chunker.add< VertexBuffer >( num_streams );
    impl->draw_ranges = chunker.add< RenderSourceRange >( num_draw_ranges );
    chunker.check();

    impl->num_vertex_buffers = num_streams;
    impl->num_draw_ranges = num_draw_ranges;

    for( u32 i = 0; i < num_streams; ++i )
    {
        const void* data = desc.vertex_data[i];
        impl->vertex_buffers[i] = device::CreateVertexBuffer( desc.vertex_layout.descs[i], desc.num_vertices, data );
    }

    RenderSourceRange& default_range = impl->draw_ranges[0];
    if( desc.index_type != EDataType::UNKNOWN )
    {
        impl->index_buffer = device::CreateIndexBuffer( desc.index_type, desc.num_indices, desc.index_data );
        default_range.count = desc.num_indices;
        impl->has_shared_index_buffer = 0;
    }
    else if( desc.shared_index_buffer.id )
    {
        impl->index_buffer = desc.shared_index_buffer;
        impl->has_shared_index_buffer = 1;
        default_range.count = desc.num_indices;
    }
    else
    {
        default_range.count = desc.num_vertices;
    }

    for( u32 i = 1; i < num_draw_ranges; ++i )
    {
        impl->draw_ranges[i] = desc.draw_ranges[i - 1];
    }

    return impl;
}

void DestroyRenderSource( RenderSource* rsource, bxAllocator* allocator /*= nullptr */ )
{
    if( !rsource[0] )
        return;

    RenderSourceImpl* impl = rsource[0];

    if( impl->has_shared_index_buffer == 0 )
    {
        device::DestroyIndexBuffer( &impl->index_buffer );
    }
    for( u32 i = 0; i < impl->num_vertex_buffers; ++i )
    {
        device::DestroyVertexBuffer( &impl->vertex_buffers[i] );
    }

    BX_FREE0( utils::getAllocator( allocator ), rsource[0] );
}
void BindRenderSource( CommandQueue* cmdq, RenderSource renderSource )
{
    rdi::VertexBuffer* vbuffers = nullptr;
    u32 num_vbuffers = 0;

    rdi::IndexBuffer ibuffer = {};

    if( renderSource )
    {
        vbuffers     = renderSource->vertex_buffers;
        num_vbuffers = renderSource->num_vertex_buffers;
        ibuffer      = renderSource->index_buffer;
    }
    context::SetVertexBuffers( cmdq, vbuffers, 0, num_vbuffers );
    context::SetIndexBuffer( cmdq, ibuffer );
}

void SubmitRenderSource( CommandQueue* cmdq, RenderSource renderSource, u32 rangeIndex )
{
    SYS_ASSERT( rangeIndex < renderSource->num_draw_ranges );
    RenderSourceRange range = renderSource->draw_ranges[rangeIndex];
    
    if( renderSource->index_buffer.id )
        context::DrawIndexed( cmdq, range.count, range.begin, 0 );
    else
        context::Draw( cmdq, range.count, range.begin );
}

void SubmitRenderSourceInstanced( CommandQueue* cmdq, RenderSource renderSource, u32 numInstances, u32 rangeIndex /*= 0 */ )
{
    SYS_ASSERT( rangeIndex < renderSource->num_draw_ranges );
    RenderSourceRange range = renderSource->draw_ranges[rangeIndex];

    if( renderSource->index_buffer.id )
        context::DrawIndexedInstanced( cmdq, range.count, range.begin, numInstances, 0 );
    else
        context::DrawInstanced( cmdq, range.count, range.begin, numInstances );
}

u32 GetNVertexBuffers( RenderSource rsource ) { return rsource->num_vertex_buffers; }
u32 GetNVertices( RenderSource rsource ) { return rsource->vertex_buffers[0].numElements; }
u32 GetNIndices( RenderSource rsource ) { return rsource->index_buffer.numElements; }
u32 GetNRanges( RenderSource rsource ) { return rsource->num_draw_ranges; }
VertexBuffer GetVertexBuffer( RenderSource rsource, u32 index )
{
    SYS_ASSERT( index < rsource->num_vertex_buffers );
    return rsource->vertex_buffers[index];
}
IndexBuffer GetIndexBuffer( RenderSource rsource ) { return rsource->index_buffer; }
RenderSourceRange GetRange( RenderSource rsource, u32 index )
{
    SYS_ASSERT( index < rsource->num_draw_ranges );
    return rsource->draw_ranges[index];
}

RenderSource CreateRenderSourceFromPolyShape( const bxPolyShape& shape )
{
    const int nVertices = shape.nvertices();
    const int nIndices = shape.ntriangles() * 3;

    const float* pos = shape.positions;
    const float* nrm = shape.normals;
    const float* uvs = shape.texcoords;
    const u32* indices = shape.indices;

    RenderSourceDesc desc = {};
    desc.Count( nVertices, nIndices );
    desc.VertexBuffer( VertexBufferDesc( EVertexSlot::POSITION ).DataType( EDataType::FLOAT, 3 ), pos );
    desc.VertexBuffer( VertexBufferDesc( EVertexSlot::NORMAL ).DataType( EDataType::FLOAT, 3 ), nrm );
    desc.VertexBuffer( VertexBufferDesc( EVertexSlot::TEXCOORD0 ).DataType( EDataType::FLOAT, 2 ), uvs );
    desc.IndexBuffer( EDataType::UINT, indices );

    RenderSource rsource = CreateRenderSource( desc );
    
    return rsource;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
RenderSource CreateFullscreenQuad()
{
    const float vertices_pos[] =
    {
        -1.f, -1.f, 0.f,
        1.f , -1.f, 0.f,
        1.f , 1.f , 0.f,

        -1.f, -1.f, 0.f,
        1.f , 1.f , 0.f,
        -1.f, 1.f , 0.f,
    };
    const float vertices_uv[] =
    {
        0.f, 0.f,
        1.f, 0.f,
        1.f, 1.f,

        0.f, 0.f,
        1.f, 1.f,
        0.f, 1.f,
    };

    rdi::RenderSourceDesc rsource_desc = {};
    rsource_desc.Count( 6 );
    rsource_desc.VertexBuffer( rdi::VertexBufferDesc( rdi::EVertexSlot::POSITION ).DataType( rdi::EDataType::FLOAT, 3 ), vertices_pos );
    rsource_desc.VertexBuffer( rdi::VertexBufferDesc( rdi::EVertexSlot::TEXCOORD0 ).DataType( rdi::EDataType::FLOAT, 2 ), vertices_uv );
    rdi::RenderSource rsource = rdi::CreateRenderSource( rsource_desc );
    return rsource;
}

void DrawFullscreenQuad( CommandQueue* cmdq, RenderSource fsq )
{
    BindRenderSource( cmdq, fsq );
    SubmitRenderSource( cmdq, fsq );
}

}}///


namespace bx { namespace rdi {

    const DispatchFunction DrawCmd::DISPATCH_FUNCTION = DrawCmdDispatch;
    const DispatchFunction UpdateConstantBufferCmd::DISPATCH_FUNCTION = UpdateConstantBufferCmdDispatch;
    const DispatchFunction SetPipelineCmd::DISPATCH_FUNCTION = SetPipelineCmdDispatch;
    const DispatchFunction SetResourcesCmd::DISPATCH_FUNCTION = SetResourcesCmdDispatch;
    const DispatchFunction SetRenderSourceCmd::DISPATCH_FUNCTION = SetRenderSourceCmdDispatch;
    const DispatchFunction RawDrawCallCmd::DISPATCH_FUNCTION = RawDrawCallCmdDispatch;
    const DispatchFunction UpdateBufferCmd::DISPATCH_FUNCTION = UpdateBufferCmdDispatch;
    const DispatchFunction DrawCallbackCmd::DISPATCH_FUNCTION = DrawCallbackCmdDispatch;
    
    void SetPipelineCmdDispatch(  CommandQueue* cmdq, Command* cmdAddr )
    {
        SetPipelineCmd* cmd = (SetPipelineCmd*)cmdAddr;
        BindPipeline( cmdq, cmd->pipeline, cmd->bindResources ? true : false );
    }
    void SetResourcesCmdDispatch(  CommandQueue* cmdq, Command* cmdAddr )
    {
        SetResourcesCmd* cmd = (SetResourcesCmd*)cmdAddr;
        BindResources( cmdq, cmd->desc );
    }

    void SetRenderSourceCmdDispatch( CommandQueue * cmdq, Command * cmdAddr )
    {
        SetRenderSourceCmd* cmd = (SetRenderSourceCmd*)cmdAddr;
        BindRenderSource( cmdq, cmd->rsource );
    }

    void DrawCmdDispatch( CommandQueue* cmdq, Command* cmdAddr )
    {
        DrawCmd* cmd = (DrawCmd*)cmdAddr;
        BindRenderSource( cmdq, cmd->rsource );
        SubmitRenderSourceInstanced( cmdq, cmd->rsource, cmd->num_instances, cmd->rsouce_range );
    }

    void RawDrawCallCmdDispatch( CommandQueue * cmdq, Command * cmdAddr )
    {
        RawDrawCallCmd* cmd = (RawDrawCallCmd*)cmdAddr;
        context::DrawInstanced( cmdq, cmd->num_vertices, cmd->start_index, cmd->num_instances );
    }

    void UpdateConstantBufferCmdDispatch( CommandQueue* cmdq, Command* cmdAddr )
    {
        UpdateConstantBufferCmd* cmd = (UpdateConstantBufferCmd*)cmdAddr;
        context::UpdateCBuffer( cmdq, cmd->cbuffer, cmd->DataPtr() );
    }

    void UpdateBufferCmdDispatch( CommandQueue * cmdq, Command * cmdAddr )
    {
        UpdateBufferCmd* cmd = (UpdateBufferCmd*)cmdAddr;
        u8* mapped_ptr = context::Map( cmdq, cmd->resource, 0, EMapType::WRITE );
        memcpy( mapped_ptr, cmd->DataPtr(), cmd->size );
        context::Unmap( cmdq, cmd->resource );
    }

    void DrawCallbackCmdDispatch( CommandQueue * cmdq, Command * cmdAddr )
    {
        DrawCallbackCmd* cmd = (DrawCallbackCmd*)cmdAddr;
        ( *cmd->ptr )( cmdq, cmd->flags, cmd->user_data );
    }

        //////////////////////////////////////////////////////////////////////////
    struct CommandBufferImpl
    {
        struct CmdInternal
        {
            u64 key;
            Command* cmd;
        };

        struct Data
        {
            void* _memory_handle = nullptr;
            CmdInternal* commands = nullptr;
            u8* data = nullptr;
            
            u32 max_commands = 0;
            u32 num_commands = 0;
            u32 data_capacity = 0;
            u32 data_size = 0;
        }_data;

        union
        {
            u32 all = 0;
            struct  
            {
                u16 commands;
                u16 data;
            };
        } _overflow;

        u32 _can_add_commands = 0;
        
        void AllocateData( u32 maxCommands, u32 dataCapacity, bxAllocator* allocator );
        void FreeData( bxAllocator* allocator );
    };

    void CommandBufferImpl::AllocateData( u32 maxCommands, u32 dataCapacity, bxAllocator* allocator )
    {
        SYS_ASSERT( _can_add_commands == 0 );

        if( _data.max_commands >= maxCommands && _data.data_capacity >= dataCapacity )
            return;

        u32 mem_size = maxCommands * sizeof( CmdInternal );
        mem_size += dataCapacity;

        void* mem = BX_MALLOC( allocator, mem_size, 16 );
        memset( mem, 0x00, mem_size );

        Data newdata = {};
        newdata._memory_handle = mem;
        newdata.max_commands = maxCommands;
        newdata.data_capacity = dataCapacity;
        
        bxBufferChunker chunker( mem, mem_size );
        newdata.commands = chunker.add< CmdInternal >( maxCommands );
        newdata.data = chunker.addBlock( dataCapacity );
        chunker.check();

        FreeData( allocator );
        _data = newdata;
    }

    void CommandBufferImpl::FreeData( bxAllocator* allocator )
    {
        BX_FREE( allocator, _data._memory_handle );
        _data = {};
    }

    CommandBuffer CreateCommandBuffer( u32 maxCommands, u32 dataCapacity )
    {
        CommandBufferImpl* impl = BX_NEW( bxDefaultAllocator(), CommandBufferImpl );
        
        dataCapacity += maxCommands * sizeof( DrawCmd );
        impl->AllocateData( maxCommands, dataCapacity, bxDefaultAllocator() );
        return impl;
    }

    void DestroyCommandBuffer( CommandBuffer* cmdBuff )
    {
        CommandBufferImpl* impl = cmdBuff[0];
        impl->FreeData( bxDefaultAllocator() );
        BX_FREE0( bxDefaultAllocator(), cmdBuff[0] );
    }

    void ClearCommandBuffer( CommandBuffer cmdBuff )
    {
        if( cmdBuff->_overflow.all )
        {
            u32 maxCommands = ( cmdBuff->_overflow.commands ) ? cmdBuff->_data.max_commands * 2 : cmdBuff->_data.max_commands;
            u32 dataCapacity = ( cmdBuff->_overflow.data ) ? cmdBuff->_data.data_capacity * 2 : cmdBuff->_data.data_capacity;
            cmdBuff->AllocateData( maxCommands, dataCapacity, bxDefaultAllocator() );
            cmdBuff->_overflow.all = 0;
        }
        cmdBuff->_data.num_commands = 0;
        cmdBuff->_data.data_size = 0;
    }

    void BeginCommandBuffer( CommandBuffer cmdBuff )
    {
        cmdBuff->_can_add_commands = 1;
    }
    void EndCommandBuffer( CommandBuffer cmdBuff )
    {
        cmdBuff->_can_add_commands = 0;
    }
    void SubmitCommandBuffer( CommandQueue* cmdq, CommandBuffer cmdBuff )
    {
        SYS_ASSERT( cmdBuff->_can_add_commands == 0 );

        struct CmdInternalCmp{
            inline bool operator () ( const CommandBufferImpl::CmdInternal a, const CommandBufferImpl::CmdInternal b ) { return a.key < b.key; }
        };

        CommandBufferImpl::Data& data = cmdBuff->_data;
        std::sort( data.commands, data.commands + data.num_commands, CmdInternalCmp() );

        for( u32 i = 0; i < data.num_commands; ++i )
        {
            Command* cmd = data.commands[i].cmd;
            while( cmd )
            {
                ( *cmd->_dispatch_ptr )( cmdq, cmd );
                cmd = cmd->_next;
            }
        }
    }
    bool SubmitCommand( CommandBuffer cmdbuff, Command* cmdPtr, u64 sortKey )
    {
        SYS_ASSERT( cmdbuff->_can_add_commands );
        CommandBufferImpl::Data& data = cmdbuff->_data;
        if( data.num_commands >= data.max_commands )
            return false;

        u32 index = data.num_commands++;
        CommandBufferImpl::CmdInternal cmd_int = {};
        cmd_int.key = sortKey;
        cmd_int.cmd = cmdPtr;
        data.commands[index] = cmd_int;
        return true;
    }
    void* _AllocateCommand( CommandBuffer cmdbuff, u32 cmdSize )
    {
        SYS_ASSERT( cmdbuff->_can_add_commands );
        CommandBufferImpl::Data& data = cmdbuff->_data;
        if( data.data_size + cmdSize > data.data_capacity )
            return nullptr;

        u8* ptr = data.data + data.data_size;
        data.data_size += cmdSize;
        
        return ptr;
    }






}
}///