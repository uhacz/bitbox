#include "demo_engine.h"
#include <gfx/gfx_camera.h>
#include <gfx/gfx_debug_draw.h>
#include <util/config.h>

#include <resource_manager/resource_manager.h>


void bxDemoEngine_startup( bxEngine* e, bxDemoEngine* dengine )
{
    dengine->gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
    dengine->gfxContext->_Startup( e->gdiDevice, e->resourceManager );

    bxGfxDebugDraw::_Startup( e->gdiDevice, e->resourceManager );

    dengine->gfxLights = BX_NEW( bxDefaultAllocator(), bxGfxLights );
    dengine->gfxLights->_Startup( e->gdiDevice, bxDemoEngine::MAX_LIGHTS, bxDemoEngine::TILE_SIZE, dengine->gfxContext->framebufferWidth(), dengine->gfxContext->framebufferHeight() );

    dengine->gfxMaterials = BX_NEW( bxDefaultAllocator(), bxGfxMaterialManager );
    dengine->gfxMaterials->_Startup( e->gdiDevice, e->resourceManager );

    dengine->gfxPostprocess = BX_NEW1( bxGfxPostprocess );
    dengine->gfxPostprocess->_Startup( e->gdiDevice, e->resourceManager, dengine->gfxContext->framebufferWidth(), dengine->gfxContext->framebufferHeight() );
}
void bxDemoEngine_shutdown( bxEngine* e, bxDemoEngine* dengine )
{
    dengine->gfxPostprocess->_Shutdown( e->gdiDevice, e->resourceManager );
    BX_DELETE01( dengine->gfxPostprocess );

    dengine->gfxMaterials->_Shutdown( e->gdiDevice, e->resourceManager );
    BX_DELETE0( bxDefaultAllocator(), dengine->gfxMaterials );

    dengine->gfxLights->_Shutdown( e->gdiDevice );
    BX_DELETE0( bxDefaultAllocator(), dengine->gfxLights );

    bxGfxDebugDraw::_Shutdown( e->gdiDevice, e->resourceManager );

    dengine->gfxContext->shutdown( e->gdiDevice, e->resourceManager );
    BX_DELETE0( bxDefaultAllocator(), dengine->gfxContext );
}

void bxDemoEngine_frameDraw( bxWindow* win, bxEngine* e, bxDemoEngine* dengine, bxGfxRenderList* rList, const bxGfxCamera& camera, float deltaTime )
{
    
    dengine->gfxLights->cullLights( camera );

    dengine->gfxContext->frame_begin( e->gdiContext );

    dengine->gfxContext->frame_zPrepass( e->gdiContext, camera, &rList, 1 );
    {
        bxGdiTexture nrmVSTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_NORMAL_VS );
        bxGdiTexture hwDepthTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
        dengine->gfxPostprocess->ssao( e->gdiContext, nrmVSTexture, hwDepthTexture );
    }

    dengine->gfxContext->frame_drawShadows( e->gdiContext, &rList, 1, camera, *dengine->gfxLights );

    dengine->gfxContext->bindCamera( e->gdiContext, camera );
    {
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->gfxPostprocess->sky( e->gdiContext, outputTexture, dengine->gfxLights->sunLight() );
    }

    dengine->gfxLights->bind( e->gdiContext );
    dengine->gfxContext->frame_drawColor( e->gdiContext, camera, &rList, 1, dengine->gfxPostprocess->ssaoOutput() );

    e->gdiContext->clear();
    dengine->gfxContext->bindCamera( e->gdiContext, camera );
    {
        bxGdiTexture colorTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        bxGdiTexture depthTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
        bxGdiTexture shadowTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME );
        dengine->gfxPostprocess->fog( e->gdiContext, outputTexture, colorTexture, depthTexture, shadowTexture, dengine->gfxLights->sunLight() );
    }
    {
        bxGdiTexture inputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        dengine->gfxPostprocess->toneMapping( e->gdiContext, outputTexture, inputTexture, deltaTime );
    }

    {
        bxGdiTexture inputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
        bxGdiTexture outputTexture = dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
        dengine->gfxPostprocess->fxaa( e->gdiContext, outputTexture, inputTexture );
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
        e->gdiContext->changeRenderTargets( &colorTexture, 1, dengine->gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH ) );
        bxGfxDebugDraw::flush( e->gdiContext, camera.matrix.viewProj );

        colorTexture = colorTextures[current];
        bxGfx::rasterizeFramebuffer( e->gdiContext, colorTexture, camera );
    }

    //_gui_lights.show( _gfxLights, pointLights, nPointLights );
    bxGfxGUI::draw( e->gdiContext );
    
    dengine->gfxContext->frame_end( e->gdiContext );

}