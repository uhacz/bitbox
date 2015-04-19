#include "demo_engine.h"
#include <gfx/gfx_camera.h>
#include <gfx/gfx_debug_draw.h>
#include "config.h"


void bxDemoEngine_startup( bxDemoEngine* dengine )
{
    bxWindow* win = bxWindow_get();
    const char* assetDir = bxConfig::global_string( "assetDir" );
    dengine->resourceManager = bxResourceManager::startup( assetDir );
    bxGdi::backendStartup( &dengine->gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

    dengine->gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
    dengine->gdiContext->_Startup( dengine->gdiDevice );

    bxGfxGUI::_Startup( dengine->gdiDevice, dengine->resourceManager, win );

    dengine->gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
    dengine->gfxContext->_Startup( dengine->gdiDevice, dengine->resourceManager );

    bxGfxDebugDraw::_Startup( dengine->gdiDevice, dengine->resourceManager );

    dengine->gfxLights = BX_NEW( bxDefaultAllocator(), bxGfxLights );
    dengine->gfxLights->_Startup( dengine->gdiDevice, bxDemoEngine::MAX_LIGHTS, bxDemoEngine::TILE_SIZE, dengine->gfxContext->framebufferWidth(), dengine->gfxContext->framebufferHeight() );

    dengine->gfxMaterials = BX_NEW( bxDefaultAllocator(), bxGfxMaterialManager );
    dengine->gfxMaterials->_Startup( dengine->gdiDevice, dengine->resourceManager );

    dengine->gfxPostprocess = BX_NEW1( bxGfxPostprocess );
    dengine->gfxPostprocess->_Startup( dengine->gdiDevice, dengine->resourceManager, dengine->gfxContext->framebufferWidth(), dengine->gfxContext->framebufferHeight() );
}
void bxDemoEngine_shutdown( bxDemoEngine* dengine )
{
    dengine->gfxPostprocess->_Shutdown( dengine->gdiDevice, dengine->resourceManager );
    BX_DELETE01( dengine->gfxPostprocess );

    dengine->gfxMaterials->_Shutdown( dengine->gdiDevice, dengine->resourceManager );
    BX_DELETE0( bxDefaultAllocator(), dengine->gfxMaterials );

    dengine->gfxLights->_Shutdown( dengine->gdiDevice );
    BX_DELETE0( bxDefaultAllocator(), dengine->gfxLights );

    bxGfxDebugDraw::_Shutdown( dengine->gdiDevice, dengine->resourceManager );

    dengine->gfxContext->shutdown( dengine->gdiDevice, dengine->resourceManager );
    BX_DELETE0( bxDefaultAllocator(), dengine->gfxContext );

    bxGfxGUI::shutdown( dengine->gdiDevice, dengine->resourceManager, bxWindow_get() );

    dengine->gdiContext->_Shutdown();
    BX_DELETE0( bxDefaultAllocator(), dengine->gdiContext );

    bxGdi::backendShutdown( &dengine->gdiDevice );
    bxResourceManager::shutdown( &dengine->resourceManager );
}

void bxDemoEngine_frameDraw( bxWindow* win, bxDemoEngine* dengine, bxGfxRenderList* rList, const bxGfxCamera& camera, float deltaTime )
{
    dengine->gfxLights->cullLights( camera );

    dengine->gfxContext->frame_begin( dengine->gdiContext );

    dengine->gfxContext->frame_zPrepass( dengine->gdiContext, camera, &rList, 1 );
    {
        bxGdiTexture nrmVSTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_NORMAL_VS );
        bxGdiTexture hwDepthTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
        dengine->gfxPostprocess->ssao( dengine->gdiContext, nrmVSTexture, hwDepthTexture );
    }

    dengine->gfxContext->frame_drawShadows( dengine->gdiContext, &rList, 1, camera, *dengine->gfxLights );

    dengine->gfxContext->bindCamera( dengine->gdiContext, camera );
    {
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->gfxPostprocess->sky( dengine->gdiContext, outputTexture, dengine->gfxLights->sunLight() );
    }

    dengine->gfxLights->bind( dengine->gdiContext );
    dengine->gfxContext->frame_drawColor( dengine->gdiContext, camera, &rList, 1, dengine->gfxPostprocess->ssaoOutput() );

    dengine->gdiContext->clear();
    dengine->gfxContext->bindCamera( dengine->gdiContext, camera );
    {
        bxGdiTexture colorTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        bxGdiTexture depthTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
        bxGdiTexture shadowTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME );
        dengine->gfxPostprocess->fog( dengine->gdiContext, outputTexture, colorTexture, depthTexture, shadowTexture, dengine->gfxLights->sunLight() );
    }
    {
        bxGdiTexture inputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->gfxPostprocess->toneMapping( dengine->gdiContext, outputTexture, inputTexture, deltaTime );
    }

    {
        bxGdiTexture inputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        dengine->gfxPostprocess->fxaa( dengine->gdiContext, outputTexture, inputTexture );
    }

    {
        bxGdiTexture colorTextures[] =
        {
            dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP ),
            dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH ),
            dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_NORMAL_VS ),
            dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS ),
            dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME ),
            dengine->gfxPostprocess->ssaoOutput(),
            //_gfxShadows->_depthTexture,
        };
        const char* colorNames[] =
        {
            "color", "depth", "normalsVS", "shadows", "shadowsVolume", "ssao",// "cascades",
        };
        const int nTextures = sizeof( colorTextures ) / sizeof( *colorTextures );
        static int current = 0;

        {
            ImGui::Begin();
            ImGui::Combo( "Visible RT", &current, colorNames, nTextures );
            ImGui::End();
        }

        bxGdiTexture colorTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->gdiContext->changeRenderTargets( &colorTexture, 1, dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH ) );
        bxGfxDebugDraw::flush( dengine->gdiContext, camera.matrix.viewProj );

        colorTexture = colorTextures[current];
        dengine->gfxContext->frame_rasterizeFramebuffer( dengine->gdiContext, colorTexture, camera );
    }

    //_gui_lights.show( _gfxLights, pointLights, nPointLights );
    bxGfxGUI::draw( dengine->gdiContext );
    
    dengine->gfxContext->frame_end( dengine->gdiContext );

}