#include "rdi.h"

#include <util/hash.h>
#include <util/memory.h>

#include <resource_manager/resource_manager.h>
#include "rdi_shader_reflection.h"
#include "util/buffer_utils.h"
#include "util/common.h"

namespace bx { namespace rdi {
namespace utils
{
    inline bxAllocator* getAllocator( bxAllocator* defaultAllocator )
    {
        return ( defaultAllocator ) ? defaultAllocator : bxDefaultAllocator();
    }
}
}}


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
struct PipelineImpl
{
    HardwareState   hw_state = {};
    InputLayout     input_layout = {};
    ShaderPass      shader_pass = {};
    ETopology::Enum topology = ETopology::TRIANGLES;
};
Pipeline CreatePipeline( const PipelineDesc& desc, bxAllocator* allocator /*= nullptr */ )
{
    PipelineImpl* impl = (PipelineImpl*)BX_NEW( utils::getAllocator( allocator ), PipelineImpl );

    ShaderReflection shader_reflection = {};
    {
        const u32 pass_index = ShaderFileFindPass( desc.shader_file, desc.shader_pass_name );
        SYS_ASSERT( pass_index != UINT32_MAX );

        const ShaderFile::Pass& pass = desc.shader_file->passes[pass_index];
        ShaderPassCreateInfo pass_create_info = {};
        pass_create_info.vertex_bytecode = TYPE_OFFSET_GET_POINTER( void, pass.offset_bytecode_vertex );
        pass_create_info.vertex_bytecode_size = pass.size_bytecode_vertex;
        pass_create_info.pixel_bytecode = TYPE_OFFSET_GET_POINTER( void, pass.offset_bytecode_pixel );
        pass_create_info.pixel_bytecode_size = pass.size_bytecode_pixel;
        pass_create_info.reflection = &shader_reflection;
        impl->shader_pass = device::CreateShaderPass( pass_create_info );
    }

    impl->input_layout = device::CreateInputLayout( shader_reflection.vertex_layout, impl->shader_pass );
    impl->hw_state = device::CreateHardwareState( desc.hw_state_desc );
    impl->topology = desc.topology;

    return impl;
}

void DestroyPipeline( Pipeline* pipeline, bxAllocator* allocator /*= nullptr */ )
{
    if( !pipeline[0] )
        return;

    PipelineImpl* impl = pipeline[0];
    device::DestroyHardwareState( &impl->hw_state );
    device::DestroyInputLayout( &impl->input_layout );
    device::DestroyShaderPass( &impl->shader_pass );

    BX_DELETE0( utils::getAllocator( allocator ), pipeline[0] );
}

