#include "game_gfx.h"
#include <resource_manager\resource_manager.h>
#include <system\window.h>

namespace bx
{
void GameGfxStartUp( GameGfxDeffered* gfx )
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
void GameGfxShutDown( GameGfxDeffered* gfx )
{
    gfx::PostProcessPass::_ShutDown( &gfx->post_pass );
    gfx::LightPass::_ShutDown      ( &gfx->light_pass );
    gfx::SsaoPass::_ShutDown       ( &gfx->ssao_pass );
    gfx::ShadowPass::_ShutDown     ( &gfx->shadow_pass );
    gfx::GeometryPass::_ShutDown   ( &gfx->geometry_pass );
    gfx->renderer.ShutDown( bx::GResourceManager() );
}

void GameGfxDrawScene( rdi::CommandQueue* cmdq, GameGfxDeffered* gfx, gfx::Scene scene, const gfx::Camera& camera )
{
    gfx->geometry_pass.PrepareScene( cmdq, scene, camera );
    gfx->geometry_pass.Flush( cmdq );

    rdi::TextureDepth depthTexture = rdi::GetTextureDepth( gfx->geometry_pass.GBuffer() );
    rdi::TextureRW normalsTexture = rdi::GetTexture( gfx->geometry_pass.GBuffer(), 2 );
    gfx->shadow_pass.PrepareScene( cmdq, scene, camera );
    gfx->shadow_pass.Flush( cmdq, depthTexture, normalsTexture );

    gfx->ssao_pass.PrepareScene( cmdq, camera );
    gfx->ssao_pass.Flush( cmdq, depthTexture, normalsTexture );

    gfx->light_pass.PrepareScene( cmdq, scene, camera );
    gfx->light_pass.Flush( cmdq,
                            gfx->renderer.GetFramebuffer( gfx::EFramebuffer::SWAP ),
                            gfx->geometry_pass.GBuffer(),
                            gfx->shadow_pass.ShadowMap(),
                            gfx->ssao_pass.SsaoTexture() );
}

void GameGfxPostProcess( rdi::CommandQueue* cmdq, GameGfxDeffered* gfx, const gfx::Camera& camera, float deltaTimeSec )
{
    rdi::TextureDepth depthTexture = rdi::GetTextureDepth( gfx->geometry_pass.GBuffer() );

    rdi::TextureRW srcColor = gfx->renderer.GetFramebuffer( gfx::EFramebuffer::SWAP );
    rdi::TextureRW dstColor = gfx->renderer.GetFramebuffer( gfx::EFramebuffer::COLOR );
    gfx->post_pass.DoToneMapping( cmdq, dstColor, srcColor, deltaTimeSec );
    
    gfx::Renderer::DebugDraw( cmdq, dstColor, depthTexture, camera );
}

void GameGfxRasterize( rdi::CommandQueue* cmdq, GameGfxDeffered* gfx, const gfx::Camera& camera )
{
    rdi::ResourceRO* toRasterize[] =
    {
        &gfx->renderer.GetFramebuffer( gfx::EFramebuffer::COLOR ),
        &gfx->ssao_pass.SsaoTexture(),
        &gfx->shadow_pass.ShadowMap(),
        &gfx->shadow_pass.DepthMap(),
    };
    const int toRasterizeN = sizeof( toRasterize ) / sizeof( *toRasterize );
    static int dstColorSelect = 0;

    bxWindow* win = bxWindow_get();
    if( bxInput_isKeyPressedOnce( &win->input.kbd, ' ' ) )
    {
        dstColorSelect = ( dstColorSelect + 1 ) % toRasterizeN;
    }

    rdi::ResourceRO texture = *toRasterize[dstColorSelect];
    gfx->renderer.RasterizeFramebuffer( cmdq, texture, camera, win->width, win->height );
}

}///


