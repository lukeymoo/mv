#ifndef HEADERS_MVWINDOW_H_
#define HEADERS_MVWINDOW_H_

#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

#include <iostream>
#include <string.h>
#include <memory>

#include "mvException.h"
#include "mvWindow.h"
#include "mvKeyboard.h"
#include "mvMouse.h"
#include "mvDevice.h"
#include "mvSwap.h"
#include "mvInit.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

const std::vector<const char *> requestedValidationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> requestedInstanceExtensions = {
    "VK_EXT_debug_utils",
    "VK_KHR_surface",
    "VK_KHR_xcb_surface"};

const std::vector<const char *> requestedDeviceExtensions = {
    "VK_KHR_swapchain"};

namespace mv
{
    class Window
    {
    public:
        class Exception : public BException
        {
        public:
            Exception(int line, std::string file, std::string message);
            ~Exception(void);

            std::string getType(void);
        };

    public:
        Window(int w, int h, const char *title);
        ~Window();

        void go(void);

        VkResult createInstance(void);
        bool initVulkan(void);

    private:
        void checkValidationSupport(void);
        void checkInstanceExt(void);
        void createCommandBuffers(void);
        void createSynchronizationPrimitives(void);
        void setupDepthStencil(void);

    public:
        bool goodInit = true;
        Keyboard kbd;
        Mouse mouse;

        mv::Device *device;

    protected: // Graphics
        uint32_t windowWidth = 0;
        uint32_t windowHeight = 0;
        VkInstance m_Instance = nullptr;
        VkPhysicalDevice m_PhysicalDevice = nullptr;
        VkDevice m_Device = nullptr;
        VkQueue m_GraphicsQueue = nullptr;
        VkCommandPool m_CommandPool = nullptr;
        Swap swapChain;

        std::vector<VkCommandBuffer> cmdBuffers;
        std::vector<VkFence> waitFences;

        VkSubmitInfo submitInfo = {};

        VkFormat format{};
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceMemoryProperties memoryProperties = {};
        VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        struct
        {
            VkSemaphore presentComplete;
            VkSemaphore renderComplete;
        } semaphores;

    private: // Window Management
        xcb_connection_t *connection;
        xcb_window_t window;
        xcb_generic_event_t *event;
        int screen;
        bool running = true;
    };
};

#define W_EXCEPT(string) throw Exception(__LINE__, __FILE__, string)
#define VK_CHECK(result)                                                \
    if (result != VK_SUCCESS)                                           \
    {                                                                   \
        throw Exception(__LINE__, __FILE__, "Vulkan operation failed"); \
    }

VKAPI_ATTR
VkBool32
    VKAPI_CALL
    debugMessageProcessor(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
        void *user_data);

#endif