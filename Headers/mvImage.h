#ifndef HEADERS_MVIMAGE_H_
#define HEADERS_MVIMAGE_H_

#include <vulkan/vulkan.h>
#include <stdexcept>

#include "stb_image.h"
#include "mvDevice.h"

#include <iostream>

namespace mv
{
    struct Image
    {
        mv::Device *device;

        VkImage image = nullptr;
        VkImageView image_view = nullptr;
        VkSampler sampler = nullptr;
        VkDeviceMemory memory = nullptr;
        VkDescriptorImageInfo descriptor = {};
        VkMemoryRequirements memory_requirements = {};

        Image(void) {}
        ~Image(void) {} // image is cleaned up in engine

        struct ImageCreateInfo
        {
            VkImageUsageFlags usage;
            VkMemoryPropertyFlags memory_properties;
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        };

        void create(mv::Device *device, ImageCreateInfo &c_info, std::string image_filename)
        {
            assert(device->m_command_pool != nullptr);
            this->device = device;
            int width;
            int height;
            int channels;

            // load image
            stbi_uc *raw_image = stbi_load(image_filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            VkDeviceSize image_size = width * height * 4;

            if (!raw_image)
            {
                throw std::runtime_error("Failed to load image => " + image_filename);
            }

            VkBuffer staging_buffer = nullptr;
            VkDeviceMemory staging_memory = nullptr;
            void *staging_mapped = nullptr;

            // create staging buffer
            create_staging_buffer(image_size, staging_buffer, staging_memory);
            if (vkMapMemory(device->device, staging_memory, 0, image_size, 0, &staging_mapped) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to map staging buffer memory");
            }

            // copy raw image to staging memory
            memcpy(staging_mapped, raw_image, static_cast<size_t>(image_size));
            vkUnmapMemory(device->device, staging_memory);

            // free stb image
            stbi_image_free(raw_image);

            // create vulkan image that will store our pixel data
            VkImageCreateInfo image_info = {};
            image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.format = c_info.format;
            image_info.extent.width = width;
            image_info.extent.height = height;
            image_info.extent.depth = 1;
            image_info.mipLevels = 1;
            image_info.arrayLayers = 1;
            image_info.samples = VK_SAMPLE_COUNT_1_BIT;
            image_info.tiling = c_info.tiling;
            image_info.usage = c_info.usage; // VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (vkCreateImage(device->device, &image_info, nullptr, &image) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create vulkan image");
            }

            vkGetImageMemoryRequirements(device->device, image, &memory_requirements);

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.memoryTypeIndex = device->get_memory_type(memory_requirements.memoryTypeBits,
                                                                 c_info.memory_properties);
            alloc_info.allocationSize = memory_requirements.size;

            if (vkAllocateMemory(device->device, &alloc_info, nullptr, &memory) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate memory for vulkan image");
            }

            if (vkBindImageMemory(device->device, image, memory, 0) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to bind vulkan image & allocated memory");
            }

            // transition image to transfer dst
            transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            // copy staging buffer to image
            VkCommandBuffer command_buffer = begin_command_buffer();

            VkBufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

            vkCmdCopyBufferToImage(command_buffer, staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            end_command_buffer(command_buffer);

            // transition image to shader read only
            transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // create view into image
            VkImageViewCreateInfo view_info = {};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = c_info.format;
            view_info.components = {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A};
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device->device, &view_info, nullptr, &image_view) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create view into vulkan image");
            }

            // create image sampler
            VkSamplerCreateInfo sampler_info = {};
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.anisotropyEnable = VK_TRUE;
            sampler_info.maxAnisotropy = device->properties.limits.maxSamplerAnisotropy;
            sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            sampler_info.unnormalizedCoordinates = VK_FALSE;
            sampler_info.compareEnable = VK_FALSE;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

            if (vkCreateSampler(device->device, &sampler_info, nullptr, &sampler) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image sampler");
            }

            // cleanup staging resources
            vkDestroyBuffer(device->device, staging_buffer, nullptr);
            vkFreeMemory(device->device, staging_memory, nullptr);

            // setup the descriptor
            descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptor.imageView = image_view;
            descriptor.sampler = sampler;

            std::cout << "Loaded model texture" << std::endl;

            return;
        }

        void create_staging_buffer(VkDeviceSize &size, VkBuffer &staging_buffer, VkDeviceMemory &staging_memory)
        {
            VkMemoryRequirements staging_req = {};

            VkBufferCreateInfo buffer_info = {};
            buffer_info.size = size;
            buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

            if (vkCreateBuffer(device->device, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Image helper failed to create staging buffer");
            }

            vkGetBufferMemoryRequirements(device->device, staging_buffer, &staging_req);

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.memoryTypeIndex = device->get_memory_type(staging_req.memoryTypeBits,
                                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            alloc_info.allocationSize = staging_req.size;

            if (vkAllocateMemory(device->device, &alloc_info, nullptr, &staging_memory) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate memory for staging buffer");
            }

            if (vkBindBufferMemory(device->device, staging_buffer, staging_memory, 0) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to bind staging buffer & memory");
            }
        }

        void transition_image_layout(VkImage image, VkFormat format, VkImageLayout initial_layout, VkImageLayout final_layout)
        {
            VkCommandBuffer command_buffer = begin_command_buffer();

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = initial_layout;
            barrier.newLayout = final_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags source_stage;
            VkPipelineStageFlags destination_stage;

            if (initial_layout == VK_IMAGE_LAYOUT_UNDEFINED && final_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (initial_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && final_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else
            {
                throw std::runtime_error("Layout transition requested is unsupported");
            }

            vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &barrier);

            end_command_buffer(command_buffer);
            return;
        }

        VkCommandBuffer begin_command_buffer(void)
        {
            VkCommandBufferAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.commandPool = device->m_command_pool;
            alloc_info.commandBufferCount = 1;

            VkCommandBuffer command_buffer;
            vkAllocateCommandBuffers(device->device, &alloc_info, &command_buffer);

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(command_buffer, &begin_info);
            return command_buffer;
        }

        void end_command_buffer(VkCommandBuffer command_buffer)
        {
            vkEndCommandBuffer(command_buffer);

            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;

            vkQueueSubmit(device->graphics_queue, 1, &submit_info, nullptr);
            vkQueueWaitIdle(device->graphics_queue);

            vkFreeCommandBuffers(device->device, device->m_command_pool, 1, &command_buffer);
            return;
        }

        void destroy(void)
        {
            if (sampler)
            {
                vkDestroySampler(device->device, sampler, nullptr);
                sampler = nullptr;
            }
            if (image_view)
            {
                vkDestroyImageView(device->device, image_view, nullptr);
                image_view = nullptr;
            }
            if (image)
            {
                vkDestroyImage(device->device, image, nullptr);
                image = nullptr;
            }
            if (memory)
            {
                vkFreeMemory(device->device, memory, nullptr);
                memory = nullptr;
            }
            return;
        }
    };
};

#endif