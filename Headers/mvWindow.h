#ifndef HEADERS_MVWINDOW_H_
#define HEADERS_MVWINDOW_H_

// #include <X11/Xlib.h>
// #include <X11/XKBlib.h>
// #include <X11/Xutil.h>
// #include <X11/XF86keysym.h>
// #include <X11/extensions/XInput2.h>
// #include <X11/Xatom.h>
// #include <X11/cursorfont.h>
// #include <X11/extensions/Xrandr.h>

// conversion to xcb
#include <xcb/xcb.h>

#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string.h>
#include <memory>

#include "mvWindow.h"
#include "mvKeyboard.h"
#include "mvMouse.h"
#include "mvDevice.h"
#include "mvSwap.h"
#include "mvInit.h"
#include "mvTimer.h"

const size_t MAX_IN_FLIGHT = 3;
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

constexpr std::vector<std::string> requested_validation_layers = {
    "VK_LAYER_KHRONOS_validation"};

constexpr std::vector<std::string> requested_instance_extensions = {
    "VK_EXT_debug_utils",
    "VK_KHR_surface",
    "VK_KHR_xcb_surface"};

constexpr std::vector<std::string> requested_device_extensions = {
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1"};

namespace mv
{
    class MWindow
    {
    public:
        // constructor
        MWindow(int w, int h, std::string title)
        {
            this->window_width = w;
            this->window_height = h;
            this->title = title;
            // connect to x server
            int xcb_scr_num = 0;

            xcb_conn = std::make_shared<xcb_connection_t>(xcb_connect(0, &xcb_scr_num));
            // validate x server connection
            if (!xcb_conn)
                throw std::runtime_error("xcb failed to open connection to x server");

            // get screen
            xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xcb_conn.get())).data;
            if (!screen)
                throw std::runtime_error("xcb failed to get screen");

            // generate xid for window
            xcb_win = std::make_shared<xcb_window_t>(xcb_generate_id(xcb_conn.get()));
            // validate xid
            if (!(*xcb_win))
                throw std::runtime_error("xcb failed to generate xid for window");

            // create window
            xcb_create_window(xcb_conn.get(),
                              XCB_COPY_FROM_PARENT,
                              *xcb_win,
                              screen->root,
                              0, 0,
                              window_width, window_height,
                              1,
                              XCB_WINDOW_CLASS_INPUT_OUTPUT,
                              screen->root_visual,
                              0, nullptr);

            // xcb_screen_t *screen = nullptr;
            // xcb_screen_iterator_t scr_iter = xcb_setup_roots_iterator(xcb_get_setup(xcb_conn.get()));
            // for (; scr_iter.rem; xcb_scr_num--, xcb_screen_next(&scr_iter))
            // {
            //     if (xcb_scr_num <= 0)
            //     {
            //         screen = scr_iter.data;
            //         break;
            //     }
            // }

            // initialize keyboard handler
            kbd = std::make_unique<mv::keyboard>(std::weak_ptr<xcb_connection_t>(xcb_conn));
            // initialize mouse handler
            mouse = std::make_unique<mv::mouse>(std::weak_ptr<xcb_connection_t>(xcb_conn),
                                                std::weak_ptr<xcb_window_t>(xcb_win));

            // show window
            xcb_map_window(xcb_conn.get(), *xcb_win);
            xcb_flush(xcb_conn.get());
            return;
        }
        ~MWindow()
        {
            // ensure gpu not using any resources
            if (logical_device)
            {
                logical_device->waitIdle();
            }

            // swapchain related resource cleanup
            swapchain->cleanup();

            // cleanup command buffers
            if (command_buffers)
            {
                if (!command_buffers->empty())
                {
                    logical_device->freeCommandBuffers(*command_pool, *command_buffers);
                }
                command_buffers.reset();
            }

            // cleanup sync objects
            if (in_flight_fences)
            {
                for (auto &fence : *in_flight_fences)
                {
                    if (fence)
                    {
                        logical_device->destroyFence(fence, nullptr);
                    }
                }
                in_flight_fences.reset();
            }
            // ''
            if (semaphores)
            {
                if (semaphores->present_complete)
                    logical_device->destroySemaphore(semaphores->present_complete, nullptr);
                if (semaphores->render_complete)
                    logical_device->destroySemaphore(semaphores->render_complete, nullptr);
                semaphores.reset();
            }

            if (render_pass)
            {
                logical_device->destroyRenderPass(*render_pass, nullptr);
                render_pass.reset();
            }

            if (frame_buffers)
            {
                if (!frame_buffers->empty())
                {
                    for (auto &buffer : *frame_buffers)
                    {
                        if (buffer)
                        {
                            logical_device->destroyFramebuffer(buffer, nullptr);
                        }
                    }
                    frame_buffers.reset();
                }
            }

            if (depth_stencil)
            {
                if (depth_stencil->image)
                {
                    logical_device->destroyImage(depth_stencil->image, nullptr);
                }
                if (depth_stencil->view)
                {
                    logical_device->destroyImageView(depth_stencil->view, nullptr);
                }
                if (depth_stencil->mem)
                {
                    logical_device->freeMemory(depth_stencil->mem, nullptr);
                }
                depth_stencil.reset();
            }

            destroy_command_pool();

            // custom logical device interface/container cleanup
            if (mv_device)
            {
                mv_device.reset();
            }

            if (instance)
            {
                instance->destroy();
                instance.reset();
            }

            if (xcb_win && xcb_conn)
                xcb_destroy_window(xcb_conn.get(), *xcb_win);
            if (xcb_conn)
                xcb_disconnect(xcb_conn.get());
            return;
        } // end destructor

        // delete copy operations
        MWindow &operator=(const MWindow &) = delete;
        MWindow(const MWindow &) = delete;
        // delete move operations
        MWindow &operator=(MWindow &&) = delete;
        MWindow(MWindow &&) = delete;

        void create_instance(void);

        // calls all other initialization functions
        void prepare(void);

        // produces mouse/keyboard handler events
        // based on xcb events
        void handle_x_event(void);

    protected:
        void init_vulkan(void);
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
        Timer fps;
        bool good_init = true;
        vk::ClearColorValue default_clear_color = std::array<float, 4>({{0.0f, 0.0f, 0.0f, 1.0f}});

        // handlers
        // logical device
        std::unique_ptr<mv::Device> mv_device;
        std::unique_ptr<mv::mouse> mouse;
        std::unique_ptr<mv::keyboard> kbd;

    protected:
        std::shared_ptr<xcb_connection_t> xcb_conn;
        std::shared_ptr<xcb_window_t> xcb_win;

        bool running = true;

        std::string title;
        uint32_t window_width = 0;
        uint32_t window_height = 0;

        std::shared_ptr<vk::Instance> instance;
        std::shared_ptr<vk::PhysicalDevice> physical_device;
        // contains logical device & graphics queue
        std::shared_ptr<mv::Device> mv_device;
        std::shared_ptr<vk::CommandPool> command_pool;
        std::shared_ptr<vk::RenderPass> render_pass;

        std::unique_ptr<mv::Swap> swapchain;

        std::unique_ptr<std::vector<vk::CommandBuffer>> command_buffers;
        std::unique_ptr<std::vector<vk::Framebuffer>> frame_buffers;
        std::unique_ptr<std::vector<vk::Fence>> in_flight_fences;
        std::unique_ptr<std::vector<vk::Fence>> wait_fences; // not allocated

        vk::Format depth_format;
        vk::PhysicalDeviceFeatures physical_features;
        vk::PhysicalDeviceProperties physical_properties;
        vk::PhysicalDeviceMemoryProperties physical_memory_properties;
        vk::PipelineStageFlags stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;

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

        std::vector<char> read_file(std::string filename);
        VkShaderModule create_shader_module(const std::vector<char> &code);
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