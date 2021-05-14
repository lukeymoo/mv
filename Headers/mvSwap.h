#ifndef HEADERS_MVSWAP_H_
#define HEADERS_MVSWAP_H_

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <cassert>
#include <iostream>
#include <memory>
#include <string.h>
#include <vector>

#include "mvDevice.h"

namespace mv
{
    // container for swap chain image
    // contains swap image + a view into it
    struct SwapchainBuffer
    {
        vk::Image image;
        vk::ImageView view;
    };

    struct Swap
    {
      public:
        Swap(void);
        ~Swap();
        // creates vulkan surface & retreives basic info such as graphics queue index
        void init(GLFWwindow *p_GLFWwindow, const vk::Instance &p_Instance, const vk::PhysicalDevice &p_PhysicalDevice);

        // create vulkan swap chain, retrieve images & create views into them
        void create(const vk::PhysicalDevice &p_PhysicalDevice, const mv::Device &p_MvDevice, uint32_t &p_WindowWidth,
                    uint32_t &p_WindowHeight);

        // destroy resources owned by this interface
        void cleanup(const vk::Instance &p_Instance, const mv::Device &p_MvDevice, bool p_ShouldDestroySurface = true);

      public:
        // Owned resources
        // clang-format off
        std::unique_ptr<vk::SurfaceKHR>                       surface;
        std::unique_ptr<vk::SwapchainKHR>                     swapchain;
        std::unique_ptr<std::vector<struct SwapchainBuffer>>  buffers; // image handles + views
        // std::unique_ptr<std::vector<vk::Image>>               images;  // swap image handles
        // Info
        uint32_t                                              graphicsIndex = UINT32_MAX;
        vk::Format                                            colorFormat;
        vk::Format                                            depthFormat;
        vk::ColorSpaceKHR                                     colorSpace;
        vk::Extent2D                                          swapExtent;
        // clang-format on
    };
}; // namespace mv

#endif