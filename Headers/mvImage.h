#ifndef HEADERS_MVIMAGE_H_
#define HEADERS_MVIMAGE_H_

#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "mvDevice.h"
#include "stb_image.h"

#include <iostream>

namespace mv
{
    struct Image
    {
        Image(void);
        ~Image(); // Image cleaned up in engine

        // owns
        vk::Image image;
        vk::ImageView imageView;
        vk::Sampler sampler;
        vk::DeviceMemory memory;

        // info structs
        vk::DescriptorImageInfo descriptor;
        vk::MemoryRequirements memoryRequirements;

        struct ImageCreateInfo
        {
            // clang-format off
            vk::ImageUsageFlags     usage;
            vk::MemoryPropertyFlags memoryProperties;
            vk::Format              format = vk::Format::eR8G8B8A8Srgb;
            vk::ImageTiling         tiling = vk::ImageTiling::eOptimal;
            // clang-format on
        };

        void create(const mv::Device &p_MvDevice, struct ImageCreateInfo &p_ImageCreateInfo,
                    std::string p_ImageFilename);

        void createStagingBuffer(const mv::Device &p_MvDevice, vk::DeviceSize &p_BufferSize,
                                 vk::Buffer &p_StagingBuffer, vk::DeviceMemory &p_StagingMemory);

        void transitionImageLayout(const mv::Device &p_MvDevice, vk::Image *p_TargetImage,
                                   vk::ImageLayout p_StartingLayout, vk::ImageLayout p_EndingLayout);

        vk::CommandBuffer beginCommandBuffer(const mv::Device &p_MvDevice);

        void endCommandBuffer(const mv::Device &p_MvDevice, vk::CommandBuffer p_CommandBuffer);

        void destroy(const mv::Device &p_MvDevice);
    };
}; // namespace mv

#endif