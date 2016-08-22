#include "renderer_impl.h"
#include <util/debug.h>
#include <vector>

#include <util/common.h>

namespace bx
{


void VulkanSwapChain::_InitSurface( bxWindow* window, VulkanRenderer* renderer )
{
    VkInstance vkInstance = renderer->_instance;
    VkPhysicalDevice gpu = renderer->_gpu;
    
    VkWin32SurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = window->hinstance;
    create_info.hwnd = window->hwnd;
    auto result = vkCreateWin32SurfaceKHR( vkInstance, &create_info, nullptr, &_surface );
    vulkan_util::checkError( result );

    VkBool32 WSI_supported = false;
    result = vkGetPhysicalDeviceSurfaceSupportKHR( gpu, renderer->_graphics_family_index, _surface, &WSI_supported );
    vulkan_util::checkError( result );
    if( !WSI_supported )
    {
        SYS_ASSERT( 0 && "WSI not supported" );
    }

    _width = window->width;
    _height = window->height;

    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR( gpu, _surface, &_surface_capabilities );
    vulkan_util::checkError( result );

    if( _surface_capabilities.currentExtent.width < UINT32_MAX )
    {
        _width = _surface_capabilities.currentExtent.width;
        _height = _surface_capabilities.currentExtent.height;
    }

    {
        uint32_t format_count = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR( gpu, _surface, &format_count, nullptr );
        vulkan_util::checkError( result );

        if( format_count == 0 )
        {
            SYS_ASSERT( 0 && "Surface formats missing." );
        }
        std::vector<VkSurfaceFormatKHR> formats( format_count );
        vkGetPhysicalDeviceSurfaceFormatsKHR( gpu, _surface, &format_count, formats.data() );
        if( formats[0].format == VK_FORMAT_UNDEFINED )
        {
            _surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
            _surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        }
        else
        {
            _surface_format = formats[0];
        }
    }
}

void VulkanSwapChain::_DeinitSurface( VkInstance vkInstance )
{
    vkDestroySurfaceKHR( vkInstance, _surface, nullptr );
    _surface = VK_NULL_HANDLE;
}

void VulkanSwapChain::_InitSwapChain( VulkanRenderer* renderer )
{
    _image_count = clamp( _image_count, _surface_capabilities.minImageCount, _surface_capabilities.maxImageCount );
    _image_count = maxOfPair( 2u, _image_count ); // ensure at least double buffering 

    VkResult result = VK_SUCCESS;

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        u32 present_mode_count = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR( renderer->_gpu, _surface, &present_mode_count, nullptr );
        vulkan_util::checkError( result );

        std::vector<VkPresentModeKHR> present_modes( present_mode_count );
        result = vkGetPhysicalDeviceSurfacePresentModesKHR( renderer->_gpu, _surface, &present_mode_count, present_modes.data() );
        vulkan_util::checkError( result );

        for( auto m : present_modes )
        {
            if( m == VK_PRESENT_MODE_MAILBOX_KHR )
            {
                present_mode = m;
                break;
            }
        }
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = _surface;
    swapchain_create_info.minImageCount = _image_count;
    swapchain_create_info.imageFormat = _surface_format.format;
    swapchain_create_info.imageColorSpace = _surface_format.colorSpace;
    swapchain_create_info.imageExtent.width = _width;
    swapchain_create_info.imageExtent.height = _height;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = nullptr;
    swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR( renderer->_device, &swapchain_create_info, nullptr, &_swapchain );
    vulkan_util::checkError( result );

    result = vkGetSwapchainImagesKHR( renderer->_device, _swapchain, &_image_count, nullptr );
    vulkan_util::checkError( result );
}

void VulkanSwapChain::_DeinitSwapChain( VulkanRenderer* renderer )
{
    vkDestroySwapchainKHR( renderer->_device, _swapchain, nullptr );
    _swapchain = VK_NULL_HANDLE;
}

void VulkanSwapChain::_InitSwapChainImages( VulkanRenderer* renderer, VkCommandBuffer setupCmdBuffer )
{
    //VkCommandBuffer setup_cmd_buffer = vulkan_util::setupCommandBufferCreate( renderer->_device, renderer->_command_pool );
    
    _images.resize( _image_count );
    _image_views.resize( _image_count );

    VkDevice device = renderer->_device;

    auto result = vkGetSwapchainImagesKHR( device, _swapchain, &_image_count, _images.data() );
    vulkan_util::checkError( result );

    for( u32 i = 0; i < _image_count; ++i )
    {
        VkImageViewCreateInfo view_create_info = {};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.image = _images[i];
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create_info.format = _surface_format.format;
        view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create_info.subresourceRange.baseMipLevel = 0;
        view_create_info.subresourceRange.levelCount = 1;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount = 1;

        vulkan_util::setImageLayout( setupCmdBuffer, _images[i], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );

        result = vkCreateImageView( device, &view_create_info, nullptr, &_image_views[i] );
        vulkan_util::checkError( result );
    }

    //vulkan_util::setupCommandBufferFlush( &setup_cmd_buffer, renderer->_device, renderer->_queue, renderer->_command_pool );
}

void VulkanSwapChain::_DeinitSwapChainImages( VulkanRenderer* renderer )
{
    for( u32 i = 0; i < _image_count; ++i )
    {
        vkDestroyImageView( renderer->_device, _image_views[i], nullptr );
        _image_views[i] = VK_NULL_HANDLE;
    }
}

VkResult VulkanSwapChain::acquireNextImage( uint32_t *currentBuffer, VkDevice device, VkSemaphore presentCompleteSemaphore )
{
    return vkAcquireNextImageKHR( device, _swapchain, UINT64_MAX, presentCompleteSemaphore, ( VkFence )nullptr, currentBuffer );
}

VkResult VulkanSwapChain::queuePresent( VkQueue queue, uint32_t currentBuffer, VkSemaphore waitSemaphore )
{
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &_swapchain;
    present_info.pImageIndices = &currentBuffer;
    if( waitSemaphore != VK_NULL_HANDLE )
    {
        present_info.pWaitSemaphores = &waitSemaphore;
        present_info.waitSemaphoreCount = 1;
    }
    return vkQueuePresentKHR( queue, &present_info );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void VulkanRenderer::_SetupExtensions()
{
    _instance_extensions.push_back( VK_KHR_SURFACE_EXTENSION_NAME );
    _instance_extensions.push_back( VK_KHR_WIN32_SURFACE_EXTENSION_NAME );
    _device_extensions.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
}

void VulkanRenderer::_CreateInstance()
{
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = "ninja";
    app_info.applicationVersion = 1;
    app_info.pEngineName = "bx";
    app_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
    app_info.apiVersion = VK_MAKE_VERSION( 1, 0, 3 );

    // initialize the VkInstanceCreateInfo structure
    VkInstanceCreateInfo inst_info = {};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.flags = 0;
    inst_info.pApplicationInfo = &app_info;

    inst_info.enabledExtensionCount = (u32)_instance_extensions.size();
    inst_info.ppEnabledExtensionNames = _instance_extensions.data();

    inst_info.enabledLayerCount = (u32)_instance_layers.size();
    inst_info.ppEnabledLayerNames = _instance_layers.data();

#ifdef BX_VK_DEBUG
    inst_info.pNext = &_debug_callback_create_info;
#endif

    auto res = vkCreateInstance( &inst_info, nullptr, &_instance );
    vulkan_util::checkError( res );
}

void VulkanRenderer::_DestroyInstance()
{
    vkDestroyInstance( _instance, nullptr );
    _instance = nullptr;
}

void VulkanRenderer::_CreateDevice()
{
    u32 gpu_count = 1;
    auto res = vkEnumeratePhysicalDevices( _instance, &gpu_count, nullptr );
    vulkan_util::checkError( res );

    std::vector<VkPhysicalDevice> gpus;
    gpus.resize( gpu_count );
    res = vkEnumeratePhysicalDevices( _instance, &gpu_count, gpus.data() );
    vulkan_util::checkError( res );
    SYS_ASSERT( (gpu_count >= 1) );
    _gpu = gpus[0];

    VkDeviceQueueCreateInfo queue_info = {};
    u32 queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( _gpu, &queue_count, nullptr );
    SYS_ASSERT( queue_count >= 1 );

    std::vector<VkQueueFamilyProperties> queue_props;
    queue_props.resize( queue_count );
    
    vkGetPhysicalDeviceQueueFamilyProperties( _gpu, &queue_count, queue_props.data() );
    SYS_ASSERT( queue_count >= 1 );

    bool found = false;
    for( unsigned int i = 0; i < queue_count; i++ )
    {
        if( queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )
        {
            _graphics_family_index = i;
            queue_info.queueFamilyIndex = i;
            found = true;
            break;
        }
    }
    SYS_ASSERT( found );
    SYS_ASSERT( queue_count >= 1 );

    float queue_priorities[1] = { 1.0f };
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.pNext = nullptr;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = queue_priorities;

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = nullptr;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = (u32)_device_extensions.size();
    device_info.ppEnabledExtensionNames = _device_extensions.data();
    device_info.enabledLayerCount = 0;
    device_info.ppEnabledLayerNames = nullptr;
    device_info.pEnabledFeatures = nullptr;

    
    res = vkCreateDevice( _gpu, &device_info, nullptr, &_device );
    vulkan_util::checkError( res );

    vkGetPhysicalDeviceMemoryProperties( _gpu, &_gpu_memory_properties );
    bool bres = vulkan_util::supportedDepthFormatGet( &_depth_format, _gpu );
    SYS_ASSERT( bres );

    //_command_pool = vulkan_util::commandPoolCreate( _device, _graphics_family_index );
}

void VulkanRenderer::_DestroyDevice()
{
    //vkDestroyCommandPool( _device, _command_pool, nullptr );
    //_command_pool = VK_NULL_HANDLE;

    vkDestroyDevice( _device, nullptr );
    _device = nullptr;
    _gpu = nullptr;
}

void VulkanRenderer::_EnumerateLayers()
{
    u32 instance_layer_count = 0;
    std::vector< VkLayerProperties > instance_layer_props;
    {
        vkEnumerateInstanceLayerProperties( &instance_layer_count, nullptr );
        instance_layer_props.resize( instance_layer_count );
        vkEnumerateInstanceLayerProperties( &instance_layer_count, instance_layer_props.data() );
    }

    u32 device_layer_count = 0;
    std::vector< VkLayerProperties > device_layer_props;
    {
        vkEnumerateDeviceLayerProperties( _gpu, &device_layer_count, nullptr );
        device_layer_props.resize( device_layer_count );
        vkEnumerateDeviceLayerProperties( _gpu, &device_layer_count, device_layer_props.data() );
    }
}

bool VulkanRenderer::_MemoryTypeIndexGet( uint32_t typeBits, VkFlags properties, u32* typeIndex )
{
    for( int i = 0; i < VK_MAX_MEMORY_TYPES; i++ )
    {
        if( ( typeBits & 1 ) == 1 )
        {
            if( ( _gpu_memory_properties.memoryTypes[i].propertyFlags & properties ) == properties )
            {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

VkDeviceMemory VulkanRenderer::deviceMemoryAllocate( const VkMemoryRequirements& requirments, VkFlags propertyFlag )
{
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirments.size;
    bool bresult = _MemoryTypeIndexGet( requirments.memoryTypeBits, propertyFlag, &alloc_info.memoryTypeIndex );
    
    VkDeviceMemory device_memory = VK_NULL_HANDLE;
    VkResult result = vkAllocateMemory( _device, &alloc_info, nullptr, &device_memory );
    vulkan_util::checkError( result );
    return device_memory;
}

#ifdef BX_VK_DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugCallback(
    VkDebugReportFlagsEXT		flags,
    VkDebugReportObjectTypeEXT	obj_type,
    uint64_t					src_obj,
    size_t						location,
    int32_t						msg_code,
    const char *				layer_prefix,
    const char *				msg,
    void *						user_data
    )
{
    printf( "VKDBG: " );
    if( flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT )
    {
        printf( "INFO: " );
    }
    if( flags & VK_DEBUG_REPORT_WARNING_BIT_EXT )
    {
        printf( "WARNING: " );
    }
    if( flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT )
    {
        printf( "PERFORMANCE: " );
    }
    if( flags & VK_DEBUG_REPORT_ERROR_BIT_EXT )
    {
        printf( "ERROR: " );
    }
    if( flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT )
    {
        printf( "DEBUG: " );
    }
    printf( "@[%s]: %s\n", layer_prefix, msg );
    return false;
}

void VulkanRenderer::_SetupDebug()
{
    _debug_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    _debug_callback_create_info.pfnCallback = VulkanDebugCallback;
    _debug_callback_create_info.flags =
        //		VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT |
        //		VK_DEBUG_REPORT_DEBUG_BIT_EXT |
        0;
    
    _instance_layers.push_back( "VK_LAYER_LUNARG_core_validation" );
    _instance_extensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
}

static PFN_vkCreateDebugReportCallbackEXT  fvkCreateDebugReportCallbackEXT = nullptr;
static PFN_vkDestroyDebugReportCallbackEXT fvkDestroyDebugReportCallbackEXT = nullptr;

void VulkanRenderer::_InitDebug()
{
    fvkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( _instance, "vkCreateDebugReportCallbackEXT" );
    fvkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr( _instance, "vkDestroyDebugReportCallbackEXT" );
    if( nullptr == fvkCreateDebugReportCallbackEXT || nullptr == fvkDestroyDebugReportCallbackEXT )
    {
        SYS_ASSERT( 0 && "Vulkan ERROR: Can't fetch debug function pointers." );
    }

    auto result = fvkCreateDebugReportCallbackEXT( _instance, &_debug_callback_create_info, nullptr, &_debug_report );
    vulkan_util::checkError( result );
}

void VulkanRenderer::_DeinitDebug()
{
    fvkDestroyDebugReportCallbackEXT( _instance, _debug_report, nullptr );
    _debug_report = nullptr;
}

#endif


void VulkanSample::initialize( VulkanRenderer* renderer, bxWindow* window )
{
    VkDevice device = renderer->_device;
    uint32_t queue_family_index = renderer->_graphics_family_index;

    _command_pool = vulkan_util::commandPoolCreate( device, queue_family_index );
    vkGetDeviceQueue( device, queue_family_index, 0, &_queue );

    VkCommandBuffer setup_cmd_buffer = vulkan_util::setupCommandBufferCreate( device, _command_pool );

    _swap_chain._InitSurface( window, renderer );
    _swap_chain._InitSwapChain( renderer );
    _swap_chain._InitSwapChainImages( renderer, setup_cmd_buffer );

    _CreateCommandBuffers( renderer );
    _CreateDepthStencil( renderer, setup_cmd_buffer );
    _CreateRenderPass( renderer );
    _CreateFramebuffer( renderer );
    _CreatePipelineCache( renderer );

    vulkan_util::setupCommandBufferFlush( &setup_cmd_buffer, device, _queue, _command_pool );
}

void VulkanSample::deinitialize( VulkanRenderer* renderer )
{
    _DestroyPipelineCache( renderer );
    _DestroyFramebuffer( renderer );
    _DestroyRenderPass( renderer );
    _DestroyDepthStencil( renderer );
    _DestroyCommandBuffers( renderer );
    
    _swap_chain._DeinitSwapChainImages( renderer );
    _swap_chain._DeinitSwapChain( renderer );
    _swap_chain._DeinitSurface( renderer->_instance );

    //renderer->commandBuffersDestroy( &_post_present_cmd_buffer, 1, _command_pool );
    //renderer->commandBuffersDestroy( &_pre_present_cmd_buffer, 1, _command_pool );
    //renderer->commandBuffersDestroy( _draw_cmd_buffers.data(), (u32)_draw_cmd_buffers.size(), _command_pool );

    vkDestroyCommandPool( renderer->_device, _command_pool, nullptr );
    renderer->_device = VK_NULL_HANDLE;
}

void VulkanSample::_CreateCommandBuffers( VulkanRenderer* renderer )
{
    VkDevice device = renderer->_device;
    vulkan_util::commandBuffersCreate( &_pre_present_cmd_buffer, 1, device, _command_pool );
    vulkan_util::commandBuffersCreate( &_post_present_cmd_buffer, 1, device, _command_pool );
    vulkan_util::commandBuffersCreate( &_draw_cmd_buffer, 1, device, _command_pool );
}

void VulkanSample::_DestroyCommandBuffers( VulkanRenderer* renderer )
{
    VkDevice device = renderer->_device;
    vulkan_util::commandBuffersDestroy( &_draw_cmd_buffer, 1, device, _command_pool );
    vulkan_util::commandBuffersDestroy( &_post_present_cmd_buffer, 1, device, _command_pool );
    vulkan_util::commandBuffersDestroy( &_pre_present_cmd_buffer, 1, device, _command_pool );
}

void VulkanSample::_CreateDepthStencil( VulkanRenderer* renderer, VkCommandBuffer setupCmdBuffer )
{
    
    VkFormat depth_format = renderer->_depth_format;

    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = depth_format;
    image_create_info.extent = { _swap_chain._width, _swap_chain._height, 1 };
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_create_info.flags = 0;

    VkMemoryAllocateInfo mem_alloc_info = {};
    mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc_info.pNext = NULL;
    mem_alloc_info.allocationSize = 0;
    mem_alloc_info.memoryTypeIndex = 0;

    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.pNext = NULL;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = depth_format;
    view_create_info.flags = 0;
    view_create_info.subresourceRange = {};
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;


    VkDevice device = renderer->_device;
    VkMemoryRequirements mem_reqs = {};
    VkResult result = VK_SUCCESS;

    result = vkCreateImage( device, &image_create_info, nullptr, &_depth_stencil.image );
    vulkan_util::checkError( result );

    vkGetImageMemoryRequirements( device, _depth_stencil.image, &mem_reqs );
    mem_alloc_info.allocationSize = mem_reqs.size;
    
    _depth_stencil.mem = renderer->deviceMemoryAllocate( mem_reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
    
    result = vkBindImageMemory( device, _depth_stencil.image, _depth_stencil.mem, 0 );
    vulkan_util::checkError( result );

    vulkan_util::setImageLayout( setupCmdBuffer, _depth_stencil.image,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );

    view_create_info.image = _depth_stencil.image;
    result = vkCreateImageView( device, &view_create_info, nullptr, &_depth_stencil.view );
    vulkan_util::checkError( result );
}

void VulkanSample::_DestroyDepthStencil( VulkanRenderer* renderer )
{
    VkDevice device = renderer->_device;

    vkDestroyImageView( device, _depth_stencil.view, nullptr );
    vkDestroyImage( device, _depth_stencil.image, nullptr );
    vkFreeMemory( device, _depth_stencil.mem, nullptr );
    _depth_stencil = {};
}

void VulkanSample::_CreateRenderPass( VulkanRenderer* renderer )
{
    VkAttachmentDescription attachments[2];
    attachments[0].format = _swap_chain._surface_format.format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachments[1].format = renderer->_depth_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_reference = {};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference = {};
    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = &depth_reference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.pNext = NULL;
    render_pass_create_info.attachmentCount = 2;
    render_pass_create_info.pAttachments = attachments;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 0;
    render_pass_create_info.pDependencies = NULL;

    VkDevice device = renderer->_device;
    VkResult result = vkCreateRenderPass( device, &render_pass_create_info, nullptr, &_render_pass );
    vulkan_util::checkError( result );
}

void VulkanSample::_DestroyRenderPass( VulkanRenderer* renderer )
{
    vkDestroyRenderPass( renderer->_device, _render_pass, nullptr );
    _render_pass = VK_NULL_HANDLE;
}

void VulkanSample::_CreateFramebuffer( VulkanRenderer* renderer )
{
    VkImageView attachments[2];

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = _depth_stencil.view;

    VkFramebufferCreateInfo frame_buffer_create_info = {};
    frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buffer_create_info.pNext = NULL;
    frame_buffer_create_info.renderPass = _render_pass;
    frame_buffer_create_info.attachmentCount = 2;
    frame_buffer_create_info.pAttachments = attachments;
    frame_buffer_create_info.width = _swap_chain._width;
    frame_buffer_create_info.height = _swap_chain._height;
    frame_buffer_create_info.layers = 1;

    // Create frame buffers for every swap chain image
    _framebuffers.resize( _swap_chain._image_count );
    VkDevice device = renderer->_device;
    for( size_t i = 0; i < _framebuffers.size(); i++ )
    {
        attachments[0] = _swap_chain._image_views[i];
        VkResult result = vkCreateFramebuffer( device, &frame_buffer_create_info, nullptr, &_framebuffers[i] );
        vulkan_util::checkError( result );
    }
}

void VulkanSample::_DestroyFramebuffer( VulkanRenderer* renderer )
{
    VkDevice device = renderer->_device;

    for( VkFramebuffer& fb : _framebuffers )
    {
        vkDestroyFramebuffer( device, fb, nullptr );
        fb = VK_NULL_HANDLE;
    }
}

void VulkanSample::_CreatePipelineCache( VulkanRenderer* renderer )
{
    VkDevice device = renderer->_device;
    VkPipelineCacheCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkResult result = vkCreatePipelineCache( device, &create_info, nullptr, &_pipeline_cache );
    vulkan_util::checkError( result );
}

void VulkanSample::_DestroyPipelineCache( VulkanRenderer* renderer )
{
    VkDevice device = renderer->_device;
    vkDestroyPipelineCache( device, _pipeline_cache, nullptr );
}

}////
