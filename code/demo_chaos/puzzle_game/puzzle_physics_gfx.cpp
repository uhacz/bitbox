#include "puzzle_physics_gfx.h"
#include "puzzle_physics_internal.h"
#include "../renderer_scene.h"
#include "../renderer_material.h"
#include <resource_manager\resource_manager.h>
#include <util/color.h>
#include <util/camera.h>

namespace bx { namespace puzzle {
namespace physics
{

struct Gfx;
struct GfxDrawData
{
    Gfx* gfx = nullptr;
    u32 index = UINT32_MAX;
};

struct GfxFrameData
{
    Matrix4 _camera_world;
    Matrix4 _viewProj;
    float _point_size;
    f32 __padding__[3];
};
struct GfxMaterialData
{
    Vector4F color;
};

struct Gfx
{
    gfx::ActorID            id_scene  [EConst::MAX_BODIES] = {};
    BodyId                  id_body   [EConst::MAX_BODIES] = {};
    u32                     color     [EConst::MAX_BODIES] = {};
    rdi::ResourceDescriptor gpu_rdesc [EConst::MAX_BODIES] = {};
    rdi::BufferRO           gpu_buffer[EConst::MAX_BODIES] = {};
    GfxDrawData             draw_data [EConst::MAX_BODIES] = {};
    u32                     size = 0;
    
    gfx::Scene   scene  = nullptr;
    Solver*      solver = nullptr;

    rdi::Pipeline pipeline = BX_RDI_NULL_HANDLE;
    rdi::Pipeline pipeline_shadow = BX_RDI_NULL_HANDLE;

    rdi::ConstantBuffer cbuffer_fdata = {};
    rdi::ConstantBuffer cbuffer_fdata_shadow = {};
    rdi::ConstantBuffer cbuffer_mdata = {};

    rdi::ResourceLayout rlayout_fdata = {};
    rdi::ResourceLayout rlayout_mdata = {};
    rdi::ResourceDescriptor rdesc_fdata = {};
};

namespace GfxGpuResource
{
    static const rdi::ResourceBinding bindings_fdata[] =
    {
        rdi::ResourceBinding( "FrameData", rdi::EBindingType::UNIFORM ).Slot( SLOT_FRAME_DATA ).StageMask( rdi::EStage::PIXEL_MASK|rdi::EStage::VERTEX_MASK ),
    };
    static const u32 bindings_fdata_count = sizeof( bindings_fdata ) / sizeof( *bindings_fdata );

