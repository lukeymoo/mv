#ifndef HEADERS_MVSWAP_H_
#define HEADERS_MVSWAP_H_

#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>

#include <cassert>
#include <iostream>
#include <memory>
#include <string.h>
#include <vector>

#include "mvDevice.h"

namespace mv {
  // container for swap chain image
  // contains swap image + a view into it
  struct swapchain_buffer {
    vk::Image image;
    vk::ImageView view;
  };

  struct Swap {
  public:
    Swap(void) {
      // initialize containers
      buffers = std::make_unique<std::vector<swapchain_buffer>>();
      images = std::make_unique<std::vector<vk::Image>>();
    }
    ~Swap() {
    }
    // creates vulkan surface & retreives basic info such as graphics queue index
    void init(Display *display, Window window, const vk::Instance &inst,
              const vk::PhysicalDevice &p_dvc);

    // map our swapchain interface with main engine class
    // void map(vk::Instance &instance, vk::PhysicalDevice &physical_device, mv::Device &mv_device);

    // create vulkan swap chain, retrieve images & create views into them
    void create(const vk::PhysicalDevice &p_dvc, const mv::Device &m_dvc, uint32_t &w, uint32_t &h);

    // destroy resources owned by this interface
    void cleanup(const vk::Instance &inst, const mv::Device &m_dvc,
                 bool should_destroy_surface = true);

  public:
    // owns
    std::unique_ptr<vk::SurfaceKHR> surface;
    std::unique_ptr<vk::SwapchainKHR> swapchain;
    std::unique_ptr<std::vector<swapchain_buffer>> buffers; // image handles + views
    std::unique_ptr<std::vector<vk::Image>> images;         // swap image handles

    // surface info structures
    vk::Format color_format;
    vk::Extent2D swap_extent;
    vk::ColorSpaceKHR color_space;

    // graphics queue index
    uint32_t graphics_index = UINT32_MAX;
  };
}; // namespace mv

#endif