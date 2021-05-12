#ifndef HEADERS_MVWINDOW_H_
#define HEADERS_MVWINDOW_H_

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "mvInput.h"
#include "mvDevice.h"
#include "mvSwap.h"
#include "mvTimer.h"
#include "mvWindow.h"

const size_t MAX_IN_FLIGHT = 3;

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

constexpr std::array<const char *, 1> requested_validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char *, 1> req_instance_extensions = {
    "VK_EXT_debug_utils",
};

constexpr std::array<const char *, 3> requested_device_extensions = {
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1",
    "VK_EXT_extended_dynamic_state",
};

namespace mv
{
    // GLFW CALLBACKS -- DEFINED IN ENGINE CPP
    void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
    void mouse_motion_callback(GLFWwindow *window, double xpos, double ypos);
    void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
    void glfw_err_callback(int error, const char *desc);

    class MWindow
    {
      public:
        // delete copy operations
        MWindow(const MWindow &) = delete;
        MWindow &operator=(const MWindow &) = delete;
        // delete move operations
        MWindow(MWindow &&) = delete;
        MWindow &operator=(MWindow &&) = delete;

        MWindow(int w, int h, std::string title);
        ~MWindow();

        void create_instance(void);

        // calls all other initialization functions
        void prepare(void);

      protected:
        void init_vulkan(void);
        void check_validation_support(void);
        void check_instance_ext(void);
        void create_command_buffers(void);
        void create_synchronization_primitives(void);
        void setup_depth_stencil(void);
        void setup_render_pass(void);
        // TODO re implement
        // void create_pipeline_cache(void);
        void setup_framebuffer(void);

        std::vector<char> read_file(std::string filename);
        vk::ShaderModule create_shader_module(const std::vector<char> &code);

      public:
        Timer timer;
        Timer fps;
        bool good_init = true;
        vk::ClearColorValue default_clear_color = std::array<float, 4>({{0.0f, 0.0f, 0.0f, 1.0f}});

        // handlers
        Mouse mouse;
        Keyboard keyboard;
        std::unique_ptr<mv::Device> mv_device;

      protected:
        // Instance/Device extension functions -- must be loaded
        PFN_vkCmdSetPrimitiveTopologyEXT pfn_vkCmdSetPrimitiveTopology;

        // Glfw
        GLFWwindow *window = nullptr;
        std::string title;
        uint32_t window_width = 0;
        uint32_t window_height = 0;

        // final list of requested instance extensions
        std::vector<std::string> f_req;

        // owns
        std::unique_ptr<vk::Instance> instance;
        std::unique_ptr<vk::PhysicalDevice> physical_device;
        std::unique_ptr<vk::CommandPool> command_pool;
        std::unique_ptr<vk::RenderPass> render_pass;

        std::unique_ptr<mv::Swap> swapchain;

        std::unique_ptr<std::vector<vk::CommandBuffer>> command_buffers;
        std::unique_ptr<std::vector<vk::Framebuffer>> frame_buffers;
        std::unique_ptr<std::vector<vk::Fence>> in_flight_fences;
        std::unique_ptr<std::vector<vk::Fence>> wait_fences; // not allocated

        struct semaphores_struct
        {
            vk::Semaphore present_complete;
            vk::Semaphore render_complete;
        };
        std::unique_ptr<struct semaphores_struct> semaphores;

        struct depth_stencil_struct
        {
            vk::Image image;
            vk::DeviceMemory mem;
            vk::ImageView view;
        };
        std::unique_ptr<struct depth_stencil_struct> depth_stencil;

        // info structures
        vk::Format depth_format;
        vk::PhysicalDeviceFeatures physical_features;
        vk::PhysicalDeviceProperties physical_properties;
        vk::PhysicalDeviceMemoryProperties physical_memory_properties;
        vk::PipelineStageFlags stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    }; // end window

}; // end namespace mv

VKAPI_ATTR
VkBool32 VKAPI_CALL debug_message_processor(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                            VkDebugUtilsMessageTypeFlagsEXT message_type,
                                            const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

#endif