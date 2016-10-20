#include "rdi.h"

#include <util/hash.h>
#include <util/memory.h>

#include <resource_manager/resource_manager.h>
#include "rdi_shader_reflection.h"
#include "util/buffer_utils.h"
#include "util/common.h"
#include <algorithm>

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
    struct Binding
    {
        u8 binding_type = EBindingType::_COUNT_;
        u8 slot = 0;
        u16 stage_mask = 0;
        u32 data_offset = 0;
    };

    //struct Desc
    //{
    //    ResourceBinding binding;
    //    u32 offset = UINT32_MAX;
    //};

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

    SYS_ASSERT( (uptr)( bindings + impl->count ) == (uptr)( (u8*)mem + mem_size ) );

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

    template< typename T >
    void _SetResource1( ResourceDescriptorImpl* impl, u32 index, const T* resourcePtr )
    {
        SYS_ASSERT( index < impl->count );
        const ResourceDescriptorImpl::Binding& binding = impl->Bindings()[index];
        T* resource_data = (T*)(impl->Data() + binding.data_offset);
        resource_data[0] = *resourcePtr;
    }
    template< typename T >
    bool _SetResource2( ResourceDescriptorImpl* impl, const char* name, const T* resourcePtr )
    {
        u32 index = _FindResource( impl, name );
        if( index == UINT32_MAX )
        {
            bxLogError( "Resource '%s' not found in descriptor", name );
            return false;
        }

        _SetResource1( impl, index, resourcePtr );
        return true;
    }
}
bool SetResourceRO( ResourceDescriptor rdesc, const char* name, const ResourceRO* resource )
{
    return _SetResource2( rdesc, name, resource );
}
bool SetResourceRW( ResourceDescriptor rdesc, const char* name, const ResourceRW* resource )
{
    return _SetResource2( rdesc, name, resource );
}
bool SetConstantBuffer( ResourceDescriptor rdesc, const char* name, const ConstantBuffer* cbuffer )
{
    return _SetResource2( rdesc, name, cbuffer );
}
bool SetSampler( ResourceDescriptor rdesc, const char* name, const Sampler* sampler )
{
    return _SetResource2( rdesc, name, sampler );
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
