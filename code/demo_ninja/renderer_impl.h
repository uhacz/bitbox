#pragma once

#define BX_VK_DEBUG 1

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <vulkan/vulkan.h>
#include "renderer_util.h"
#include <util/type.h>

#include <vector>
#include <system/window.h>

namespace bx
{

//////////////////////////////////////////////////////////////////////////
struct VulkanInstance
{
    VkInstance       _instance = VK_NULL_HANDLE;
    
    std::vector< const char* > _instance_extensions;
    std::vector< const char* > _instance_layers;
#ifdef BX_VK_DEBUG
    VkDebugReportCallbackEXT	        _debug_report = nullptr;
    VkDebugReportCallbackCreateInfoEXT	_debug_callback_create_info = {};

    void _SetupDebug();
    void _InitDebug();
    void _DeinitDebug();
#endif

    void _SetupExtensions();

    void _CreateInstance();
    void _DestroyInstance();
};
extern void vulkanInstanceCreate();
extern void vulkanInstanceDestroy();
extern VkInstance vulkanInstance();

//////////////////////////////////////////////////////////////////////////

struct VulkanDevice;

struct VulkanSwapChain
{
    VkSurfaceKHR             _surface                 = VK_NULL_HANDLE;
    VkSurfaceFormatKHR       _surface_format          = {};
    VkSurfaceCapabilitiesKHR _surface_capabilities    = {};

    VkSwapchainKHR           _swapchain               = VK_NULL_HANDLE;
    u32                      _image_count   = 2;
    u32                      _width = 512;
    u32                      _height = 512;

    std::vector< VkImage>    _images;
    std::vector< VkImageView>_image_views;
    
    void _InitSurface( bxWindow* window, VulkanDevice* vkdev );
    void _DeinitSurface( VkInstance vkInstance );

    void _InitSwapChain( VulkanDevice* vkdev );
    void _DeinitSwapChain( VulkanDevice* vkdev );

    void _InitSwapChainImages( VulkanDevice* vkdev, VkCommandBuffer setupCmdBuffer );
    void _DeinitSwapChainImages( VulkanDevice* vkdev );

    VkResult acquireNextImage( uint32_t *currentBuffer, VkDevice device, VkSemaphore presentCompleteSemaphore );
    VkResult queuePresent( VkQueue queue, uint32_t currentBuffer, VkSemaphore waitSemaphore );
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct VulkanDevice
{
    VkDevice         _device   = VK_NULL_HANDLE;
    VkPhysicalDevice _gpu      = VK_NULL_HANDLE;
    u32              _graphics_family_index = 0;
    
    VkPhysicalDeviceMemoryProperties _gpu_memory_properties = {};
    VkFormat                         _depth_format = VK_FORMAT_UNDEFINED;

    std::vector< const char* > _device_extensions;

    void _SetupExtensions();

    void _CreateDevice();
    void _DestroyDevice();

    void _EnumerateLayers();
    bool _MemoryTypeIndexGet( uint32_t typeBits, VkFlags properties, u32* typeIndex );

    // propertyFlag is combination of VkMemoryPropertyFlagBits
    VkDeviceMemory memoryAllocate( const VkMemoryRequirements& requirments, VkFlags propertyFlag );
};

//////////////////////////////////////////////////////////////////////////
struct VulkanSampleContext
{
    VulkanSwapChain _swap_chain = {};
    VkQueue _queue = VK_NULL_HANDLE;

    VkCommandPool _command_pool = VK_NULL_HANDLE;
    
    // Command buffer for submitting a post present image barrier
    VkCommandBuffer _post_present_cmd_buffer = VK_NULL_HANDLE;
    // Command buffer for submitting a pre present image barrier
    VkCommandBuffer _pre_present_cmd_buffer = VK_NULL_HANDLE;

    VkSemaphore _render_complete_semaphore = VK_NULL_HANDLE;
    VkSemaphore _present_complete_semaphore = VK_NULL_HANDLE;

