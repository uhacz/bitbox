#include "renderer.h"
#include <util\common.h>
#include <util\buffer_utils.h>
#include <util\poly\poly_shape.h>
#include <resource_manager\resource_manager.h>

#include <rdi/rdi.h>
#include <rdi/rdi_debug_draw.h>

#include "renderer_scene.h"
#include "renderer_scene_actor.h"
#include "renderer_material.h"
#include "renderer_shared_mesh.h"
#include "util\camera.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace bx{ namespace gfx{
void Renderer::StartUp( const RendererDesc& desc, ResourceManager* resourceManager )
{
    TextureManager::_StartUp();
    MaterialManager::_StartUp();
    MeshManager::_StartUp();
    {
        _actor_handle_manager = BX_NEW( bxDefaultAllocator(), ActorHandleManager );
        _actor_handle_manager->StartUp();
    }

    _desc = desc;
    {
        rdi::RenderTargetDesc rt_desc = {};
        rt_desc.Size( desc.framebuffer_width, desc.framebuffer_height );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) ); // COLOR
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) ); // SWAP
        
        
        _render_target = rdi::CreateRenderTarget( rt_desc );
    }
    
    _shf_texutil = rdi::ShaderFileLoad( "shader/bin/texture_utils.shader", resourceManager );
    {
        rdi::PipelineDesc pipeline_desc = {};
        pipeline_desc.Shader( _shf_texutil, "copy_rgba" );
        _pipeline_copy_texture_rgba = rdi::CreatePipeline( pipeline_desc );
    }

    {
        rdi::SamplerDesc samp_desc = {};

        samp_desc.Filter( rdi::ESamplerFilter::NEAREST );
        _samplers.point = rdi::device::CreateSampler( samp_desc );

        samp_desc.Filter( rdi::ESamplerFilter::LINEAR );
        _samplers.linear = rdi::device::CreateSampler( samp_desc );

        samp_desc.Filter( rdi::ESamplerFilter::BILINEAR_ANISO );
        _samplers.bilinear = rdi::device::CreateSampler( samp_desc );

        samp_desc.Filter( rdi::ESamplerFilter::TRILINEAR_ANISO );
        _samplers.trilinear = rdi::device::CreateSampler( samp_desc );

        rdi::ResourceBinding samp_bindings[] =
        {
            rdi::ResourceBinding( "point", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 0 ),
            rdi::ResourceBinding( "linear", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 1 ),
            rdi::ResourceBinding( "bilinear_aniso", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 2 ),
            rdi::ResourceBinding( "trilinear_aniso", rdi::EBindingType::SAMPLER ).StageMask( rdi::EStage::ALL_STAGES_MASK ).Slot( 3 ),
        };
        rdi::ResourceLayout resource_layout = {};
        resource_layout.bindings = samp_bindings;
        resource_layout.num_bindings = sizeof( samp_bindings ) / sizeof( *samp_bindings );
        _samplers.resource_desc = rdi::CreateResourceDescriptor( resource_layout );

        rdi::SetSampler( _samplers.resource_desc, "point", &_samplers.point );
        rdi::SetSampler( _samplers.resource_desc, "linear", &_samplers.linear );
        rdi::SetSampler( _samplers.resource_desc, "bilinear_aniso", &_samplers.bilinear );
        rdi::SetSampler( _samplers.resource_desc, "trilinear_aniso", &_samplers.trilinear );
    }

    {
        
        rdi::RenderSource rsource = rdi::CreateFullscreenQuad();
        MeshHandle hmesh = GMeshManager()->Add( ":fullscreen_quad" );
        GMeshManager()->SetRenderSource( hmesh, rsource );
    }

    {//// poly shapes
        bxPolyShape polyShape;
        bxPolyShape_createBox( &polyShape, 1 );
        rdi::RenderSource rsource_box = rdi::CreateRenderSourceFromPolyShape( polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        bxPolyShape_createShpere( &polyShape, 9 );
        rdi::RenderSource rsource_sphere = rdi::CreateRenderSourceFromPolyShape( polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        MeshHandle hmesh = GMeshManager()->Add( ":sphere" );
        GMeshManager()->SetRenderSource( hmesh, rsource_sphere );
        GMeshManager()->SetLocalAABB( hmesh, bxAABB( Vector3( -0.5f ), Vector3( 0.5f ) ) );

        hmesh = GMeshManager()->Add( ":box" );
        GMeshManager()->SetRenderSource( hmesh, rsource_box );
        GMeshManager()->SetLocalAABB( hmesh, bxAABB( Vector3( -0.5f ), Vector3( 0.5f ) ) );
    }
}

void Renderer::ShutDown( ResourceManager* resourceManager )
{
    {
        rdi::RenderSource rs;
        GMeshManager()->RemoveByName( ":box", &rs );
        rdi::DestroyRenderSource( &rs );

        GMeshManager()->RemoveByName( ":sphere", &rs );
        rdi::DestroyRenderSource( &rs );

        GMeshManager()->RemoveByName( ":fullscreen_quad", &rs );
        rdi::DestroyRenderSource( &rs );
    }

    {
        rdi::DestroyResourceDescriptor( &_samplers.resource_desc );
        rdi::device::DestroySampler( &_samplers.trilinear );
        rdi::device::DestroySampler( &_samplers.bilinear );
        rdi::device::DestroySampler( &_samplers.linear );
        rdi::device::DestroySampler( &_samplers.point );
    }

    rdi::DestroyPipeline( &_pipeline_copy_texture_rgba );
    rdi::DestroyRenderTarget( &_render_target );

    rdi::ShaderFileUnload( &_shf_texutil, resourceManager );

    MeshManager::_ShutDown();
    MaterialManager::_ShutDown();
    TextureManager::_ShutDown();

    {
        _actor_handle_manager->ShutDown();
        BX_DELETE0( bxDefaultAllocator(), _actor_handle_manager );
    }
}

Scene Renderer::CreateScene( const char* name )
{
    SceneImpl* impl = BX_NEW( bxDefaultAllocator(), SceneImpl );
    impl->_handle_manager = _actor_handle_manager;
    impl->Prepare( name, nullptr );

    return impl;
}

void Renderer::DestroyScene( Scene* scene )
{
    SceneImpl* impl = scene[0];
    if( !impl )
        return;

    impl->Unprepare();
    BX_DELETE0( bxDefaultAllocator(), scene[0] );
}

void Renderer::BeginFrame( rdi::CommandQueue* command_queue )
{
    rdi::context::ClearState( command_queue );
    rdi::BindResources( command_queue, _samplers.resource_desc );
}

void Renderer::EndFrame( rdi::CommandQueue* command_queue )
{
    rdi::context::Swap( command_queue );
}

void Renderer::RasterizeFramebuffer( rdi::CommandQueue* cmdq, const rdi::ResourceRO source, const Camera& camera, u32 windowW, u32 windowH )
{
    const rdi::Viewport screen_viewport = gfx::computeViewport( camera, windowW, windowH, _desc.framebuffer_width, _desc.framebuffer_height );
    rdi::context::ChangeToMainFramebuffer( cmdq );
    rdi::context::SetViewport( cmdq, screen_viewport );
    
    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_copy_texture_rgba );
    rdi::SetResourceRO( rdesc, "gtexture", &source );
    //rdi::SetSampler( rdesc, "gsampler", &_samplers.point );
    
    rdi::BindPipeline( cmdq, _pipeline_copy_texture_rgba, true );
    rdi::BindResources( cmdq, rdesc );

    MeshHandle hmesh_fullscreen_quad = GMeshManager()->Find( ":fullscreen_quad" );
    rdi::RenderSource rsource_fullscreen_quad = GMeshManager()->RenderSource( hmesh_fullscreen_quad );
    rdi::BindRenderSource( cmdq, rsource_fullscreen_quad );
    rdi::SubmitRenderSource( cmdq, rsource_fullscreen_quad );

}
void Renderer::DrawFullScreenQuad( rdi::CommandQueue* cmdq )
{
    MeshHandle hmesh_fullscreen_quad = GMeshManager()->Find( ":fullscreen_quad" );
    rdi::RenderSource rsource_fullscreen_quad = GMeshManager()->RenderSource( hmesh_fullscreen_quad );
    rdi::DrawFullscreenQuad( cmdq, rsource_fullscreen_quad );
}

