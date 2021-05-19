#pragma once

#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "stb_image.h"

#include "mvEngine.h"

#include <iostream>

class Image
{
  public:
    Image(void);
    ~Image(); // Image cleaned up in engine

    Engine *engine = nullptr; // initialized in call to create()

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

    void create(Engine *p_Engine, ImageCreateInfo &p_ImageCreateInfo,
                std::string p_ImageFilename);

    void createStagingBuffer(vk::DeviceSize &p_BufferSize,
                             vk::Buffer &p_StagingBuffer,
                             vk::DeviceMemory &p_StagingMemory);

    void transitionImageLayout(vk::Image *p_TargetImage,
                               vk::ImageLayout p_StartingLayout,
                               vk::ImageLayout p_EndingLayout);

    vk::CommandBuffer beginCommandBuffer(void);

    void endCommandBuffer(vk::CommandBuffer p_CommandBuffer);

    void destroy(void);
};