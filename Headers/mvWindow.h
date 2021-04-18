#ifndef HEADERS_MVWINDOW_H_
#define HEADERS_MVWINDOW_H_

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>

#include <filesystem>
#include <fstream>
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

const size_t MAX_IN_FLIGHT = 2;
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

const std::vector<const char *> requestedValidationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> requestedInstanceExtensions = {
    "VK_EXT_debug_utils",
    "VK_KHR_surface",
    "VK_KHR_xlib_surface"};

const std::vector<const char *> requestedDeviceExtensions = {
    "VK_KHR_swapchain"};

namespace mv
{
    class MWindow
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
        MWindow(int w, int h, const char *title);
        ~MWindow();

        MWindow &operator=(const MWindow &) = delete;
        MWindow(const MWindow &) = delete;

        VkResult createInstance(void);

        //void go(void);
        void prepare(void);

        XEvent createEvent(const char *eventType);
        void handleXEvent(void);

    private:
        bool initVulkan(void);
        void checkValidationSupport(void);
        void checkInstanceExt(void);
        void createCommandBuffers(void);
        void createSynchronizationPrimitives(void);
        void setupDepthStencil(void);
        void setupRenderPass(void);
        void createPipelineCache(void);
        void setupFramebuffer(void);

        void destroyCommandBuffers(void);
        void destroyCommandPool(void);

    public:
        VkClearColorValue defaultClearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};
        bool goodInit = true;
        Keyboard kbd;
        Mouse mouse;

        mv::Device *device;

    protected: // Graphics
        Display *display;
        Window window;
        XEvent event;

        int screen;
        bool running = true;

        uint32_t windowWidth = 0;
        uint32_t windowHeight = 0;

        VkInstance m_Instance = nullptr;
        VkPhysicalDevice m_PhysicalDevice = nullptr;
        VkDevice m_Device = nullptr;
        VkQueue m_GraphicsQueue = nullptr;
        VkCommandPool m_CommandPool = nullptr;
        VkRenderPass m_RenderPass = nullptr;
        VkPipelineCache m_PipelineCache = nullptr;

        Swap swapChain;

        std::vector<VkCommandBuffer> cmdBuffers;
        std::vector<VkFramebuffer> frameBuffers;
        std::vector<VkFence> waitFences;
        std::vector<VkFence> inFlightFences;

        VkSubmitInfo submitInfo = {};

        VkFormat depthFormat{};
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceMemoryProperties memoryProperties = {};
        VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        struct
        {
            VkSemaphore presentComplete;
            VkSemaphore renderComplete;
        } semaphores;

        struct
        {
            VkImage image = nullptr;
            VkDeviceMemory mem = nullptr;
            VkImageView view = nullptr;
        } depthStencil;

        std::vector<char> readFile(std::string filename);
        VkShaderModule createShaderModule(const std::vector<char> &code);

    private:
    };
};

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