void Renderer::DebugDraw( rdi::CommandQueue* cmdq, rdi::TextureRW targetColor, rdi::TextureDepth targetDepth, const Camera& camera )
{
    rdi::context::ChangeRenderTargets( cmdq, &targetColor, 1, targetDepth );
    rdi::debug_draw::_Flush( cmdq, camera.view, camera.proj );
}

}}///


namespace bx{ namespace gfx{

void GeometryPass::PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera )
{
    {
        FrameData fdata;
        fdata._view = camera.view;
        fdata._view_proj = camera.view_proj;

        rdi::context::UpdateCBuffer( cmdq, _cbuffer_frame_data, &fdata );
        //rdi::BindResources( cmdq, _rdesc_frame_data );
    }

    {
        _vertex_transform_data.Map( cmdq );

        rdi::ClearCommandBuffer( _command_buffer );
        rdi::BeginCommandBuffer( _command_buffer );

        scene->BuildCommandBuffer( _command_buffer, &_vertex_transform_data, camera );

        rdi::EndCommandBuffer( _command_buffer );

        _vertex_transform_data.Unmap( cmdq );
    }
}

void GeometryPass::Flush( rdi::CommandQueue* cmdq )
{
    rdi::BindResources( cmdq, _rdesc_frame_data );
    _vertex_transform_data.Bind( cmdq );
    
    rdi::BindRenderTarget( cmdq, _rtarget_gbuffer );
    //rdi::ClearRenderTarget( cmdq, _rtarget_gbuffer, 5000.f, 6000.f, 8000.f, 1000.f, 1.f );
    rdi::ClearRenderTarget( cmdq, _rtarget_gbuffer, 0.5f, 0.6f, 0.7f, 1.f, 1.f );

    rdi::SubmitCommandBuffer( cmdq, _command_buffer );
}

void GeometryPass::_StartUp( GeometryPass* pass )
{
    {
        rdi::RenderTargetDesc rt_desc = {};
        rt_desc.Size( 1920, 1080 );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        rt_desc.Texture( rdi::Format( rdi::EDataType::FLOAT, 4 ) );
        rt_desc.Depth( rdi::EDataType::DEPTH32F );

        pass->_rtarget_gbuffer = rdi::CreateRenderTarget( rt_desc );
    }

    {
        pass->_cbuffer_frame_data = rdi::device::CreateConstantBuffer( sizeof( FrameData ) );
        rdi::ResourceBinding binding = rdi::ResourceBinding( "FrameData", rdi::EBindingType::UNIFORM ).Slot( SLOT_FRAME_DATA ).StageMask( rdi::EStage::ALL_STAGES_MASK );
        rdi::ResourceLayout layout = {};
        layout.bindings = &binding;
        layout.num_bindings = 1;
        pass->_rdesc_frame_data = rdi::CreateResourceDescriptor( layout );
        rdi::SetConstantBuffer( pass->_rdesc_frame_data, "FrameData", &pass->_cbuffer_frame_data );
    }

    {
        pass->_command_buffer = rdi::CreateCommandBuffer( 1024 );
    }

    VertexTransformData::_Init( &pass->_vertex_transform_data, 1024 );

}

void GeometryPass::_ShutDown( GeometryPass* pass )
{
    VertexTransformData::_Deinit( &pass->_vertex_transform_data );

    rdi::DestroyCommandBuffer( &pass->_command_buffer );
    rdi::DestroyResourceDescriptor( &pass->_rdesc_frame_data );
    rdi::device::DestroyConstantBuffer( &pass->_cbuffer_frame_data );
    rdi::DestroyRenderTarget( &pass->_rtarget_gbuffer );
}
//////////////////////////////////////////////////////////////////////////
namespace
{
    static Matrix3 computeBasis1( const Vector3& dir )
    {
        // Derive two remaining vectors
        Vector3 right, up;
        if( ::abs( dir.getY().getAsFloat() ) > 0.9999f )
        {
            right = Vector3( 1.0f, 0.0f, 0.0f );
        }
        else
        {
            right = normalize( cross( Vector3( 0.0f, 1.0f, 0.0f ), dir ) );
        }

        up = cross( dir, right );
        return Matrix3( right, up, dir );
    }
}
void ShadowPass::_ComputeLightMatrixOrtho( LightMatrices* matrices, const Vector3 wsFrustumCorners[8], const Vector3 wsLightDirection )
{
    Vector3 wsCenter( 0.f );
    for( int i = 0; i < 8; ++i )
    {
        wsCenter += wsFrustumCorners[i];
    }
    wsCenter *= 1.f / 8.f;

    Matrix3 lRot = computeBasis1( -wsLightDirection );
    Matrix4 lWorld( lRot, wsCenter );
    Matrix4 lView = orthoInverse( lWorld );
    rdi::debug_draw::AddAxes( lWorld );

    bxAABB lsAABB = bxAABB::prepare();
    for( int i = 0; i < 8; ++i )
    {
        Vector3 lsCorner = mulAsVec4( lView, wsFrustumCorners[i] );
        lsAABB = bxAABB::extend( lsAABB, lsCorner );
    }

    Vector3 lsAABBsize = bxAABB::size( lsAABB ) * halfVec;
    float3_t lsMin, lsMax, lsExt;
    m128_to_xyz( lsMin.xyz, lsAABB.min.get128() );
    m128_to_xyz( lsMax.xyz, lsAABB.max.get128() );
    m128_to_xyz( lsExt.xyz, lsAABBsize.get128() );

    const Matrix4 lProj = bx::gfx::cameraMatrixOrtho( lsMin.x, lsMax.x, lsMin.y, lsMax.y, lsMin.z, lsMax.z );
    //const Matrix4 lProj = bx::gfx::cameraMatrixOrtho( -lsExt.x, lsExt.x, -lsExt.y, lsExt.y, -lsExt.z, lsExt.z );
    //const Matrix4 lProj = cameraMatrixProjection( 1.f, 0.857652068f, 0.1, lsMax.z );

    //float l = lsMin.x;
    //float r = lsMax.x;
    //float b = lsMin.y;
    //float t = lsMax.y;
    //float n = -lsExt.z;
    //float f = lsExt.z;

    //const Matrix4 lProj1 = Matrix4(
    //    Vector4( 2.f / (r - l), 0.f, 0.f, 0.f ),
    //    Vector4( 0.f, 2.f / (t - b), 0.f, 0.f ),
    //    Vector4( 0.f, 0.f,-2.f / (f - n), 0.f ),
    //    Vector4( -((r+l) / (r-l)), -((t+b)/(t-b)), -((f+n)/(f-n)), 1.f )
    //    );


    //Matrix4 vp = cameraMatrixProjectionDx11( lProj ) * lView;
    //Vector4 a = vp * Point3( 0.f );

    rdi::debug_draw::AddFrustum( lProj * lView, 0xFF00FF00, true );

    matrices->world = lWorld;
    matrices->view = lView;
    matrices->proj = lProj;
    
}

bool ShadowPass::PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera )
{
    SunSkyLight* sunSky = scene->GetSunSkyLight();
    if( !sunSky )
        return false;
    
    Vector3 cameraFrustumCorners[8];
    viewFrustumExtractCorners( cameraFrustumCorners, camera.proj * camera.view );
        
    LightMatrices lightMatrices;
    _ComputeLightMatrixOrtho( &lightMatrices, cameraFrustumCorners, sunSky->sun_direction );
    
    {
        const ViewFrustum lightFrustum = viewFrustumExtract( lightMatrices.proj * lightMatrices.view );
        _vertex_transform_data.Map( cmdq );

        rdi::ClearCommandBuffer( _cmd_buffer );
        rdi::BeginCommandBuffer( _cmd_buffer );

        scene->BuildCommandBufferShadow( _cmd_buffer, &_vertex_transform_data, lightMatrices.world, lightFrustum );

        rdi::EndCommandBuffer( _cmd_buffer );

        _vertex_transform_data.Unmap( cmdq );
    }

    {
        const Matrix4 sc = Matrix4::scale( Vector3( 0.5f, 0.5f, 0.5f ) );
        const Matrix4 tr = Matrix4::translation( Vector3( 1.f, 1.f, 1.f ) );
        const Matrix4 proj = sc * tr * lightMatrices.proj;

        ShadowPass::MaterialData mdata;
        memset( &mdata, 0x00, sizeof( mdata ) );

        mdata.cameraViewProjInv = inverse( camera.view_proj );
        mdata.lightViewProj = cameraMatrixProjectionDx11( lightMatrices.proj ) * lightMatrices.view;
        mdata.lightViewProj_01 = proj * lightMatrices.view;
        m128_to_xyz( mdata.lightDirectionWS.xyzw, sunSky->sun_direction.get128() );

        mdata.depthMapSize = float2_t( _depth_map.info.width, _depth_map.info.height );
        mdata.depthMapSizeRcp = float2_t( 1.f / _depth_map.info.width, 1.f / _depth_map.info.height );

        mdata.shadowMapSize = float2_t( _shadow_map.info.width, _shadow_map.info.height );
        mdata.shadowMapSizeRcp = float2_t( 1.f / _shadow_map.info.width, 1.f / _shadow_map.info.height );

        rdi::context::UpdateCBuffer( cmdq, _cbuffer, &mdata );
    }

    return true;
}

