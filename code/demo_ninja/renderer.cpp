#include "renderer.h"
#include "renderer_impl.h"

namespace bx
{

static VulkanRenderer g_vk = {};
//static VulkanSwapChain g_vkwin = {};
void rendererStartup()
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
    //g_renderer._EnumerateLayers();
}

void rendererShutdown()
{
    g_vk._DestroyDevice();
#ifdef BX_VK_DEBUG
    g_vk._DeinitDebug();
#endif
    g_vk._DestroyInstance();
}

static VulkanSample g_sample = {};
void sampleStartup( bxWindow* window )
{
    g_sample.initialize( &g_vk, window );
}

void sampleShutdown()
{
    g_sample.deinitialize( &g_vk );
}

}///
