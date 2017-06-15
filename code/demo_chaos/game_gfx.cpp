#include "game_gfx.h"
#include <resource_manager\resource_manager.h>
#include <system\window.h>

namespace bx{ namespace game_gfx{

void StartUp( Deffered* gfx )
{
    ResourceManager* resource_manager = GResourceManager();
    {
        gfx::RendererDesc rdesc = {};
        rdesc.framebuffer_width = 1920;
        rdesc.framebuffer_height = 1080;
        gfx->renderer.StartUp( rdesc, resource_manager );

        gfx::GeometryPass::_StartUp   ( &gfx->geometry_pass, gfx->renderer.GetDesc() );
        gfx::ShadowPass::_StartUp     ( &gfx->shadow_pass  , gfx->renderer.GetDesc(), 1024 * 8 );
        gfx::SsaoPass::_StartUp       ( &gfx->ssao_pass    , gfx->renderer.GetDesc(), false );
        gfx::LightPass::_StartUp      ( &gfx->light_pass );
        gfx::PostProcessPass::_StartUp( &gfx->post_pass );
    }
}
void ShutDown( Deffered* gfx )
{
    gfx::PostProcessPass::_ShutDown( &gfx->post_pass );
    gfx::LightPass::_ShutDown      ( &gfx->light_pass );
    gfx::SsaoPass::_ShutDown       ( &gfx->ssao_pass );
    gfx::ShadowPass::_ShutDown     ( &gfx->shadow_pass );
    gfx::GeometryPass::_ShutDown   ( &gfx->geometry_pass );
    gfx->renderer.ShutDown( bx::GResourceManager() );
}

void Deffered::DrawScene( rdi::CommandQueue* cmdq, gfx::Scene scene, const gfx::Camera& camera )
{
    geometry_pass.PrepareScene( cmdq, scene, camera );
    geometry_pass.Flush( cmdq );

    rdi::TextureDepth depthTexture = rdi::GetTextureDepth( geometry_pass.GBuffer() );
    rdi::TextureRW normalsTexture = rdi::GetTexture( geometry_pass.GBuffer(), gfx::EGBuffer::WNRM_METAL );
    shadow_pass.PrepareScene( cmdq, scene, camera );
    shadow_pass.Flush( cmdq, depthTexture, normalsTexture );

    ssao_pass.PrepareScene( cmdq, camera );
    ssao_pass.Flush( cmdq, depthTexture, normalsTexture );

    light_pass.PrepareScene( cmdq, scene, camera );
    light_pass.Flush( cmdq,
                      renderer.GetFramebuffer( gfx::EFramebuffer::SWAP ),
                      geometry_pass.GBuffer(),
                      shadow_pass.ShadowMap(),
                      ssao_pass.SsaoTexture() );
}

void Deffered::PostProcess( rdi::CommandQueue* cmdq, const gfx::Camera& camera, float deltaTimeSec )
{
    rdi::TextureDepth depthTexture = rdi::GetTextureDepth( geometry_pass.GBuffer() );

    rdi::TextureRW srcColor = renderer.GetFramebuffer( gfx::EFramebuffer::SWAP );
    rdi::TextureRW dstColor = renderer.GetFramebuffer( gfx::EFramebuffer::COLOR );
    post_pass.DoToneMapping( cmdq, dstColor, srcColor, deltaTimeSec );
    
    gfx::Renderer::DebugDraw( cmdq, dstColor, depthTexture, camera );
}

void Deffered::Rasterize( rdi::CommandQueue* cmdq, const gfx::Camera& camera )
{
    rdi::ResourceRO* toRasterize[] =
    {
        &renderer.GetFramebuffer( gfx::EFramebuffer::COLOR ),
        &ssao_pass.SsaoTexture(),
        &shadow_pass.ShadowMap(),
        &shadow_pass.DepthMap(),
    };
    const int toRasterizeN = sizeof( toRasterize ) / sizeof( *toRasterize );
    static int dstColorSelect = 0;

    bxWindow* win = bxWindow_get();
    if( bxInput_isKeyPressedOnce( &win->input.kbd, ' ' ) )
    {
        dstColorSelect = ( dstColorSelect + 1 ) % toRasterizeN;
    }

    rdi::ResourceRO texture = *toRasterize[dstColorSelect];
    renderer.RasterizeFramebuffer( cmdq, texture, camera, win->width, win->height );
}

}}///