void ShadowPass::Flush( rdi::CommandQueue* cmdq, rdi::TextureDepth sceneDepthTex, rdi::ResourceRO sceneNormalsTex )
{
    {
        rdi::context::ChangeRenderTargets( cmdq, nullptr, 0, _depth_map );
        rdi::context::ClearDepthBuffer( cmdq, _depth_map, 1.f );

        _vertex_transform_data.Bind( cmdq );
        rdi::BindPipeline( cmdq, _pipeline_depth, true );
        //rdi::context::SetShader( cmdq, rdi::Shader(), rdi::EStage::PIXEL );
        rdi::SubmitCommandBuffer( cmdq, _cmd_buffer );
    }

    {
        rdi::context::ChangeRenderTargets( cmdq, &_shadow_map, 1, rdi::TextureDepth() );
        rdi::context::ClearColorBuffer( cmdq, _shadow_map, 1.f, 1.f, 1.f, 1.f );

        rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_resolve );
        rdi::SetResourceRO( rdesc, "sceneDepthTex", &sceneDepthTex );
        rdi::SetResourceRO( rdesc, "sceneNormalsTex", &sceneNormalsTex );

        rdi::BindPipeline( cmdq, _pipeline_resolve, true );
        Renderer::DrawFullScreenQuad( cmdq );
    }
}

