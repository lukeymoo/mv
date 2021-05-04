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

const size_t MAX_IN_FLIGHT = 3;
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

static bool get_key(mv::Keyboard::Key key)
{
    // sanity check
    KeySym key_sym;
    switch(key) {
        case mv::Keyboard::Key::l_shift:    key = 0;    break;
        case mv::Keyboard::Key::r_shift:    key = 0;    break;
        case mv::Keyboard::Key::l_ctrl:     key = 0;    break;
        case mv::Keyboard::Key::r_ctrl:     key = 0;    break;
        case mv::Keyboard::Key::a_alt:      key = 0;    break;
        case mv::Keyboard::Key::r_alt:      key = 0;    break;
        case mv::Keyboard::Key::l_super:    key = 0;    break;
        case mv::Keyboard::Key::r_super:    key = 0;    break;
        case mv::Keyboard::Key::menu:       key = 0;    break;
        case mv::Keyboard::Key::escape:     key = 0;    break;
        case mv::Keyboard::Key::semicolon:  key = 0;    break;
        case mv::Keyboard::Key::slash:      key = 0;    break;
        case mv::Keyboard::Key::equal:      key = 0;    break;
        case mv::Keyboard::Key::minus:      key = 0;    break;
        case mv::Keyboard::Key::l_bracket:  key = 0;    break;
        case mv::Keyboard::Key::r_bracket:  key = 0;    break;
        case mv::Keyboard::Key::comma:      key = 0;    break;
        case mv::Keyboard::Key::period:     key = 0;    break;
        case mv::Keyboard::Key::apostrophe: key = 0;    break;
        case mv::Keyboard::Key::backslash:  key = 0;    break;
        case mv::Keyboard::Key::grave:      key = 0;    break;
        case mv::Keyboard::Key::space:      key = 0;    break;
        case mv::Keyboard::Key::sk_return:  key = 0;    break;
        case mv::Keyboard::Key::backspace:  key = 0;    break;
        case mv::Keyboard::Key::tab:        key = 0;    break;
        case mv::Keyboard::Key::prior:      key = 0;    break;
        case mv::Keyboard::Key::next:       key = 0;    break;
        case mv::Keyboard::Key::end:        key = 0;    break;
        case mv::Keyboard::Key::home:       key = 0;    break;
        case mv::Keyboard::Key::insert:     key = 0;    break;
        case mv::Keyboard::Key::sk_delete:  key = 0;    break;
        case mv::Keyboard::Key::pause:      key = 0;    break;
        case mv::Keyboard::Key::f1:         key = 0;    break;
        case mv::Keyboard::Key::f2:         key = 0;    break;
        case mv::Keyboard::Key::f3:         key = 0;    break;
        case mv::Keyboard::Key::f4:         key = 0;    break;
        case mv::Keyboard::Key::f5:         key = 0;    break;
        case mv::Keyboard::Key::f6:         key = 0;    break;
        case mv::Keyboard::Key::f7:         key = 0;    break;
        case mv::Keyboard::Key::f8:         key = 0;    break;
        case mv::Keyboard::Key::f9:         key = 0;    break;
        case mv::Keyboard::Key::f10:        key = 0;    break;
        case mv::Keyboard::Key::f11:        key = 0;    break;
        case mv::Keyboard::Key::f12:        key = 0;    break;
        case mv::Keyboard::Key::f13:        key = 0;    break;
        case mv::Keyboard::Key::f14:        key = 0;    break;
        case mv::Keyboard::Key::f15:        key = 0;    break;
        case mv::Keyboard::Key::left:       key = 0;    break;
        case mv::Keyboard::Key::right:      key = 0;    break;
        case mv::Keyboard::Key::up:         key = 0;    break;
        case mv::Keyboard::Key::down:       key = 0;    break;
        case mv::Keyboard::Key::a:          key = 0;    break;
        case mv::Keyboard::Key::b:          key = 0;    break;
        case mv::Keyboard::Key::c:          key = 0;    break;
        case mv::Keyboard::Key::d:          key = 0;    break;
        case mv::Keyboard::Key::e:          key = 0;    break;
        case mv::Keyboard::Key::f:          key = 0;    break;
        case mv::Keyboard::Key::g:          key = 0;    break;
        case mv::Keyboard::Key::h:          key = 0;    break;
        case mv::Keyboard::Key::i:          key = 0;    break;
        case mv::Keyboard::Key::j:          key = 0;    break;
        case mv::Keyboard::Key::k:          key = 0;    break;
        case mv::Keyboard::Key::l:          key = 0;    break;
        case mv::Keyboard::Key::m:          key = 0;    break;
        case mv::Keyboard::Key::n:          key = 0;    break;
        case mv::Keyboard::Key::o:          key = 0;    break;
        case mv::Keyboard::Key::p:          key = 0;    break;
        case mv::Keyboard::Key::q:          key = 0;    break;
        case mv::Keyboard::Key::r:          key = 0;    break;
        case mv::Keyboard::Key::s:          key = 0;    break;
        case mv::Keyboard::Key::t:          key = 0;    break;
        case mv::Keyboard::Key::u:          key = 0;    break;
        case mv::Keyboard::Key::v:          key = 0;    break;
        case mv::Keyboard::Key::w:          key = 0;    break;
        case mv::Keyboard::Key::x:          key = 0;    break;
        case mv::Keyboard::Key::y:          key = 0;    break;
        case mv::Keyboard::Key::z:          key = 0;    break;
        case mv::Keyboard::Key::sk_0:       key = 0;    break;
        case mv::Keyboard::Key::sk_1:       key = 0;    break;
        case mv::Keyboard::Key::sk_2:       key = 0;    break;
        case mv::Keyboard::Key::sk_3:       key = 0;    break;
        case mv::Keyboard::Key::sk_4:       key = 0;    break;
        case mv::Keyboard::Key::sk_5:       key = 0;    break;
        case mv::Keyboard::Key::sk_6:       key = 0;    break;
        case mv::Keyboard::Key::sk_7:       key = 0;    break;
        case mv::Keyboard::Key::sk_8:       key = 0;    break;
        case mv::Keyboard::Key::sk_9:       key = 0;    break;
        default:    key = 0;    break;
    }
    KeyCode key_code = XKeysymToKeycode(display, key);
    return;
}

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
        Timer fps;
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