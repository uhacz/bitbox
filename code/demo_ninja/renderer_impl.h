#pragma once

#define BX_VK_DEBUG 1

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <vulkan/vulkan.h>
#include "renderer_util.h"
#include <util/type.h>

#ifdef BX_VK_DEBUG
#include <vector>
#endif

#include <system/window.h>

namespace bx
{
struct VulkanRenderer;

struct VulkanSwapChain
{
    VkSurfaceKHR             _surface                 = VK_NULL_HANDLE;
    VkSurfaceFormatKHR       _surface_format          = {};
    VkSurfaceCapabilitiesKHR _surface_capabilities    = {};

    VkSwapchainKHR           _swapchain               = VK_NULL_HANDLE;
    u32                      _swapchain_image_count   = 2;
    u32                      _surface_size_x = 512;
    u32                      _surface_size_y = 512;

    std::vector< VkImage>    _swapchain_images;
    std::vector< VkImageView>_swapchain_image_views;
    
    void _InitSurface( bxWindow* window, VulkanRenderer* renderer );
    void _DeinitSurface( VkInstance vkInstance );

    void _InitSwapChain( VulkanRenderer* renderer );
    void _DeinitSwapChain( VulkanRenderer* renderer );

    void _InitSwapChainImages( VulkanRenderer* renderer );
    void _DeinitSwapChainImages( VulkanRenderer* renderer );

    VkResult acquireNextImage( uint32_t *currentBuffer, VkDevice device, VkSemaphore presentCompleteSemaphore );
    VkResult queuePresent( VkQueue queue, uint32_t currentBuffer, VkSemaphore waitSemaphore );
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct VulkanRenderer
{
    VkInstance       _instance = VK_NULL_HANDLE;
    VkDevice         _device   = VK_NULL_HANDLE;
    VkPhysicalDevice _gpu      = VK_NULL_HANDLE;
    VkQueue          _queue    = VK_NULL_HANDLE;
    u32              _graphics_family_index = 0;
    
    VkPhysicalDeviceMemoryProperties _gpu_memory_properties = {};
    VkFormat                         _depth_format = VK_FORMAT_UNDEFINED;

    VkCommandPool    _command_pool = VK_NULL_HANDLE;

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
    bool _MemoryTypeIndexGet( uint32_t typeBits, VkFlags properties, u32* typeIndex );

    // propertyFlag is combination of VkMemoryPropertyFlagBits
    VkDeviceMemory deviceMemoryAllocate( const VkMemoryRequirements& requirments, VkFlags propertyFlag );
};

struct VulkanSample
{
    VkCommandPool _command_pool = VK_NULL_HANDLE;
    //VkCommandBuffer _setup_cmd_buffer = VK_NULL_HANDLE;
    // Command buffer for submitting a post present image barrier
    VkCommandBuffer _post_present_cmd_buffer = VK_NULL_HANDLE;
    // Command buffer for submitting a pre present image barrier
    VkCommandBuffer _pre_present_cmd_buffer = VK_NULL_HANDLE;

    VkRenderPass _render_pass = VK_NULL_HANDLE;

    // Command buffers used for rendering
    std::vector<VkCommandBuffer> _draw_cmd_buffers;
    // List of available frame buffers (same as number of swap chain images)
    std::vector<VkFramebuffer> _frame_buffers;
    u32 _current_framebuffer = 0;

    // Descriptor set pool
    VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
    
    // List of shader modules created (stored for cleanup)
    std::vector<VkShaderModule> shader_modules;
    
    // Pipeline cache object
    VkPipelineCache _pipeline_cache;
    
    struct
    {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } depthStencil;

    void initialize( VulkanRenderer* renderer, VulkanSwapChain* window );
    void deinitialize( VulkanRenderer* renderer, VulkanSwapChain* window );
};


}///