void ShadowPass::_StartUp( ShadowPass* pass, const RendererDesc& rndDesc, u32 shadowMapSize )
{
    pass->_depth_map = rdi::device::CreateTexture2Ddepth( shadowMapSize, shadowMapSize, 1, rdi::EDataType::DEPTH32F );

    const u32 bindFlags = rdi::EBindMask::SHADER_RESOURCE | rdi::EBindMask::RENDER_TARGET;
    const u32 cpuAccessMask = 0;
    pass->_shadow_map = rdi::device::CreateTexture2D( rndDesc.framebuffer_width, rndDesc.framebuffer_height, 1, rdi::Format( rdi::EDataType::FLOAT, 1 ), bindFlags, cpuAccessMask, nullptr );

    pass->_cbuffer = rdi::device::CreateConstantBuffer( sizeof( ShadowPass::MaterialData ) );

    rdi::ShaderFile* shf = rdi::ShaderFileLoad( "shader/bin/shadow.shader", GResourceManager() );
    rdi::PipelineDesc pipeline_desc = {};
    rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;

    pipeline_desc.Shader( shf, "depth" );
    pass->_pipeline_depth = rdi::CreatePipeline( pipeline_desc );
    rdesc = rdi::GetResourceDescriptor( pass->_pipeline_depth );
    rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer );

    pipeline_desc.Shader( shf, "resolve" );
    pass->_pipeline_resolve = rdi::CreatePipeline( pipeline_desc );
    rdesc = rdi::GetResourceDescriptor( pass->_pipeline_resolve );
    rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer );
    rdi::SetResourceRO( rdesc, "lightDepthTex", &pass->_depth_map );
    
    rdi::SamplerDesc sampler_desc;
    sampler_desc.Filter( rdi::ESamplerFilter::NEAREST ).Address( rdi::EAddressMode::CLAMP ).DepthCmp( rdi::ESamplerDepthCmp::LEQUAL );
    pass->_sampler_shadow = rdi::device::CreateSampler( sampler_desc );
    rdi::SetSampler( rdesc, "samplShadowMap", &pass->_sampler_shadow );
    
    rdi::ShaderFileUnload( &shf, GResourceManager() );

    pass->_cmd_buffer = rdi::CreateCommandBuffer( 1024 );
    VertexTransformData::_Init( &pass->_vertex_transform_data, 1024 );
}