    VkRenderPass _render_pass = VK_NULL_HANDLE;

    // List of available frame buffers (same as number of swap chain images)
    std::vector<VkFramebuffer> _framebuffers;
    u32 _current_framebuffer = 0;

    // Descriptor set pool
    //VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
    
    // List of shader modules created (stored for cleanup)
    std::vector<VkShaderModule> _shader_modules;
    
    // Pipeline cache object
    VkPipelineCache _pipeline_cache;
    
    struct
    {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
    } _depth_stencil;

    void initialize( VulkanDevice* vkdev, bxWindow* window );
    void deinitialize( VulkanDevice* vkdev );

    void submitCommandBuffers( VulkanDevice* vkdev, VkCommandBuffer* cmdBuffers, u32 cmdBuffersCount );
    
    void _CreateCommandBuffers( VulkanDevice* vkdev );
    void _DestroyCommandBuffers( VulkanDevice* vkdev );
    
    void _CreateSemaphores( VulkanDevice* vkdev );
    void _DestroySemaphores( VulkanDevice* vkdev );

    void _CreateDepthStencil( VulkanDevice* vkdev, VkCommandBuffer setupCmdBuffer );
    void _DestroyDepthStencil( VulkanDevice* vkdev );

    void _CreateRenderPass( VulkanDevice* vkdev );
    void _DestroyRenderPass( VulkanDevice* vkdev );

    void _CreateFramebuffer( VulkanDevice* vkdev );
    void _DestroyFramebuffer( VulkanDevice* vkdev );

    void _CreatePipelineCache( VulkanDevice* vkdev );
    void _DestroyPipelineCache( VulkanDevice* vkdev );
};

//////////////////////////////////////////////////////////////////////////
struct VulkanSampleTriangle
{
    struct StagingBuffer 
    {
        VkDeviceMemory memory = nullptr;
        VkBuffer buffer = nullptr;
    };

    VkBuffer _vertex_buffer_pos = VK_NULL_HANDLE;
    VkBuffer _vertex_buffer_col = VK_NULL_HANDLE;
    VkBuffer _index_buffer = VK_NULL_HANDLE;
    VkBuffer _uniform_buffer = VK_NULL_HANDLE;

    VkDeviceMemory _vertex_and_index_memory = VK_NULL_HANDLE;
    VkDeviceMemory _uniform_memory = VK_NULL_HANDLE;

    VkPipelineVertexInputStateCreateInfo _vertex_input_info = {};
    VkVertexInputBindingDescription _vertex_bind_desc[2];
    VkVertexInputAttributeDescription _vertex_attrib_desc[2];

    VkPipeline _solid_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout _pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSet _descriptor_set = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descriptor_set_layout = VK_NULL_HANDLE;

    // Command buffers used for rendering
    std::vector< VkCommandBuffer > _draw_cmd_buffer;

    void initialize( VulkanDevice* vkdev, VulkanSampleContext* ctx );
    void deinitialize( VulkanDevice* vkdev, VulkanSampleContext* ctx );

    void _CreateCommandBuffers( VulkanDevice* vkdev, VulkanSampleContext* ctx );
    void _DestroyCommandBuffers( VulkanDevice* vkdev, VulkanSampleContext* ctx );

    void _CreateBuffers( VulkanDevice* vkdev, VulkanSampleContext* ctx );
    void _DestroyBuffers( VulkanDevice* vkdev, VulkanSampleContext* ctx );

    void _CreatePipeline( VulkanDevice* vkdev, VulkanSampleContext* ctx );
    void _DestroyPipeline( VulkanDevice* vkdev, VulkanSampleContext* ctx );

    void _CreatePipelineLayout( VulkanDevice* vkdev, VulkanSampleContext* ctx );
    void _DestroyPipelineLayout( VulkanDevice* vkdev, VulkanSampleContext* ctx );
};
//////////////////////////////////////////////////////////////////////////

}///

