#include "renderer.h"
#include "renderer_impl.h"

namespace bx
{

static VulkanDevice g_vk = {};
//static VulkanSwapChain g_vkwin = {};
void rendererStartup()
{
    vulkanInstanceCreate();    
    g_vk._CreateDevice();
    //g_renderer._EnumerateLayers();
}

void rendererShutdown()
{
    g_vk._DestroyDevice();
    vulkanInstanceDestroy();
}

static VulkanSampleContext g_sample = {};
static VulkanSampleTriangle g_tri = {};
void sampleStartup( bxWindow* window )
{
    g_sample.initialize( &g_vk, window );
    g_tri.initialize( &g_vk, &g_sample );
}

void sampleShutdown()
{
    g_tri.deinitialize( &g_vk, &g_sample );
    g_sample.deinitialize( &g_vk );
}

}///