void ShadowPass::_ShutDown( ShadowPass* pass )
{
    VertexTransformData::_Deinit( &pass->_vertex_transform_data );
    rdi::DestroyCommandBuffer( &pass->_cmd_buffer );
    
    rdi::device::DestroySampler( &pass->_sampler_shadow );
    rdi::device::DestroyConstantBuffer( &pass->_cbuffer );

    rdi::DestroyPipeline( &pass->_pipeline_resolve );
    rdi::DestroyPipeline( &pass->_pipeline_depth );

    rdi::device::DestroyTexture( &pass->_shadow_map );
    rdi::device::DestroyTexture( &pass->_depth_map );
}
//////////////////////////////////////////////////////////////////////////
void SsaoPass::PrepareScene( rdi::CommandQueue* cmdq, const Camera& camera, unsigned fbWidth, unsigned fbHeight )
{
    SsaoPass::MaterialData mdata;
    memset( &mdata, 0x00, sizeof( mdata ) );

    mdata.g_ViewMatrix = camera.view;
    mdata.g_renderTargetSize = float2_t( fbWidth, fbHeight );
    mdata.g_SSAOPhase = 0.f;

    {
        const float fbWidthRcp = 1.f / fbWidth;
        const float fbHeightRcp = 1.f / fbHeight;

        const Matrix4& proj = camera.proj_api;
        const float m11 = proj.getElem( 0, 0 ).getAsFloat();
        const float m22 = proj.getElem( 1, 1 ).getAsFloat();
        const float m33 = proj.getElem( 2, 2 ).getAsFloat();
        const float m44 = proj.getElem( 3, 2 ).getAsFloat();

        const float m13 = proj.getElem( 0, 2 ).getAsFloat();
        const float m23 = proj.getElem( 1, 2 ).getAsFloat();

        const float4_t reprojectInfo = float4_t( 1.f / m11, 1.f / m22, m33, -m44 );
        const float reprojectScale = ( _halfRes ) ? 2.f : 1.f;
        
        mdata.g_ReprojectInfoHalfResFromInt = float4_t(
            ( -reprojectInfo.x * reprojectScale ) * fbWidthRcp,
            ( -reprojectInfo.y * reprojectScale ) * fbHeightRcp,
            reprojectInfo.x,
            reprojectInfo.y
        );
    }

    {
        const float zNear = camera.params.zNear;
        const float zFar = camera.params.zFar;
        mdata.g_reprojectionDepth = float2_t( ( zFar - zNear ) / ( -zFar * zNear ), zFar / ( zFar * zNear ) );
    }

    rdi::context::UpdateCBuffer( cmdq, _cbuffer_mdata, &mdata );
}

void SsaoPass::Flush( rdi::CommandQueue* cmdq, rdi::ResourceRW normalsTexture, rdi::TextureDepth depthTexture )
{

}

