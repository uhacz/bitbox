#include "renderer.h"
#include "util\common.h"
#include "util\buffer_utils.h"
#include "renderer_scene.h"

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

    impl->input_layout = gdi::create::inputLayout( desc.vertex_layout.descs, desc.vertex_layout.count, desc.shaders[gdi::eSTAGE_VERTEX] );
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
    u16 num_vertex_buffers = 0;
    u16 num_draw_ranges = 0;
    gdi::IndexBuffer index_buffer;
    gdi::VertexBuffer* vertex_buffers = nullptr;
    RenderSourceRange* draw_ranges = nullptr;
};

RenderSource createRenderSource( const RenderSourceDesc& desc, bxAllocator* allocator /*= nullptr */ )
{
    const u32 num_streams = desc.vertex_layout.count;
    const u32 num_draw_ranges = maxOfPair( 1u, desc.num_draw_ranges );

    u32 mem_size = sizeof( RenderSourceImpl );
    mem_size += ( num_streams )  * sizeof( gdi::VertexBuffer );
    mem_size += num_draw_ranges * sizeof( RenderSourceRange );

    void* mem = BX_MALLOC( utils::getAllocator( allocator ), mem_size, ALIGNOF( RenderSourceImpl ) );
    memset( mem, 0x00, mem_size );

    bxBufferChunker chunker( mem, mem_size );
    RenderSourceImpl* impl = chunker.add< RenderSourceImpl >();
    impl->vertex_buffers = chunker.add< gdi::VertexBuffer >( num_streams );
    impl->draw_ranges = chunker.add< RenderSourceRange >( num_draw_ranges );
    chunker.check();
    
    impl->num_vertex_buffers = num_streams;
    impl->num_draw_ranges = num_draw_ranges;

    for( u32 i = 0; i < num_streams; ++i )
    {
        const void* data = desc.vertex_data[i];
        impl->vertex_buffers[i] = gdi::create::vertexBuffer( desc.vertex_layout.descs[i], desc.num_vertices, data );
    }

    RenderSourceRange& default_range = impl->draw_ranges[0];
    if( desc.index_type != gdi::eTYPE_UNKNOWN )
    {
        impl->index_buffer = gdi::create::indexBuffer( desc.index_type, desc.num_indices, desc.index_data );
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

u32 getNVertexBuffers( RenderSource rsource )
{
    return rsource->num_vertex_buffers;
}
u32 getNVertices( RenderSource rsource )
{
    return rsource->vertex_buffers[0].numElements;
}
u32 getNIndices( RenderSource rsource )
{
    return rsource->index_buffer.numElements;
}
u32 getNRanges( RenderSource rsource )
{
    return rsource->num_draw_ranges;
}
gdi::VertexBuffer getVertexBuffer( RenderSource rsource, u32 index )
{
    SYS_ASSERT( index < rsource->num_vertex_buffers );
    return rsource->vertex_buffers[index];
}
gdi::IndexBuffer getIndexBuffer( RenderSource rsource )
{
    return rsource->index_buffer;
}
RenderSourceRange getRange( RenderSource rsource, u32 index )
{
    SYS_ASSERT( index < rsource->num_draw_ranges );
    return rsource->draw_ranges[index];
}

}}///

#include <util/array.h>
#include <util/id_table.h>
#include <util/string_util.h>
namespace bx{
namespace gfx{
    static const ResourceBinding g_material_bindings[] = 
    {
        { eRESOURCE_TYPE_UNIFORM, gdi::eSTAGE_MASK_PIXEL, 3, 1 },

    };


//////////////////////////////////////////////////////////////////////////
struct GBuffer
{

};

struct MaterialContainer
{
    enum { eMAX_COUNT = 128, };
    id_table_t< eMAX_COUNT > _id_to_index;
    id_t                    _index_to_id[eMAX_COUNT] = {};
    //Pipeline                _pipelines[eMAX_COUNT] = {};
    ResourceDescriptor      _rdescs[eMAX_COUNT] = {};
    const char*             _names[eMAX_COUNT] = {};

    inline id_t makeId( Material m ) const { return make_id( m.i ); }
    inline Material makeMaterial( id_t id ) const { Material m; m.i = id.hash; return m; }

    bool alive( Material m ) const
    {
        id_t id = makeId( m );
        return id_table::has( _id_to_index, id );
    }

