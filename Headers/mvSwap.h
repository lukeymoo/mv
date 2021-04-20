#ifndef HEADERS_MVSWAP_H_
#define HEADERS_MVSWAP_H_

#include <X11/Xlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>

#include <iostream>
#include <string.h>
#include <vector>
#include <memory>
#include <cassert>

namespace mv
{
    typedef struct _swap_chain_buffers
    {
        VkImage image;
        VkImageView view;
    } swap_chain_buffer;

    class Swap
    {
    private:
        VkInstance instance;
        VkPhysicalDevice physical_device;
        VkDevice device;
        VkSurfaceKHR surface;

        /* Function pointers must be loaded */
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR fp_get_physical_device_surface_support_khr;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fp_get_physical_device_surface_capabilities_khr;
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fp_get_physical_device_surface_formats_khr;
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fp_get_physical_device_surface_present_modes_khr;
        PFN_vkCreateSwapchainKHR fp_create_swapchain_khr;
        PFN_vkDestroySwapchainKHR fp_destroy_swapchain_khr;
        PFN_vkGetSwapchainImagesKHR fp_get_swapchain_images_khr;
        PFN_vkAcquireNextImageKHR fp_acquire_next_image_khr;
        PFN_vkQueuePresentKHR fp_queue_present_khr;

    public:
        VkFormat color_format = {};
        VkExtent2D swap_extent = {};
        VkColorSpaceKHR color_space = {};
        VkSwapchainKHR swapchain = nullptr;
        uint32_t image_count = 0; // swap chain image count
        Display* display;
        Window window;

        std::vector<VkImage> images; // swapchain image handles
        std::vector<swap_chain_buffer> buffers;
        uint32_t queue_index = UINT32_MAX; // graphics queue index

        void init_surface(Display *disp, Window &window);
        void create(uint32_t *width, uint32_t *height);
        void connect(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device);
        void cleanup(bool should_destroy_surface = true);
    };
};

#endif