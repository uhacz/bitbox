#include "renderer_util.h"
#include <util/type.h>
#include <util/debug.h>

namespace bx{
namespace vulkan_util{

    void checkError( VkResult result )
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

    bool memoryTypeIndexGet( uint32_t* typeIndex, VkMemoryPropertyFlags propertyFlags, uint32_t typeBits, VkFlags properties )
{
    for( int i = 0; i < VK_MAX_MEMORY_TYPES; i++ )
    {
        if( ( typeBits & 1 ) == 1 )
        {
            if( ( propertyFlags & properties ) == properties )
            {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

bool supportedDepthFormatGet( VkFormat *depthFormat, VkPhysicalDevice gpu )
{
    // Since all depth formats may be optional, we need to find a suitable depth format to use
    // Start with the highest precision packed format
    VkFormat depthFormats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };
    const size_t n = sizeof( depthFormats ) / sizeof( *depthFormats );
    for( size_t i = 0; i < n; ++i )
    {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties( gpu, depthFormats[i], &formatProps );
        // Format must support depth stencil attachment for optimal tiling
        if( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT )
        {
            *depthFormat = depthFormats[i];
            return true;
        }
    }

    return false;
}

VkCommandPool commandPoolCreate( VkDevice device, uint32_t queueFamilyIndex )
{
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = queueFamilyIndex;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkResult result = vkCreateCommandPool( device, &create_info, nullptr, &command_pool );
    vulkan_util::checkError( result );
    return command_pool;
}

bool commandBuffersCreate( VkCommandBuffer* buffers, uint32_t count, VkDevice device, VkCommandPool pool, VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY */ )
{
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool;
    alloc_info.level = level;
    alloc_info.commandBufferCount = count;
    VkResult result = vkAllocateCommandBuffers( device, &alloc_info, buffers );
    vulkan_util::checkError( result );
    return ( result == VK_SUCCESS );
}

void commandBuffersDestroy( VkCommandBuffer* buffers, uint32_t count, VkDevice device, VkCommandPool pool )
{
    vkFreeCommandBuffers( device, pool, count, buffers );
    for( u32 i = 0; i < count; ++i )
        buffers[i] = VK_NULL_HANDLE;
}

VkCommandBuffer setupCommandBufferCreate( VkDevice device, VkCommandPool pool, bool beginCmdBuffer /*= true */ )
{
    VkCommandBuffer setup_cmd_buffer = VK_NULL_HANDLE;
    bool bres = commandBuffersCreate( &setup_cmd_buffer, 1, device, pool );

    if( beginCmdBuffer )
    {
        VkCommandBufferBeginInfo setup_cmd_buffer_begin_info = {};
        setup_cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VkResult result = vkBeginCommandBuffer( setup_cmd_buffer, &setup_cmd_buffer_begin_info );
        SYS_ASSERT( result == VK_SUCCESS );
    }
    return setup_cmd_buffer;
}

void setupCommandBufferFlush( VkCommandBuffer* cmdBuffer, VkDevice device, VkQueue queue, VkCommandPool pool )
{
    VkResult result = vkEndCommandBuffer( *cmdBuffer );
    SYS_ASSERT( result == VK_SUCCESS );

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = cmdBuffer;

    result = vkQueueSubmit( queue, 1, &submit_info, VK_NULL_HANDLE );
    SYS_ASSERT( result == VK_SUCCESS );

    result = vkQueueWaitIdle( queue );
    SYS_ASSERT( result == VK_SUCCESS );

    if( pool != VK_NULL_HANDLE )
    {
        commandBuffersDestroy( cmdBuffer, 1, device, pool );
        cmdBuffer[0] = VK_NULL_HANDLE;
    }
}

void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange )
{
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    image_memory_barrier.oldLayout = oldImageLayout;
    image_memory_barrier.newLayout = newImageLayout;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange = subresourceRange;

    // Source layouts (old)

    // Undefined layout
    // Only allowed as initial layout!
    // Make sure any writes to the image have been finished
    if( oldImageLayout == VK_IMAGE_LAYOUT_PREINITIALIZED )
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    // Old layout is color attachment
    // Make sure any writes to the color buffer have been finished
    if( oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL )
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    // Old layout is depth/stencil attachment
    // Make sure any writes to the depth/stencil buffer have been finished
    if( oldImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL )
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    // Old layout is transfer source
    // Make sure any reads from the image have been finished
    if( oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL )
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    // Old layout is shader read (sampler, input attachment)
    // Make sure any shader reads from the image have been finished
    if( oldImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    // Target layouts (new)

    // New layout is transfer destination (copy, blit)
    // Make sure any copyies to the image have been finished
    if( newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL )
    {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    // New layout is transfer source (copy, blit)
    // Make sure any reads from and writes to the image have been finished
    if( newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL )
    {
        image_memory_barrier.srcAccessMask = image_memory_barrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    // New layout is color attachment
    // Make sure any writes to the color buffer hav been finished
    if( newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL )
    {
        image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    // New layout is depth attachment
    // Make sure any writes to depth/stencil buffer have been finished
    if( newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL )
    {
        image_memory_barrier.dstAccessMask = image_memory_barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    // New layout is shader read (sampler, input attachment)
    // Make sure any writes to the image have been finished
    if( newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    // Put barrier on top
    VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageFlags,
        destStageFlags,
        0,
        0, nullptr,
        0, nullptr,
        1, &image_memory_barrier );
}

void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout )
{
    VkImageSubresourceRange subresource_range = {};
    subresource_range.aspectMask = aspectMask;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.layerCount = 1;
    setImageLayout( cmdbuffer, image, aspectMask, oldImageLayout, newImageLayout, subresource_range );
}



VkImageMemoryBarrier prePresentBarrier( VkImage presentImage )
{
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;

    image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_memory_barrier.dstAccessMask = 0;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    image_memory_barrier.image = presentImage;
    return image_memory_barrier;
}

VkImageMemoryBarrier postPresentBarrier( VkImage presentImage )
{
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;

    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    image_memory_barrier.image = presentImage;
    return image_memory_barrier;
}

}}///