    Material add( const char* name, Pipeline pipeline, ResourceDescriptor rdesc )
    {
        id_t id = id_table::create( _id_to_index );
        u32 index = id.index;

        _index_to_id[index] = id;
        _pipelines[index] = pipeline;
        _rdescs[index] = rdesc;
        _names[index] = string::duplicate( nullptr, name );

        return makeMaterial( id );
    }
    void remove( Material* m )
    {
        id_t id = makeId( *m );
        if( !id_table::has( _id_to_index, id ) )
            return;

        u32 index = id.index;
        string::free_and_null( (char**)&_names[index] );
        _rdescs[index] = BX_GFX_NULL_HANDLE;
        _pipelines[index] = BX_GFX_NULL_HANDLE;
        _index_to_id[index] = makeInvalidHandle<id_t>();

        m[0] = makeMaterial( makeInvalidHandle<id_t>() );
    }

    Material find( const char* name )
    {
        const u32 n = id_table::size( _id_to_index );
        u32 counter = 0;
        for( u32 i = 0; i < n && counter < n; ++i )
        {
            const char* current_name = _names[i];
            if( !current_name )
                continue;

            if( string::equal( current_name, name ) )
            {
                SYS_ASSERT( id_table::has( _id_to_index, _index_to_id[i] ) );
                return makeMaterial( _index_to_id[i] );
            }

            ++counter;
        }
        return makeMaterial( makeInvalidHandle<id_t>() );
    }

    inline u32 _GetIndex( Material m )
    {
        id_t id = makeId( m );
        SYS_ASSERT( id_table::has( _id_to_index, id ) );
        return id.index;
    }
    Pipeline getPipeline( Material m )
    {
        u32 index = _GetIndex( m );
        return _pipelines[index];
    }
    ResourceDescriptor getResourceDesc( Material m )
    {
        u32 index = _GetIndex( m );
        return _rdescs[index];
    }
};

struct SharedMeshContainer
{
    struct Entry
    {
        const char* name = nullptr;
        RenderSource rsource = BX_GFX_NULL_HANDLE;
    };
    array_t< Entry > _entries;

    u32 add( const char* name, RenderSource rs )
    {
        u32 index = find( name );
        if( index != UINT32_MAX )
            return index;

        Entry e;
        e.name = string::duplicate( nullptr, name );
        e.rsource = rs;
        return array::push_back( _entries, e );
    }
    void remove( u32 index )
    {
        SYS_ASSERT( index < array::sizeu( _entries ) );

        Entry& e = _entries[index];
        string::free( (char*)e.name );

        array::erase( _entries, index );
    }
    u32 find( const char* name )
    {
        for( u32 i = 0; i < array::sizeu( _entries ); ++i )
        {
            if( string::equal( name, _entries[i].name ) )
                return i;
        }
        return UINT32_MAX;
    }

    RenderSource getRenderSource( u32 index )
    {
        SYS_ASSERT( index < array::sizeu( _entries ) );
        return _entries[index].rsource;
    }

};


namespace renderer
{
    static SharedMeshContainer g_mesh_container = {};
    static MaterialContainer g_material_container = {};
    
//////////////////////////////////////////////////////////////////////////
void startup()
{

}

void shutdown()
{

}

Material createMaterial( const char* name, const MaterialDesc& desc )
{
    //Pipeline pipeline = createPipeline( pipelineDesc, nullptr );
    //ResourceDescriptor resourceDesc = createResourceDescriptor( resourceLayout, nullptr );
    //return g_material_container.add( name, pipeline, resourceDesc );
}

void destroyMaterial( Material* m )
{
    if( !g_material_container.alive( *m ) )
        return;

    //Pipeline pipeline = g_material_container.getPipeline( *m );
    //ResourceDescriptor rdesc = g_material_container.getResourceDesc( *m );
    g_material_container.remove( m );

    //destroyResourceDescriptor( &rdesc, nullptr );
    //destroyPipeline( &pipeline, nullptr );
}

Material findMaterial( const char* name )
{
    return g_material_container.find( name );
}

void addSharedRenderSource( const char* name, RenderSource rsource )
{
    g_mesh_container.add( name, rsource );
}

RenderSource findSharedRenderSource( const char* name )
{
    u32 index = g_mesh_container.find( name );
    return ( index != UINT32_MAX ) ? g_mesh_container.getRenderSource( index ) : BX_GFX_NULL_HANDLE;
}

Scene createScene( const char* name )
{
    SceneImpl* impl = BX_NEW( bxDefaultAllocator(), SceneImpl );
    impl->prepare( name, nullptr );
    return impl;
}

void destroyScene( Scene* scene )
{
    SceneImpl* impl = scene[0];
    if( !impl )
        return;

    impl->unprepare();
    BX_DELETE0( bxDefaultAllocator(), scene[0] );
}

}///

}}///