void SsaoPass::_StartUp( SsaoPass* pass, const RendererDesc& rndDesc, bool halfRes /*= true */ )
{
    pass->_halfRes = halfRes;
    const u32 width = (halfRes ) ? rndDesc.framebuffer_width / 2 : rndDesc.framebuffer_width;
    const u32 height = ( halfRes ) ? rndDesc.framebuffer_height / 2 : rndDesc.framebuffer_height;

    pass->_ssao_texture = rdi::device::CreateTexture2D( width, height, 1, rdi::Format( rdi::EDataType::FLOAT, 2 ), rdi::EBindMask::SHADER_RESOURCE | rdi::EBindMask::RENDER_TARGET, 0, nullptr );
    pass->_temp_texture = rdi::device::CreateTexture2D( width, height, 1, rdi::Format( rdi::EDataType::FLOAT, 2 ), rdi::EBindMask::SHADER_RESOURCE | rdi::EBindMask::RENDER_TARGET, 0, nullptr );

    pass->_cbuffer_mdata = rdi::device::CreateConstantBuffer( sizeof( SsaoPass::MaterialData ) );

    rdi::ShaderFile* sf = rdi::ShaderFileLoad( "shader/bin/ssao.shader", GResourceManager() );

    rdi::PipelineDesc pipeline_desc = {};
    rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;
    
    pipeline_desc.Shader( sf, "ssao" );
    pass->_pipeline_ssao = rdi::CreatePipeline( pipeline_desc );
    rdesc = rdi::GetResourceDescriptor( pass->_pipeline_ssao );
    rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer_mdata );

    pipeline_desc.Shader( sf, "blurX" );
    pass->_pipeline_blurx = rdi::CreatePipeline( pipeline_desc );
    rdesc = rdi::GetResourceDescriptor( pass->_pipeline_blurx );
    rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer_mdata );
    rdi::SetResourceRO( rdesc, "in_textureSSAO", &pass->_ssao_texture );

    pipeline_desc.Shader( sf, "blurY" );
    pass->_pipeline_blury = rdi::CreatePipeline( pipeline_desc );
    rdesc = rdi::GetResourceDescriptor( pass->_pipeline_blury );
    rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer_mdata );
    rdi::SetResourceRO( rdesc, "in_textureSSAO", &pass->_temp_texture );

    rdi::ShaderFileUnload( &sf, GResourceManager() );
}

void SsaoPass::_ShutDown( SsaoPass* pass )
{
    rdi::DestroyPipeline( &pass->_pipeline_blury );
    rdi::DestroyPipeline( &pass->_pipeline_blurx );
    rdi::DestroyPipeline( &pass->_pipeline_ssao );
    rdi::device::DestroyConstantBuffer( &pass->_cbuffer_mdata );
    rdi::device::DestroyTexture( &pass->_temp_texture );
    rdi::device::DestroyTexture( &pass->_ssao_texture );
}

//////////////////////////////////////////////////////////////////////////
void LightPass::PrepareScene( rdi::CommandQueue* cmdq, Scene scene, const Camera& camera )
{
    {
        //SYS_STATIC_ASSERT( sizeof( LightPass::MaterialData ) == 152 );
        LightPass::MaterialData mdata = {};
        memset( &mdata, 0x00, sizeof( mdata ) );

        storeXYZ( camera.worldEye(), mdata.camera_eye.xyzw );
        storeXYZ( camera.worldDir(), mdata.camera_dir.xyzw );

        SunSkyLight* sunSky = scene->GetSunSkyLight();
        if( sunSky )
        {
            mdata.sun_color = float4_t( sunSky->sun_color, 1.0 );
            mdata.sun_intensity = sunSky->sun_intensity;
            mdata.sky_intensity = sunSky->sky_intensity;

            // L means dir TO light
            const Vector3 L = -normalize( sunSky->sun_direction );
            storeXYZ( L, mdata.sun_L.xyzw );
            mdata.sun_L.w = 0.f;

            if( GTextureManager()->Alive( sunSky->sky_cubemap ) )
            {
                rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _pipeline_skybox );
                rdi::TextureRO* sky_cubemap = GTextureManager()->Texture( sunSky->sky_cubemap );
                rdi::SetResourceRO( rdesc, "skybox", sky_cubemap );
                mdata.environment_map_width = sky_cubemap->info.width;
                mdata.environment_map_max_mip = sky_cubemap->info.mips - 1;
                _has_skybox = 1;
            }
            else
            {
                _has_skybox = 0;
            }
        }


        mdata.view_proj_inv = inverse( camera.view_proj );
        mdata.render_target_size = float2_t( 1920.f, 1080.f );
        mdata.render_target_size_rcp = float2_t( 1.f / mdata.render_target_size.x, 1.f / mdata.render_target_size.y );

        rdi::context::UpdateCBuffer( cmdq, _cbuffer_fdata, &mdata );
    }
}

