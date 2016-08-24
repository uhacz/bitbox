#pragma once

#include <vulkan/vulkan.h>

#define VK_FLAGS_NONE 0

namespace bx
{
    namespace vulkan_util
    {
        void checkError( VkResult res );
        bool supportedDepthFormatGet( VkFormat *depthFormat, VkPhysicalDevice gpu );

        VkCommandPool commandPoolCreate( VkDevice device, uint32_t queueFamilyIndex );
        bool commandBuffersCreate( VkCommandBuffer* buffers, uint32_t count, VkDevice device, VkCommandPool pool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY );
        void commandBuffersDestroy( VkCommandBuffer* buffers, uint32_t count, VkDevice device, VkCommandPool pool );
        
        VkCommandBuffer setupCommandBufferCreate( VkDevice device, VkCommandPool pool, bool beginCmdBuffer = true );
        void setupCommandBufferFlush( VkCommandBuffer* cmdBuffer, VkDevice device, VkQueue queue, VkCommandPool pool );

        // Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
        void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange );
        void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout );

        // Returns a pre-present image memory barrier
        // Transforms the image's layout from color attachment to present khr
        VkImageMemoryBarrier prePresentBarrier( VkImage presentImage );

        // Returns a post-present image memory barrier
        // Transforms the image's layout back from present khr to color attachment
        VkImageMemoryBarrier postPresentBarrier( VkImage presentImage );
    }

}///
