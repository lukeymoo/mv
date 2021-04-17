#ifndef HEADERS_MVSWAP_H_
#define HEADERS_MVSWAP_H_

#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

#include <iostream>
#include <string.h>
#include <vector>
#include <memory>
#include <cassert>

namespace mv
{
    typedef struct _SwapChainBuffers
    {
        VkImage image;
        VkImageView view;
    } SwapChainBuffer;

    class Swap
    {
    private:
        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkSurfaceKHR surface;

        /* Function pointers must be loaded */
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
        PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
        PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
        PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
        PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
        PFN_vkQueuePresentKHR fpQueuePresentKHR;

    public:
        VkFormat colorFormat = {};
        VkExtent2D swapExtent = {};
        VkColorSpaceKHR colorSpace = {};
        VkSwapchainKHR swapchain = nullptr;
        uint32_t imageCount = 0; // swap chain image count
        xcb_connection_t *conn;
        xcb_window_t window;

        std::vector<VkImage> images; // swapchain image handles
        std::vector<SwapChainBuffer> buffers;
        uint32_t queueIndex = UINT32_MAX; // graphics queue index

        void initSurface(xcb_connection_t *conn, xcb_window_t &window);
        void create(uint32_t *width, uint32_t *height);
        void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
        void cleanup(void);
    };
};

#endif