void LightPass::Flush( rdi::CommandQueue* cmdq, rdi::TextureRW outputTexture, rdi::RenderTarget gbuffer, rdi::ResourceRO shadowMap )
{
    float rgbad[] = { 0.0f, 0.0f, 0.0f, 1.f, 1.f };
    rdi::context::ChangeRenderTargets( cmdq, &outputTexture, 1, rdi::TextureDepth() );
    rdi::context::ClearBuffers( cmdq, &outputTexture, 1, rdi::TextureDepth(), rgbad, 1, 0 );

    rdi::Pipeline pipeline = ( _has_skybox ) ? _pipeline_skybox : _pipeline;
    rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( pipeline );
    rdi::SetResourceRO( rdesc, "gbuffer_albedo_spec", &rdi::GetTexture( gbuffer, 0 ) );
    rdi::SetResourceRO( rdesc, "gbuffer_wpos_rough", &rdi::GetTexture( gbuffer, 1 ) );
    rdi::SetResourceRO( rdesc, "gbuffer_wnrm_metal", &rdi::GetTexture( gbuffer, 2 ) );
    rdi::SetResourceRO( rdesc, "depthTexture", &rdi::GetTextureDepth( gbuffer ) );
    rdi::SetResourceRO( rdesc, "shadowMap", &shadowMap );
    rdi::BindPipeline( cmdq, pipeline, true );

    Renderer::DrawFullScreenQuad( cmdq );
}

void LightPass::_StartUp( LightPass* pass )
{
    rdi::ShaderFile* shf = rdi::ShaderFileLoad( "shader/bin/deffered_lighting.shader", GResourceManager() );

    rdi::PipelineDesc pipeline_desc;
    
    pipeline_desc.Shader( shf, "lighting" );
    pass->_pipeline = rdi::CreatePipeline( pipeline_desc );
    
    pipeline_desc.Shader( shf, "lighting_skybox" );
    pass->_pipeline_skybox = rdi::CreatePipeline( pipeline_desc );


    pass->_cbuffer_fdata = rdi::device::CreateConstantBuffer( sizeof( LightPass::MaterialData ), nullptr );

    {
        rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( pass->_pipeline );
        rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer_fdata );
    }
    {
        rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( pass->_pipeline_skybox );
        rdi::SetConstantBuffer( rdesc, "MaterialData", &pass->_cbuffer_fdata );
    }

    rdi::ShaderFileUnload( &shf, GResourceManager() );

    //pass->_sky_cubemap = GTextureManager()->CreateFromFile( "texture/sky1_cubemap.DDS" );
    //rdi::SetResourceRO( rdesc, "skybox", GTextureManager()->Texture( pass->_sky_cubemap ) );
}

void LightPass::_ShutDown( LightPass* pass )
{
    //GTextureManager()->Release( pass->_sky_cubemap );
    rdi::device::DestroyConstantBuffer( &pass->_cbuffer_fdata );
    rdi::DestroyPipeline( &pass->_pipeline );
    rdi::DestroyPipeline( &pass->_pipeline_skybox );
}

//////////////////////////////////////////////////////////////////////////
static void ChangeTargetAndClear( rdi::CommandQueue* cmdq, rdi::TextureRW texture, const float clearColor[5] )
{
    rdi::context::ChangeRenderTargets( cmdq, &texture, 1, rdi::TextureDepth() );
    rdi::context::ClearBuffers( cmdq, &texture, 1, rdi::TextureDepth(), clearColor, 1, 0 );
}
void PostProcessPass::_StartUp( PostProcessPass* pass )
{
    // --- ToneMapping
    {
        PostProcessPass::ToneMapping& tm = pass->_tm;
        PostProcessPass::ToneMapping::MaterialData& data = tm.data;
        data.input_size0 = float2_t( 0.f, 0.f );
        data.delta_time = 1.f / 60.f;
        
        data.bloom_thresh = 0.f;
        data.bloom_blur_sigma = 0.f;
        data.bloom_magnitude = 0.f;
        
        data.lum_tau = 5.f;
        //data.adaptation_rate = 0.5f;
        data.exposure_key_value = 0.30f;
        data.use_auto_exposure = 1;
        data.camera_aperture = 1.f / 16.f;
        data.camera_shutterSpeed = 0.005f;
        data.camera_iso = 200.f;

        tm.cbuffer_data = rdi::device::CreateConstantBuffer( sizeof( PostProcessPass::ToneMapping::MaterialData ), &data );
        
        const int lumiTexSize = 1024;
        tm.adapted_luminance[0] = rdi::device::CreateTexture2D( lumiTexSize, lumiTexSize, 11, rdi::Format( rdi::EDataType::FLOAT, 1 ), rdi::EBindMask::RENDER_TARGET | rdi::EBindMask::SHADER_RESOURCE, 0, 0 );
        tm.adapted_luminance[1] = rdi::device::CreateTexture2D( lumiTexSize, lumiTexSize, 11, rdi::Format( rdi::EDataType::FLOAT, 1 ), rdi::EBindMask::RENDER_TARGET | rdi::EBindMask::SHADER_RESOURCE, 0, 0 );
        tm.initial_luminance    = rdi::device::CreateTexture2D( lumiTexSize, lumiTexSize, 1 , rdi::Format( rdi::EDataType::FLOAT, 1 ), rdi::EBindMask::RENDER_TARGET | rdi::EBindMask::SHADER_RESOURCE, 0, 0 );

        rdi::ShaderFile* sf = rdi::ShaderFileLoad( "shader/bin/tone_mapping.shader", GResourceManager() );
        
        rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;
        rdi::PipelineDesc pipeline_desc = {};
        
        {
            pipeline_desc.Shader( sf, "luminance_map" );
            tm.pipeline_luminance_map = rdi::CreatePipeline( pipeline_desc );
        }

        {
            pipeline_desc.Shader( sf, "adapt_luminance" );
            tm.pipeline_adapt_luminance = rdi::CreatePipeline( pipeline_desc );
            rdesc = rdi::GetResourceDescriptor( tm.pipeline_adapt_luminance );
            rdi::SetConstantBuffer( rdesc, "MaterialData", &tm.cbuffer_data );
        }

        {
            pipeline_desc.Shader( sf, "composite" );
            tm.pipeline_composite = rdi::CreatePipeline( pipeline_desc );
            rdesc = rdi::GetResourceDescriptor( tm.pipeline_composite );
            rdi::SetConstantBuffer( rdesc, "MaterialData", &tm.cbuffer_data );
        }
    }
}

