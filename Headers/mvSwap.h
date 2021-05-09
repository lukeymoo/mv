#ifndef HEADERS_MVSWAP_H_
#define HEADERS_MVSWAP_H_

#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.hpp>
// #include <vulkan/vulkan_xcb.h>

#include <iostream>
#include <string.h>
#include <vector>
#include <memory>
#include <cassert>

#include "mvDevice.h"

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
        Swap(){
            buffers = std::make_unique<std::vector<swapchain_buffer>>();
            images = std::make_unique<std::vector<vk::Image>>();
        }
        ~Swap(){}
        // creates vulkan surface & retreives basic info such as graphics queue index
        void init(std::weak_ptr<xcb_connection_t*> connection, std::weak_ptr<xcb_window_t> window);

        // map our swapchain interface with main engine class
        void map(std::weak_ptr<vk::Instance> instance,
                 std::weak_ptr<vk::PhysicalDevice> physical_device,
                 std::weak_ptr<mv::Device> mv_device);

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
        std::weak_ptr<mv::Device> mv_device;

    public:
        // owns
        
        std::shared_ptr<vk::SwapchainKHR> swapchain;
        
        // swap chain image handles + view into them
        std::unique_ptr<std::vector<swapchain_buffer>> buffers;
        
        // swapchain image handles
        std::unique_ptr<std::vector<vk::Image>> images;

        // references
        
        std::weak_ptr<xcb_connection_t*> xcb_conn;
        std::weak_ptr<xcb_window_t> xcb_win;

        // surface info structures
        
        vk::Format color_format;
        vk::Extent2D swap_extent;
        vk::ColorSpaceKHR color_space;

        // graphics queue index
        uint32_t graphics_index = UINT32_MAX;
    };
};

#endif