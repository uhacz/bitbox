#pragma once

#define BX_VK_DEBUG 1

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <vulkan/vulkan.h>
#include <util/type.h>

#ifdef BX_VK_DEBUG
#include <vector>
#endif

#include <system/window.h>

namespace bx
{

struct VulkanRenderer;

struct VulkanWindow
{
    VkSurfaceKHR             _surface                 = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR _surface_cap             = {};
    VkSurfaceFormatKHR       _surface_format          = {};
    VkSurfaceCapabilitiesKHR _surface_capabilities    = {};

    VkSwapchainKHR           _swapchain               = VK_NULL_HANDLE;
    u32                      _swapchain_image_count   = 2;
    u32 _surface_size_x = 512;
    u32 _surface_size_y = 512;

    std::vector< VkImage>    _swapchain_images;
    std::vector< VkImageView>_swapchain_image_views;
    
    void _InitSurface( bxWindow* window, VulkanRenderer* renderer );
    void _DeinitSurface( VkInstance vkInstance );

    void _InitSwapChain( VulkanRenderer* renderer );
    void _DeinitSwapChain( VulkanRenderer* renderer );

    void _InitSwapChainImages( VulkanRenderer* renderer );
    void _DeinitSwapChainImages( VulkanRenderer* renderer );
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct VulkanRenderer
{
    VkInstance       _instance = nullptr;
    VkDevice         _device   = nullptr;
    VkPhysicalDevice _gpu      = nullptr;
    VkQueue          _queue    = nullptr;
    u32              _graphics_family_index = 0;

    std::vector< const char* > _instance_extensions;
    std::vector< const char* > _device_extensions;
#ifdef BX_VK_DEBUG
    std::vector< const char* > _instance_layers;

    VkDebugReportCallbackEXT	        _debug_report = nullptr;
    VkDebugReportCallbackCreateInfoEXT	_debug_callback_create_info = {};
    
    void _SetupDebug();
    void _InitDebug();
    void _DeinitDebug();
#endif
    void _SetupExtensions();

    void _CreateInstance();
    void _DestroyInstance();

    void _CreateDevice();
    void _DestroyDevice();

    void _EnumerateLayers();
};

void vulkanCheckError( VkResult res );

}///

