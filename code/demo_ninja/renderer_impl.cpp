#include "renderer_impl.h"
#include <util/debug.h>
#include <vector>

#include <util/common.h>

namespace bx
{

void VulkanWindow::_InitSurface( bxWindow* window, VulkanRenderer* renderer )
{
    VkInstance vkInstance = renderer->_instance;
    VkPhysicalDevice gpu = renderer->_gpu;
    
    VkWin32SurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = window->hinstance;
    create_info.hwnd = window->hwnd;
    auto result = vkCreateWin32SurfaceKHR( vkInstance, &create_info, nullptr, &_surface );
    vulkanCheckError( result );

    VkBool32 WSI_supported = false;
    result = vkGetPhysicalDeviceSurfaceSupportKHR( gpu, renderer->_graphics_family_index, _surface, &WSI_supported );
    vulkanCheckError( result );
    if( !WSI_supported )
    {
        SYS_ASSERT( 0 && "WSI not supported" );
    }

    _surface_size_x = window->width;
    _surface_size_y = window->height;

    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR( gpu, _surface, &_surface_capabilities );
    vulkanCheckError( result );

    if( _surface_capabilities.currentExtent.width < UINT32_MAX )
    {
        _surface_size_x = _surface_capabilities.currentExtent.width;
        _surface_size_y = _surface_capabilities.currentExtent.height;
    }

    {
        uint32_t format_count = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR( gpu, _surface, &format_count, nullptr );
        vulkanCheckError( result );

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

void VulkanWindow::_DeinitSurface( VkInstance vkInstance )
{
    vkDestroySurfaceKHR( vkInstance, _surface, nullptr );
    _surface = VK_NULL_HANDLE;
}

void VulkanWindow::_InitSwapChain( VulkanRenderer* renderer )
{
    _swapchain_image_count = clamp( _swapchain_image_count, _surface_capabilities.minImageCount, _surface_capabilities.maxImageCount );
    _swapchain_image_count = maxOfPair( 2u, _swapchain_image_count ); // ensure at least double buffering 

    VkResult result = VK_SUCCESS;

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        u32 present_mode_count = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR( renderer->_gpu, _surface, &present_mode_count, nullptr );
        vulkanCheckError( result );

        std::vector<VkPresentModeKHR> present_modes( present_mode_count );
        result = vkGetPhysicalDeviceSurfacePresentModesKHR( renderer->_gpu, _surface, &present_mode_count, present_modes.data() );
        vulkanCheckError( result );

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
    swapchain_create_info.minImageCount = _swapchain_image_count;
    swapchain_create_info.imageFormat = _surface_format.format;
    swapchain_create_info.imageColorSpace = _surface_format.colorSpace;
    swapchain_create_info.imageExtent.width = _surface_size_x;
    swapchain_create_info.imageExtent.height = _surface_size_y;
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
    vulkanCheckError( result );

    result = vkGetSwapchainImagesKHR( renderer->_device, _swapchain, &_swapchain_image_count, nullptr );
    vulkanCheckError( result );
}

void VulkanWindow::_DeinitSwapChain( VulkanRenderer* renderer )
{
    vkDestroySwapchainKHR( renderer->_device, _swapchain, nullptr );
    _swapchain = VK_NULL_HANDLE;
}

void VulkanWindow::_InitSwapChainImages( VulkanRenderer* renderer )
{
    _swapchain_images.resize( _swapchain_image_count );
    _swapchain_image_views.resize( _swapchain_image_count );

    VkDevice device = renderer->_device;

    auto result = vkGetSwapchainImagesKHR( device, _swapchain, &_swapchain_image_count, _swapchain_images.data() );
    vulkanCheckError( result );

    for( u32 i = 0; i < _swapchain_image_count; ++i )
    {
        VkImageViewCreateInfo view_create_info = {};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.image = _swapchain_images[i];
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

        result = vkCreateImageView( device, &view_create_info, nullptr, &_swapchain_image_views[i] );
        vulkanCheckError( result );
    }
}

void VulkanWindow::_DeinitSwapChainImages( VulkanRenderer* renderer )
{
    for( u32 i = 0; i < _swapchain_image_count; ++i )
    {
        vkDestroyImageView( renderer->_device, _swapchain_image_views[i], nullptr );
        _swapchain_image_views[i] = VK_NULL_HANDLE;
    }
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
    vulkanCheckError( res );
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
    vulkanCheckError( res );

    std::vector<VkPhysicalDevice> gpus;
    gpus.resize( gpu_count );
    res = vkEnumeratePhysicalDevices( _instance, &gpu_count, gpus.data() );
    vulkanCheckError( res );
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
    vulkanCheckError( res );
}

void VulkanRenderer::_DestroyDevice()
{
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
    vulkanCheckError( result );
}

void VulkanRenderer::_DeinitDebug()
{
    fvkDestroyDebugReportCallbackEXT( _instance, _debug_report, nullptr );
    _debug_report = nullptr;
}
#endif

void vulkanCheckError( VkResult result )
{
    if( result < 0 )
    {
        switch( result )
        {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            printf( "VK_ERROR_OUT_OF_HOST_MEMORY\n" );
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            printf( "VK_ERROR_OUT_OF_DEVICE_MEMORY\n" );
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            printf( "VK_ERROR_INITIALIZATION_FAILED\n" );
            break;
        case VK_ERROR_DEVICE_LOST:
            printf( "VK_ERROR_DEVICE_LOST\n" );
            break;
        case VK_ERROR_MEMORY_MAP_FAILED:
            printf( "VK_ERROR_MEMORY_MAP_FAILED\n" );
            break;
        case VK_ERROR_LAYER_NOT_PRESENT:
            printf( "VK_ERROR_LAYER_NOT_PRESENT\n" );
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            printf( "VK_ERROR_EXTENSION_NOT_PRESENT\n" );
            break;
        case VK_ERROR_FEATURE_NOT_PRESENT:
            printf( "VK_ERROR_FEATURE_NOT_PRESENT\n" );
            break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            printf( "VK_ERROR_INCOMPATIBLE_DRIVER\n" );
            break;
        case VK_ERROR_TOO_MANY_OBJECTS:
            printf( "VK_ERROR_TOO_MANY_OBJECTS\n" );
            break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            printf( "VK_ERROR_FORMAT_NOT_SUPPORTED\n" );
            break;
        case VK_ERROR_SURFACE_LOST_KHR:
            printf( "VK_ERROR_SURFACE_LOST_KHR\n" );
            break;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            printf( "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR\n" );
            break;
        case VK_SUBOPTIMAL_KHR:
            printf( "VK_SUBOPTIMAL_KHR\n" );
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            printf( "VK_ERROR_OUT_OF_DATE_KHR\n" );
            break;
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            printf( "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR\n" );
            break;
        case VK_ERROR_VALIDATION_FAILED_EXT:
            printf( "VK_ERROR_VALIDATION_FAILED_EXT\n" );
            break;
        default:
            break;
        }
        SYS_ASSERT( false && "Vulkan runtime error." );
    }
}



}////