    static const rdi::ResourceBinding bindings_mdata[] =
    {
        rdi::ResourceBinding( "_particle_data", rdi::EBindingType::READ_ONLY ).Slot( SLOT_INSTANCE_PARTICLE_DATA ).StageMask( rdi::EStage::VERTEX_MASK ),
        rdi::ResourceBinding( "MaterialData", rdi::EBindingType::UNIFORM ).Slot( SLOT_MATERIAL_DATA ).StageMask( rdi::EStage::PIXEL_MASK ),
    };
    static const u32 bindings_mdata_count = sizeof( bindings_mdata ) / sizeof( *bindings_mdata );
}//


static void StartUp( Gfx* gfx )
{
    rdi::ShaderFile* sfile = rdi::ShaderFileLoad( "shader/bin/deffered_particles.shader", GResourceManager() );
    rdi::PipelineDesc pipeline_desc = {};
    pipeline_desc.topology = rdi::ETopology::TRIANGLE_STRIP;

    pipeline_desc.Shader( sfile, "simple" );
    gfx->pipeline = rdi::CreatePipeline( pipeline_desc );
    SYS_ASSERT( gfx->pipeline != BX_RDI_NULL_HANDLE );

    pipeline_desc.Shader( sfile, "shadow" );
    gfx->pipeline_shadow = rdi::CreatePipeline( pipeline_desc );
    SYS_ASSERT( gfx->pipeline_shadow != BX_RDI_NULL_HANDLE );

    rdi::ShaderFileUnload( &sfile, GResourceManager() );

    gfx->cbuffer_fdata        = rdi::device::CreateConstantBuffer( sizeof( GfxFrameData ) );
    gfx->cbuffer_fdata_shadow = rdi::device::CreateConstantBuffer( sizeof( GfxFrameData ) );
    gfx->cbuffer_mdata        = rdi::device::CreateConstantBuffer( sizeof( GfxMaterialData ) );
    gfx->rlayout_fdata = rdi::ResourceLayout( GfxGpuResource::bindings_fdata, GfxGpuResource::bindings_fdata_count );
    gfx->rlayout_mdata = rdi::ResourceLayout( GfxGpuResource::bindings_mdata, GfxGpuResource::bindings_mdata_count );

    gfx->rdesc_fdata = rdi::CreateResourceDescriptor( gfx->rlayout_fdata );
    rdi::SetConstantBuffer( gfx->rdesc_fdata, "FrameData", &gfx->cbuffer_fdata );

    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( gfx->pipeline_shadow );
    rdi::SetConstantBuffer( rdesc, "FrameData", &gfx->cbuffer_fdata_shadow );

}

static void ShutDown( Gfx* gfx )
{
    rdi::device::DestroyConstantBuffer( &gfx->cbuffer_mdata );
    rdi::device::DestroyConstantBuffer( &gfx->cbuffer_fdata_shadow );
    rdi::device::DestroyConstantBuffer( &gfx->cbuffer_fdata );
    rdi::DestroyResourceDescriptor( &gfx->rdesc_fdata );
    
    rdi::DestroyPipeline( &gfx->pipeline_shadow );
    rdi::DestroyPipeline( &gfx->pipeline );

    for( u32 i = 0; i < gfx->size; ++i )
    {
        rdi::device::DestroyBufferRO( &gfx->gpu_buffer[i] );
        rdi::DestroyResourceDescriptor( &gfx->gpu_rdesc[i] );
        bxLogError( "Unreleased physics::Gfx entity!!" );
    }
}

static void DrawCallback( rdi::CommandQueue* cmdq, u32 flags, void* userData )
{
    GfxDrawData* ddata = (GfxDrawData*)userData;
    Gfx* gfx = ddata->gfx;
    Solver* solver = gfx->solver;

    const u32 index = ddata->index;
    BodyId id = gfx->id_body[index];
    const u32 num_particles = GetNbParticles( solver, id );
    
    if( flags & gfx::ESceneDrawFlag::COLOR )
    {
        Vector4F color_4f;
        bxColor::u32ToFloat4( gfx->color[index], &color_4f.mX );
        rdi::context::UpdateCBuffer( cmdq, gfx->cbuffer_mdata, &color_4f );

        rdi::BindPipeline( cmdq, gfx->pipeline, false );
        rdi::BindResources( cmdq, gfx->rdesc_fdata );
    }
    else if( flags & gfx::ESceneDrawFlag::SHADOW )
    {
        rdi::BindPipeline( cmdq, gfx->pipeline_shadow, true );
    }

    rdi::BindResources( cmdq, gfx->gpu_rdesc[index] );
    rdi::context::SetVertexBuffers( cmdq, nullptr, 0, 0 );
    rdi::context::SetIndexBuffer( cmdq, rdi::IndexBuffer() );
    rdi::context::DrawInstanced( cmdq, 4, 0, num_particles );
}

void Create( Gfx** gfx, Solver* solver, gfx::Scene scene )
{
    Gfx* g = BX_NEW( bxDefaultAllocator(), Gfx );
    g->scene = scene;
    g->solver = solver;
    StartUp( g );

    gfx[0] = g;
}
void Destroy( Gfx** gfx )
{
    if( !gfx[0] )
        return;

    ShutDown( gfx[0] );

    BX_DELETE0( bxDefaultAllocator(), gfx[0] );
}

static u32 AddInternal( Gfx* gfx, const char* name, u32 numParticles )
{
    SYS_ASSERT( gfx->size < EConst::MAX_BODIES );

    const u32 index = gfx->size++;

    rdi::BufferRO gpu_buffer = rdi::device::CreateBufferRO( numParticles, rdi::Format( rdi::EDataType::FLOAT, 3 ), rdi::ECpuAccess::WRITE, rdi::EGpuAccess::READ );
    gfx->gpu_buffer[index] = gpu_buffer;

    rdi::ResourceDescriptor rdesc = rdi::CreateResourceDescriptor( gfx->rlayout_mdata );
    rdi::SetResourceRO( rdesc, "_particle_data", &gpu_buffer );
    rdi::SetConstantBuffer( rdesc, "MaterialData", &gfx->cbuffer_mdata );
    gfx->gpu_rdesc[index] = rdesc;


    GfxDrawData& ddata = gfx->draw_data[index];
    ddata.gfx = gfx;
    ddata.index = index;

    gfx::ActorID scene_id = gfx->scene->Add( name, 1 );
    gfx->scene->SetSceneCallback( scene_id, DrawCallback, &ddata );
    gfx->id_scene[index] = scene_id;

    return index;
}

u32 AddBody( Gfx* gfx, BodyId id )
{
    if( !IsBodyAlive( gfx->solver, id ) )
        return UINT32_MAX;
    
    const u32 num_particles = GetNbParticles( gfx->solver, id );

    char name[64];
    snprintf( name, 64, "PhysicsBody%u", gfx->size );
   
    const u32 index = AddInternal( gfx, name, num_particles );
    gfx->id_body[index] = id;

    return index;
}

u32 AddActor( Gfx* gfx, u32 numParticles, u32 colorRGBA /*= 0xFFFFFFFF */ )
{
    char name[64];
    snprintf( name, 64, "PhysicsBody%u", gfx->size );

    const u32 index = AddInternal( gfx, name, numParticles );
    gfx->color[index] = colorRGBA;

    gfx->id_body[index].i = 0;

    return index;
}

u32 FindBody( Gfx* gfx, BodyId id )
{
    u32 index = UINT32_MAX;
    for( u32 i = 0; i < gfx->size; ++i )
    {
        if( gfx->id_body[i] == id )
        {
            index = i;
            break;
        }
    }
    return index;
}

void SetColor( Gfx* gfx, BodyId id, u32 colorRGBA )
{
    const u32 index = FindBody( gfx, id );
    if( index != UINT32_MAX )
        gfx->color[index] = colorRGBA;
}

void SetParticleData( Gfx* gfx, rdi::CommandQueue* cmdq, u32 index, const Vector3F* pdata, u32 count )
{
    if( index >= gfx->size )
    {
        bxLogError( "physics::Gfx: Invalid index" );
        return;
    }

    const rdi::BufferRO gpu_buffer = gfx->gpu_buffer[index];
    const u32 gpu_buffer_size = rdi::util::GetNumElements( gpu_buffer );
    const u32 elements_to_copy = minOfPair( gpu_buffer_size, count );
    
    SYS_ASSERT( sizeof( *pdata ) == gpu_buffer.format.ByteWidth() );
    const u32 bytes_to_copy = elements_to_copy * gpu_buffer.format.ByteWidth();

    u8* gpu_mapped_data = rdi::context::Map( cmdq, gpu_buffer, 0, rdi::EMapType::WRITE );
        memcpy( gpu_mapped_data, pdata, bytes_to_copy );
    rdi::context::Unmap( cmdq, gpu_buffer );
}

void Tick( Gfx* gfx, rdi::CommandQueue* cmdq, const gfx::Camera& camera, const Matrix4& lightWorld, const Matrix4& lightProj )
{
    // do garbage collection

    {// set gpu data
        Solver* solver = gfx->solver;
        
        // --- frame data
        GfxFrameData fdata;
        memset( &fdata, 0x00, sizeof( fdata ) );
        fdata._camera_world = camera.world;
        fdata._viewProj = camera.view_proj;
        fdata._point_size = GetParticleRadius( solver );
        rdi::context::UpdateCBuffer( cmdq, gfx->cbuffer_fdata, &fdata );

        GfxFrameData fdata_shadow;
        memset( &fdata_shadow, 0x00, sizeof( fdata_shadow ) );
        fdata_shadow._camera_world = lightWorld;
        fdata_shadow._viewProj = gfx::cameraMatrixProjectionDx11( lightProj ) * inverse( lightWorld );
        fdata_shadow._point_size = fdata._point_size;
        rdi::context::UpdateCBuffer( cmdq, gfx->cbuffer_fdata_shadow, &fdata_shadow );

        // --- entity data
        for( u32 i = 0; i < gfx->size; ++i )
        {
            BodyId body_id = gfx->id_body[i];
            if( !IsBodyAlive( solver, body_id ) )
                continue;

            rdi::BufferRO gpu_buffer = gfx->gpu_buffer[i];

            const u32 num_particles = GetNbParticles( solver, body_id );
            Vector3F* particle_data = MapInterpolatedPositions( solver, body_id );
            u8* gpu_mapped_data = rdi::context::Map( cmdq, gpu_buffer, 0, rdi::EMapType::WRITE );
            
            memcpy( gpu_mapped_data, particle_data, num_particles * sizeof( *particle_data ) );            
            
            rdi::context::Unmap( cmdq, gpu_buffer );
            Unmap( solver, particle_data );
        }
    }
}

}//
}}//
