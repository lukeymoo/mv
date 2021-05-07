#ifndef HEADERS_MVSWAP_H_
#define HEADERS_MVSWAP_H_

#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <string.h>
#include <vector>
#include <memory>
#include <cassert>

namespace mv
{
    // container for swap chain image
    // contains swap image + a view into it
    struct swapchain_buffer
    {
        vk::Image image;
        vk::ImageView view;
    };

    struct Swap
    {
    public:
        // creates vulkan surface & retreives basic info such as graphics queue index
        void init(std::weak_ptr<xcb_connection_t> connection, std::weak_ptr<xcb_window_t> window);

        // map our swapchain interface with main engine class
        void map(std::weak_ptr<vk::Instance> instance,
                 std::weak_ptr<vk::PhysicalDevice> physical_device,
                 std::weak_ptr<vk::Device> device);

        // create vulkan swap chain, retrieve images & create views into them
        void create(uint32_t& width, uint32_t& height);

        // destroy resources owned by this interface
        void cleanup(bool should_destroy_surface = true);

    private:
        // owns
        std::shared_ptr<vk::SurfaceKHR> surface;

        // references
        std::weak_ptr<vk::Instance> instance;
        std::weak_ptr<vk::PhysicalDevice> physical_device;
        std::weak_ptr<vk::Device> logical_device;

    public:
        // owns
        std::shared_ptr<vk::SwapchainKHR> swapchain;
        // swap chain image handles + view into them
        std::unique_ptr<std::vector<swapchain_buffer>> buffers;
        // swapchain image handles
        std::unique_ptr<std::vector<vk::Image>> images;

        // references
        std::weak_ptr<xcb_connection_t> xcb_conn;
        std::weak_ptr<xcb_window_t> xcb_win;

        // surface info structures
        vk::Format color_format;
        vk::Extent2D swap_extent;
        vk::ColorSpaceKHR color_space;
        uint32_t image_count = 0; // swap chain image count

        // graphics queue index
        uint32_t graphics_index = UINT32_MAX;
    };
};

#endif