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
#include "mvTimer.h"

const size_t MAX_IN_FLIGHT = 2;
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

const std::vector<const char *> requested_validation_layers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> requested_instance_extensions = {
    "VK_EXT_debug_utils",
    "VK_KHR_surface",
    "VK_KHR_xlib_surface"};

const std::vector<const char *> requested_device_extensions = {
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1"};

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

        VkResult create_instance(void);

        //void go(void);
        void prepare(void);

        XEvent create_event(const char *eventType);
        void handle_x_event(void);

    protected:
        bool init_vulkan(void);
        void check_validation_support(void);
        void check_instance_ext(void);
        void create_command_buffers(void);
        void create_synchronization_primitives(void);
        void setup_depth_stencil(void);
        void setup_render_pass(void);
        void create_pipeline_cache(void);
        void setup_framebuffer(void);

        void destroy_command_buffers(void);
        void destroy_command_pool(void);
        void cleanup_depth_stencil(void);

    public:
        Timer timer;
        VkClearColorValue default_clear_color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        bool good_init = true;
        Keyboard kbd;
        Mouse mouse;

        mv::Device *device;

    protected: // Graphics
        Display *display;
        Window window;
        XEvent event;

        int screen;
        bool running = true;

        uint32_t window_width = 0;
        uint32_t window_height = 0;

        VkInstance m_instance = nullptr;
        VkPhysicalDevice m_physical_device = nullptr;
        VkDevice m_device = nullptr;
        VkQueue m_graphics_queue = nullptr;
        VkCommandPool m_command_pool = nullptr;
        VkRenderPass m_render_pass = nullptr;
        VkPipelineCache m_pipeline_cache = nullptr;

        Swap swapchain;

        std::vector<VkCommandBuffer> command_buffers;
        std::vector<VkFramebuffer> frame_buffers;
        std::vector<VkFence> wait_fences;
        std::vector<VkFence> in_flight_fences;

        VkSubmitInfo submit_info = {};

        VkFormat depth_format{};
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceMemoryProperties memory_properties = {};
        VkPipelineStageFlags stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        struct
        {
            VkSemaphore present_complete;
            VkSemaphore render_complete;
        } semaphores;

        struct
        {
            VkImage image = nullptr;
            VkDeviceMemory mem = nullptr;
            VkImageView view = nullptr;
        } depth_stencil;

        std::vector<char> read_file(std::string filename);
        VkShaderModule create_shader_module(const std::vector<char> &code);

    private:
    };
};

VKAPI_ATTR
VkBool32
    VKAPI_CALL
    debug_message_processor(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
        void *user_data);

#endif