void BindPipeline( CommandQueue* cmdq, Pipeline pipeline )
{
    context::SetShaderPass ( cmdq, pipeline->shader_pass );
    context::SetInputLayout( cmdq, pipeline->input_layout );
    context::SetHardwareState( cmdq, pipeline->hw_state );
    context::SetTopology( cmdq, pipeline->topology );
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct BIT_ALIGNMENT_16 ResourceDescriptorImpl
{
    struct Desc
    {
        ResourceBinding binding;
        u32 offset = UINT32_MAX;
    };
    Desc* descs = nullptr;
    u8* data = nullptr;
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
ResourceDescriptor CreateResourceDescriptor( const ResourceLayout& layout, bxAllocator* allocator /*= nullptr */ )
{
    u32 mem_size = sizeof( ResourceDescriptorImpl );
    mem_size += layout.num_bindings * sizeof( ResourceDescriptorImpl::Desc );

    u32 data_size = 0;
    for( u32 i = 0; i < layout.num_bindings; ++i )
    {
        const ResourceBinding binding = layout.bindings[i];
        data_size += binding.count * _resource_size[binding.type];
    }
    mem_size += data_size;

    u8* mem = (u8*)BX_MALLOC( utils::getAllocator( allocator ), mem_size, 16 );
    memset( mem, 0x00, mem_size );

    bxBufferChunker chunker( mem, mem_size );

    ResourceDescriptorImpl* impl = chunker.add< ResourceDescriptorImpl >();
    impl->descs = chunker.add< ResourceDescriptorImpl::Desc >( layout.num_bindings );
    impl->data = chunker.addBlock( data_size );

    chunker.check();

    bxBufferChunker data_chunker( impl->data, data_size );
    for( u32 i = 0; i < layout.num_bindings; ++i )
    {
        const ResourceBinding binding = layout.bindings[i];
        ResourceDescriptorImpl::Desc& desc = impl->descs[i];
        u8* desc_data_ptr = data_chunker.addBlock( binding.count * _resource_size[binding.type] );

        desc.binding = binding;
        ptrdiff_t data_offset = (ptrdiff_t)desc_data_ptr - (ptrdiff_t)impl->data;
        SYS_ASSERT( data_offset >= 0 );
        SYS_ASSERT( data_offset < data_size );
        desc.offset = (u32)( data_offset );
    }

    data_chunker.check();

    return impl;
}

void DestroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator /*= nullptr */ )
{
    BX_DELETE0( utils::getAllocator( allocator ), rdesc[0] );
}

namespace
{
    const ResourceDescriptorImpl::Desc* _FindResourceDesc( const ResourceDescriptorImpl* impl, EBindingType::Enum type, u8 stageMask, u8 slot )
    {
        for( u32 i = 0; i < impl->count; ++i )
        {
            const ResourceDescriptorImpl::Desc* desc = &impl->descs[i];
            if( desc->binding.type == type && desc->binding.stage_mask == stageMask )
            {
                const u16 begin_slot = desc->binding.first_slot;
                const u16 end_slot = begin_slot + desc->binding.count;
                const bool slot_in_range = slot >= begin_slot && slot < end_slot;
                if( slot_in_range )
                {
                    return desc;
                }
            }
        }

        bxLogWarning( "Resource not found in descriptor" );
        return nullptr;
    }

    template< class T >
    void _SetResource( ResourceDescriptorImpl* impl, const ResourceDescriptorImpl::Desc* desc, const T* resourcePtr, u8 slot )
    {
        T* ro = (T*)( impl->data + desc->offset );
        const int local_slot = (int)slot - (int)desc->binding.first_slot;
        SYS_ASSERT( local_slot >= 0 && local_slot < desc->binding.count );
        ro[local_slot] = *resourcePtr;
    }
}
bool SetResourceRO( ResourceDescriptor rdesc, const ResourceRO* resource, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, EBindingType::READ_ONLY, stageMask, slot );
    if( !desc )
        return false;

    _SetResource( rdesc, desc, resource, slot );
    return true;
}
bool SetResourceRW( ResourceDescriptor rdesc, const ResourceRW* resource, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, EBindingType::READ_WRITE, stageMask, slot );
    if( !desc )
        return false;

    _SetResource( rdesc, desc, resource, slot );
    return true;
}
bool SetConstantBuffer( ResourceDescriptor rdesc, const ConstantBuffer cbuffer, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, EBindingType::UNIFORM, stageMask, slot );
    if( !desc )
        return false;

    const ConstantBuffer* resource = &cbuffer;
    _SetResource( rdesc, desc, resource, slot );
    return true;
}
bool SetSampler( ResourceDescriptor rdesc, const Sampler sampler, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, EBindingType::SAMPLER, stageMask, slot );
    if( !desc )
        return false;

    const Sampler* resource = &sampler;
    _SetResource( rdesc, desc, resource, slot );
    return true;

}

void BindResources( CommandQueue* cmdq, ResourceDescriptor rdesc )
{
    const u32 n = rdesc->count;
    for( u32 i = 0; i < n; ++i )
    {
        const ResourceDescriptorImpl::Desc desc = rdesc->descs[i];
        const ResourceBinding binding = desc.binding;

        const u8* data_begin = rdesc->data + desc.offset;
        if( binding.type == EBindingType::READ_ONLY )
        {
            ResourceRO* ro = ( ResourceRO* )data_begin;
            context::SetResourcesRO( cmdq, ro, binding.first_slot, binding.count, binding.stage_mask );
        }
        else if( binding.type == EBindingType::READ_WRITE )
        {
            ResourceRW* rw = ( ResourceRW* )data_begin;
            context::SetResourcesRW( cmdq, rw, binding.first_slot, binding.count, binding.stage_mask );
        }
        else if( binding.type == EBindingType::UNIFORM )
        {
            ConstantBuffer* cb = ( ConstantBuffer* )data_begin;
            context::SetCbuffers( cmdq, cb, binding.first_slot, binding.count, binding.stage_mask );
        }
        else if( binding.type == EBindingType::SAMPLER )
        {
            Sampler* sampl = ( Sampler* )data_begin;
            context::SetSamplers( cmdq, sampl, binding.first_slot, binding.count, binding.stage_mask );
        }
        else
        {
            SYS_ASSERT( false );
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RenderSourceImpl
{
    u16 num_vertex_buffers = 0;
    u16 num_draw_ranges = 0;
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

    device::DestroyIndexBuffer( &impl->index_buffer );
    for( u32 i = 0; i < impl->num_vertex_buffers; ++i )
    {
        device::DestroyVertexBuffer( &impl->vertex_buffers[i] );
    }

    BX_FREE0( utils::getAllocator( allocator ), rsource[0] );
}
void BindRenderSource( CommandQueue* cmdq, RenderSource renderSource )
{
    context::SetVertexBuffers( cmdq, renderSource->vertex_buffers, 0, renderSource->num_vertex_buffers );
    context::SetIndexBuffer( cmdq, renderSource->index_buffer );
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
}}///
