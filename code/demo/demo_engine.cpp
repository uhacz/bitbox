#include "demo_engine.h"
#include <gfx/gfx_camera.h>
#include <gfx/gfx_debug_draw.h>


void bxDemoEngine_startup( bxDemoEngine* dengine )
{
    bxWindow* win = bxWindow_get();
    dengine->_resourceManager = bxResourceManager::startup( "d:/dev/code/bitBox/assets/" );
    //_resourceManager = bxResourceManager::startup( "d:/tmp/bitBox/assets/" );
    bxGdi::backendStartup( &dengine->_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

    dengine->_gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
    dengine->_gdiContext->_Startup( dengine->_gdiDevice );

    bxGfxGUI::_Startup( dengine->_gdiDevice, dengine->_resourceManager, win );

    dengine->_gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
    dengine->_gfxContext->_Startup( dengine->_gdiDevice, dengine->_resourceManager );

    bxGfxDebugDraw::_Startup( dengine->_gdiDevice, dengine->_resourceManager );

    dengine->_gfxLights = BX_NEW( bxDefaultAllocator(), bxGfxLights );
    dengine->_gfxLights->_Startup( dengine->_gdiDevice, bxDemoEngine::MAX_LIGHTS, bxDemoEngine::TILE_SIZE, dengine->_gfxContext->framebufferWidth(), dengine->_gfxContext->framebufferHeight() );

    dengine->_gfxMaterials = BX_NEW( bxDefaultAllocator(), bxGfxMaterialManager );
    dengine->_gfxMaterials->_Startup( dengine->_gdiDevice, dengine->_resourceManager );

    dengine->_gfxPostprocess = BX_NEW1( bxGfxPostprocess );
    dengine->_gfxPostprocess->_Startup( dengine->_gdiDevice, dengine->_resourceManager, dengine->_gfxContext->framebufferWidth(), dengine->_gfxContext->framebufferHeight() );

    

}
void bxDemoEngine_shutdown( bxDemoEngine* dengine )
{
    dengine->_gfxPostprocess->_Shutdown( dengine->_gdiDevice, dengine->_resourceManager );
    BX_DELETE01( dengine->_gfxPostprocess );

    dengine->_gfxMaterials->_Shutdown( dengine->_gdiDevice, dengine->_resourceManager );
    BX_DELETE0( bxDefaultAllocator(), dengine->_gfxMaterials );

    dengine->_gfxLights->_Shutdown( dengine->_gdiDevice );
    BX_DELETE0( bxDefaultAllocator(), dengine->_gfxLights );

    bxGfxDebugDraw::_Shutdown( dengine->_gdiDevice, dengine->_resourceManager );

    dengine->_gfxContext->shutdown( dengine->_gdiDevice, dengine->_resourceManager );
    BX_DELETE0( bxDefaultAllocator(), dengine->_gfxContext );

    bxGfxGUI::shutdown( dengine->_gdiDevice, dengine->_resourceManager, bxWindow_get() );

    dengine->_gdiContext->_Shutdown();
    BX_DELETE0( bxDefaultAllocator(), dengine->_gdiContext );

    bxGdi::backendShutdown( &dengine->_gdiDevice );
    bxResourceManager::shutdown( &dengine->_resourceManager );
}

void bxDemoEngine_frameDraw( bxWindow* win, bxDemoEngine* dengine, bxGfxRenderList* rList, const bxGfxCamera& camera, float deltaTime )
{
    dengine->_gfxLights->cullLights( camera );

    dengine->_gfxContext->frame_begin( dengine->_gdiContext );

    dengine->_gfxContext->frame_zPrepass( dengine->_gdiContext, camera, &rList, 1 );
    {
        bxGdiTexture nrmVSTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_NORMAL_VS );
        bxGdiTexture hwDepthTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
        dengine->_gfxPostprocess->ssao( dengine->_gdiContext, nrmVSTexture, hwDepthTexture );
    }

    dengine->_gfxContext->frame_drawShadows( dengine->_gdiContext, &rList, 1, camera, *dengine->_gfxLights );

    dengine->_gfxContext->bindCamera( dengine->_gdiContext, camera );
    {
        bxGdiTexture outputTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->_gfxPostprocess->sky( dengine->_gdiContext, outputTexture, dengine->_gfxLights->sunLight() );
    }

    dengine->_gfxLights->bind( dengine->_gdiContext );
    dengine->_gfxContext->frame_drawColor( dengine->_gdiContext, camera, &rList, 1, dengine->_gfxPostprocess->ssaoOutput() );

    dengine->_gdiContext->clear();
    dengine->_gfxContext->bindCamera( dengine->_gdiContext, camera );
    {
        bxGdiTexture colorTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        bxGdiTexture outputTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        bxGdiTexture depthTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
        bxGdiTexture shadowTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME );
        dengine->_gfxPostprocess->fog( dengine->_gdiContext, outputTexture, colorTexture, depthTexture, shadowTexture, dengine->_gfxLights->sunLight() );
    }
    {
        bxGdiTexture inputTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        bxGdiTexture outputTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->_gfxPostprocess->toneMapping( dengine->_gdiContext, outputTexture, inputTexture, deltaTime );
    }

    {
        bxGdiTexture inputTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        bxGdiTexture outputTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        dengine->_gfxPostprocess->fxaa( dengine->_gdiContext, outputTexture, inputTexture );
    }

    {
        bxGdiTexture colorTextures[] =
        {
            dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP ),
            dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH ),
            dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_NORMAL_VS ),
            dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS ),
            dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME ),
            dengine->_gfxPostprocess->ssaoOutput(),
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

        bxGdiTexture colorTexture = dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->_gdiContext->changeRenderTargets( &colorTexture, 1, dengine->_gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH ) );
        bxGfxDebugDraw::flush( dengine->_gdiContext, camera.matrix.viewProj );

        colorTexture = colorTextures[current];
        dengine->_gfxContext->frame_rasterizeFramebuffer( dengine->_gdiContext, colorTexture, camera );
    }

    //_gui_lights.show( _gfxLights, pointLights, nPointLights );
    bxGfxGUI::draw( dengine->_gdiContext );
    
    dengine->_gfxContext->frame_end( dengine->_gdiContext );

}