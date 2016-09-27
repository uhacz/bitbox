#include "renderer.h"
#include "util\common.h"
#include "util\buffer_utils.h"

namespace bx {
namespace gfx {
namespace utils
{
    inline bxAllocator* getAllocator( bxAllocator* defaultAllocator )
    {
        return ( defaultAllocator ) ? defaultAllocator : bxDefaultAllocator();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct PipelineImpl
{
    gdi::BlendState blend_state = {};
    gdi::DepthState depth_state = {};
    gdi::RasterState raster_state = {};
    gdi::InputLayout input_layout{};
    gdi::ETopology topology = gdi::eTRIANGLES;
    gdi::Shader      shaders[gdi::eDRAW_STAGES_COUNT] = {};
};
Pipeline createPipeline( const PipelineDesc& desc, bxAllocator* allocator )
{
    PipelineImpl* impl = (PipelineImpl*)BX_NEW( utils::getAllocator( allocator ), PipelineImpl );

    for( u32 i = 0; i < gdi::eDRAW_STAGES_COUNT; ++i )
    {
        impl->shaders[i] = desc.shaders[i];
    }

    impl->input_layout = gdi::create::inputLayout( desc.vertex_stream_descs, desc.num_vertex_stream_descs, desc.shaders[gdi::eSTAGE_VERTEX] );
    impl->blend_state = gdi::create::blendState( desc.hw_state_desc.blend );
    impl->depth_state = gdi::create::depthState( desc.hw_state_desc.depth );
    impl->raster_state = gdi::create::rasterState( desc.hw_state_desc.raster );
    impl->topology = desc.topology;

    return impl;
}


void destroyPipeline( Pipeline* pipeline, bxAllocator* allocator /*= nullptr */ )
{
    if( !pipeline[0] )
        return;

    PipelineImpl* impl = pipeline[0];
    gdi::release::rasterState( &impl->raster_state );
    gdi::release::depthState( &impl->depth_state );
    gdi::release::blendState( &impl->blend_state );
    gdi::release::inputLayout( &impl->input_layout );

    for( u32 i = 0; i < gdi::eDRAW_STAGES_COUNT; ++i )
    {
        impl->shaders[i] = {};
    }

    BX_DELETE( utils::getAllocator( allocator ), impl );

    pipeline[0] = nullptr;
}

void bindPipeline( gdi::CommandQueue* cmdq, Pipeline pipeline )
{
    gdi::set::shaderPrograms( cmdq, pipeline->shaders, gdi::eDRAW_STAGES_COUNT );
    gdi::set::inputLayout( cmdq, pipeline->input_layout );
    gdi::set::blendState( cmdq, pipeline->blend_state );
    gdi::set::depthState( cmdq, pipeline->depth_state );
    gdi::set::rasterState( cmdq, pipeline->raster_state );
    gdi::set::topology( cmdq, pipeline->topology );
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RenderPassImpl
{
    gdi::TextureRW color_textures[gdi::cMAX_RENDER_TARGETS] = {};
    gdi::TextureDepth depth_texture;
    u16 num_color_textures = 0;
};
RenderPass createRenderPass( const RenderPassDesc& desc, bxAllocator* allocator /*= nullptr */ )
{
    RenderPassImpl* impl = BX_NEW( utils::getAllocator( allocator ), RenderPassImpl );
    for( u32 i = 0; i < desc.num_color_textures; ++i )
    {
        impl->color_textures[i] = gdi::create::texture2D( desc.width, desc.height, desc.mips, desc.color_texture_formats[i], gdi::eBIND_RENDER_TARGET | gdi::eBIND_SHADER_RESOURCE, 0, nullptr );
    }
    impl->num_color_textures = desc.num_color_textures;

    if( desc.depth_texture_type != gdi::eTYPE_UNKNOWN )
    {
        impl->depth_texture = gdi::create::texture2Ddepth( desc.width, desc.height, desc.mips, desc.depth_texture_type );
    }

    return impl;
}

void destroyRenderPass( RenderPass* renderPass, bxGdiDeviceBackend* dev, bxAllocator* allocator /*= nullptr */ )
{
    if( renderPass[0] == nullptr )
        return;

    RenderPassImpl* impl = renderPass[0];

    if( impl->depth_texture.id )
    {
        gdi::release::texture( &impl->depth_texture );
    }

    for( u32 i = 0; i < impl->num_color_textures; ++i )
    {
        gdi::release::texture( &impl->color_textures[i] );
    }

    BX_DELETE( utils::getAllocator( allocator ), impl );
    renderPass[0] = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RenderSubPassImpl
{
    u8 color_texture_indices[gdi::cMAX_RENDER_TARGETS] = {};
    u16 num_color_textures = 0;
    u16 has_depth_texture = 0;
    RenderPass render_pass = nullptr;
};
RenderSubPass createRenderSubPass( const RenderSubPassDesc & desc, bxAllocator * allocator )
{
    SYS_ASSERT( desc.render_pass != nullptr );

    RenderSubPassImpl* impl = BX_NEW( utils::getAllocator( allocator ), RenderSubPassImpl );

    for( u32 i = 0; i < desc.num_color_textures; ++i )
    {
        SYS_ASSERT( desc.color_texture_indices[i] < desc.render_pass->num_color_textures );
        impl->color_texture_indices[i] = desc.color_texture_indices[i];
    }
    impl->num_color_textures = desc.num_color_textures;

    if( desc.has_depth_texture )
    {
        SYS_ASSERT( desc.render_pass->depth_texture.id != 0 );
        impl->has_depth_texture = desc.has_depth_texture;
    }

    impl->render_pass = desc.render_pass;
    return impl;
}

void destroyRenderSubPass( RenderSubPass * subPass, bxAllocator * allocator )
{
    BX_DELETE0( utils::getAllocator( allocator ), subPass[0] );
}

void bindRenderSubPass( gdi::CommandQueue* cmdq, RenderSubPass subPass )
{
    gdi::TextureRW color[gdi::cMAX_RENDER_TARGETS] = {};
    for( u32 i = 0; i < subPass->num_color_textures; ++i )
    {
        color[i] = subPass->render_pass->color_textures[subPass->color_texture_indices[i]];
    }
    gdi::TextureDepth depth = {};
    if( subPass->has_depth_texture )
    {
        depth = subPass->render_pass->depth_texture;
    }

    gdi::set::changeRenderTargets( cmdq, color, subPass->num_color_textures, depth );
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
    static const u32 _resource_size[_eRESOURCE_TYPE_COUNT_] =
    {
        sizeof( gdi::ResourceRO ),     //eRESOURCE_TYPE_READ_ONLY,
        sizeof( gdi::ResourceRW ),     //eRESOURCE_TYPE_READ_WRITE,
        sizeof( gdi::ConstantBuffer ), //eRESOURCE_TYPE_UNIFORM,
        sizeof( gdi::Sampler ),        //eRESOURCE_TYPE_SAMPLER,
    };
}///
ResourceDescriptor createResourceDescriptor( const ResourceLayout& layout, bxAllocator* allocator /*= nullptr */ )
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

void destroyResourceDescriptor( ResourceDescriptor* rdesc, bxAllocator* allocator /*= nullptr */ )
{
    BX_DELETE0( utils::getAllocator( allocator ), rdesc[0] );
}

namespace
{
    const ResourceDescriptorImpl::Desc* _FindResourceDesc( const ResourceDescriptorImpl* impl, EResourceType type, u8 stageMask, u8 slot )
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
bool setResourceRO( ResourceDescriptor rdesc, const gdi::ResourceRO* resource, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, eRESOURCE_TYPE_READ_ONLY, stageMask, slot );
    if( !desc )
        return false;

    _SetResource( rdesc, desc, resource, slot );
    return true;
}
bool setResourceRW( ResourceDescriptor rdesc, const gdi::ResourceRW* resource, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, eRESOURCE_TYPE_READ_WRITE, stageMask, slot );
    if( !desc )
        return false;

    _SetResource( rdesc, desc, resource, slot );
    return true;
}
bool setConstantBuffer( ResourceDescriptor rdesc, const gdi::ConstantBuffer cbuffer, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, eRESOURCE_TYPE_UNIFORM, stageMask, slot );
    if( !desc )
        return false;

    const gdi::ConstantBuffer* resource = &cbuffer;
    _SetResource( rdesc, desc, resource, slot );
    return true;
}
bool setSampler( ResourceDescriptor rdesc, const gdi::Sampler sampler, u8 stageMask, u8 slot )
{
    const ResourceDescriptorImpl::Desc* desc = _FindResourceDesc( rdesc, eRESOURCE_TYPE_SAMPLER, stageMask, slot );
    if( !desc )
        return false;

    const gdi::Sampler* resource = &sampler;
    _SetResource( rdesc, desc, resource, slot );
    return true;

}

void bindResources( gdi::CommandQueue* cmdq, ResourceDescriptor rdesc )
{
    const u32 n = rdesc->count;
    for( u32 i = 0; i < n; ++i )
    {
        const ResourceDescriptorImpl::Desc desc = rdesc->descs[i];
        const ResourceBinding binding = desc.binding;

        const u8* data_begin = rdesc->data + desc.offset;
        if( binding.type == eRESOURCE_TYPE_READ_ONLY )
        {
            gdi::ResourceRO* ro = ( gdi::ResourceRO* )data_begin;
            gdi::set::resourcesRO( cmdq, ro, binding.first_slot, binding.count, binding.stage_mask );
        }
        else if( binding.type == eRESOURCE_TYPE_READ_WRITE )
        {
            gdi::ResourceRW* rw = ( gdi::ResourceRW* )data_begin;
            gdi::set::resourcesRW( cmdq, rw, binding.first_slot, binding.count, binding.stage_mask );
        }
        else if( binding.type == eRESOURCE_TYPE_UNIFORM )
        {
            gdi::ConstantBuffer* cb = ( gdi::ConstantBuffer* )data_begin;
            gdi::set::cbuffers( cmdq, cb, binding.first_slot, binding.count, binding.stage_mask );
        }
        else if( binding.type == eRESOURCE_TYPE_SAMPLER )
        {
            gdi::Sampler* sampl = ( gdi::Sampler* )data_begin;
            gdi::set::samplers( cmdq, sampl, binding.first_slot, binding.count, binding.stage_mask );
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
    u32 num_vertex_buffers = 0;
    gdi::IndexBuffer index_buffer;
    gdi::VertexBuffer vertex_buffers[1];
};

RenderSource createRenderSource( const RenderSourceDesc& desc, bxAllocator* allocator /*= nullptr */ )
{
    u32 mem_size = sizeof( RenderSourceImpl );
    mem_size += ( desc.num_streams - 1 )  * sizeof( gdi::VertexBuffer );

    void* mem = BX_MALLOC( utils::getAllocator( allocator ), mem_size, ALIGNOF( RenderSourceImpl ) );
    memset( mem, 0x00, mem_size );

    RenderSourceImpl* impl = (RenderSourceImpl*)mem;
    impl->num_vertex_buffers = desc.num_streams;

    for( u32 i = 0; i < desc.num_streams; ++i )
    {
        const void* data = ( desc.streams_data ) ? desc.streams_data[i] : nullptr;
        impl->vertex_buffers[i] = gdi::create::vertexBuffer( desc.streams_desc[i], desc.num_vertices, data );
    }

    if( desc.index_type != gdi::eTYPE_UNKNOWN )
    {
        impl->index_buffer = gdi::create::indexBuffer( desc.index_type, desc.num_indices, desc.index_data );
    }

    return impl;    
}

void destroyRenderSource( RenderSource* rsource, bxAllocator* allocator /*= nullptr */ )
{
    if( !rsource[0] )
        return;

    RenderSourceImpl* impl = rsource[0];

    gdi::release::indexBuffer( &impl->index_buffer );
    for( u32 i = 0; i < impl->num_vertex_buffers; ++i )
    {
        gdi::release::vertexBuffer( &impl->vertex_buffers[i] );
    }

    BX_FREE0( utils::getAllocator( allocator ), rsource[0] );
}

}}///
