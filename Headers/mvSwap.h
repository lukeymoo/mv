#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <cassert>
#include <iostream>
#include <memory>
#include <string.h>
#include <vector>

// container for swap chain image
// contains swap image + a view into it
class Image;
struct SwapchainBuffer
{
    vk::Image image;
    vk::ImageView view;
    std::unique_ptr<Image> msaaImage;
};

class Swap
{
  public:
    Swap(void);
    ~Swap();

    // creates vulkan surface & retreives basic info such as graphics queue
    // index
    void init(GLFWwindow *p_GLFWwindow, const vk::Instance &p_Instance, const vk::PhysicalDevice &p_PhysicalDevice);

    // create vulkan swap chain, retrieve images & create views into them
    void create(const vk::PhysicalDevice &p_PhysicalDevice, const vk::Device &p_LogicalDevice, uint32_t &p_WindowWidth,
                uint32_t &p_WindowHeight);

    // destroy resources owned by this interface
    void cleanup(const vk::Instance &p_Instance, const vk::Device &p_LogicalDevice, bool p_ShouldDestroySurface = true);

  public:
    // clang-format off
    vk::SurfaceKHR               surface;
    vk::SwapchainKHR             swapchain;
    std::vector<SwapchainBuffer> buffers; // image handles + views
    // Info
    uint32_t          graphicsIndex = UINT32_MAX;
    vk::Format        colorFormat;
    vk::Format        depthFormat;
    vk::ColorSpaceKHR colorSpace;
    vk::Extent2D      swapExtent;
    vk::SampleCountFlagBits sampleCount;
    // clang-format on
};
