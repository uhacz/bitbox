#include "renderer.h"
#include "renderer_impl.h"

namespace bx
{

static VulkanRenderer g_vk = {};
static VulkanWindow g_vkwin = {};
void rendererStartup( bxWindow* window )
{
#ifdef BX_VK_DEBUG
    g_vk._SetupDebug();
#endif
    
    g_vk._SetupExtensions();
    g_vk._CreateInstance();

#ifdef BX_VK_DEBUG
    g_vk._InitDebug();
#endif
    
    g_vk._CreateDevice();

    g_vkwin._InitSurface( window, &g_vk );
    g_vkwin._InitSwapChain( &g_vk );
    g_vkwin._InitSwapChainImages( &g_vk );
    //g_renderer._EnumerateLayers();
}

void rendererShutdown()
{
    g_vkwin._DeinitSwapChainImages( &g_vk );
    g_vkwin._DeinitSwapChain( &g_vk );
    g_vkwin._DeinitSurface( g_vk._instance );

    g_vk._DestroyDevice();
#ifdef BX_VK_DEBUG
    g_vk._DeinitDebug();
#endif
    g_vk._DestroyInstance();
}

}///