void PostProcessPass::_ShutDown( PostProcessPass* pass )
{
    // --- ToneMapping
    {
        PostProcessPass::ToneMapping& tm = pass->_tm;
        rdi::DestroyPipeline( &tm.pipeline_composite );
        rdi::DestroyPipeline( &tm.pipeline_adapt_luminance );
        rdi::DestroyPipeline( &tm.pipeline_luminance_map );
    
        rdi::device::DestroyTexture( &tm.initial_luminance );
        rdi::device::DestroyTexture( &tm.adapted_luminance[1] );
        rdi::device::DestroyTexture( &tm.adapted_luminance[0] );

        rdi::device::DestroyConstantBuffer( &tm.cbuffer_data );
    }
}

void PostProcessPass::DoToneMapping( rdi::CommandQueue* cmdq, rdi::TextureRW outTexture, rdi::TextureRW inTexture, float deltaTime )
{
    // --- ToneMapping
    {
        //SYS_STATIC_ASSERT( sizeof( PostProcessPass::ToneMapping::MaterialData ) == 44 );
        _tm.data.input_size0 = float2_t( (float)outTexture.info.width, (float)outTexture.info.height );
        _tm.data.delta_time = deltaTime;
        //_tm.data.adaptation_rate = adaptationRate;
        rdi::context::UpdateCBuffer( cmdq, _tm.cbuffer_data, &_tm.data );


        const float clear_color[5] = { 0.f, 0.f, 0.f, 0.f, 1.0f };

        { // --- LuminanceMap
            ChangeTargetAndClear( cmdq, _tm.initial_luminance, clear_color );
            
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_luminance_map );
            rdi::SetResourceRO( rdesc, "tex_input0", &inTexture );
            rdi::BindPipeline( cmdq, _tm.pipeline_luminance_map, true );
            Renderer::DrawFullScreenQuad( cmdq );
        }

        { // --- AdaptLuminance
            ChangeTargetAndClear( cmdq, _tm.CurrLuminanceTexture(), clear_color );
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_adapt_luminance );
            rdi::SetResourceRO( rdesc, "tex_input0", &_tm.PrevLuminanceTexture() );
            rdi::SetResourceRO( rdesc, "tex_input1", &_tm.initial_luminance );
            rdi::BindPipeline( cmdq, _tm.pipeline_adapt_luminance, true );
            Renderer::DrawFullScreenQuad( cmdq );

            rdi::context::GenerateMipmaps( cmdq, _tm.CurrLuminanceTexture() );
        }

        { // --- Composite
            ChangeTargetAndClear( cmdq, outTexture, clear_color );
            rdi::ResourceDescriptor rdesc = rdi::GetResourceDescriptor( _tm.pipeline_composite );
            rdi::SetResourceRO( rdesc, "tex_input0", &inTexture );
            rdi::SetResourceRO( rdesc, "tex_input1", &_tm.CurrLuminanceTexture() );
            rdi::BindPipeline( cmdq, _tm.pipeline_composite, true );
            Renderer::DrawFullScreenQuad( cmdq );
        }
    
        _tm.current_luminance_texture = !_tm.current_luminance_texture;
    }
}

}}///
