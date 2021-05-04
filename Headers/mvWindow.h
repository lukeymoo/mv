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
    if(key < 0 || key > mv::Keyboard::key_count)
    {
        return false;
    }
    KeySym key_sym;
    switch(key)
    {
        case mv::Keyboard::Key::l_shift:    key_sym = XK_Shift_L;       break;
        case mv::Keyboard::Key::r_shift:    key_sym = XK_Shift_R;       break;
        case mv::Keyboard::Key::l_ctrl:     key_sym = XK_Control_L;     break;
        case mv::Keyboard::Key::r_ctrl:     key_sym = XK_Control_R;     break;
        case mv::Keyboard::Key::l_alt:      key_sym = XK_Alt_L;         break;
        case mv::Keyboard::Key::r_alt:      key_sym = XK_Alt_R;         break;
        case mv::Keyboard::Key::l_super:    key_sym = XK_Super_L;       break;
        case mv::Keyboard::Key::r_super:    key_sym = XK_Super_R;       break;
        case mv::Keyboard::Key::menu:       key_sym = XK_Menu;          break;
        case mv::Keyboard::Key::escape:     key_sym = XK_Escape;        break;
        case mv::Keyboard::Key::semicolon:  key_sym = XK_semicolon;     break;
        case mv::Keyboard::Key::slash:      key_sym = XK_slash;         break;
        case mv::Keyboard::Key::equal:      key_sym = XK_equal;         break;
        case mv::Keyboard::Key::minus:      key_sym = XK_minus;         break;
        case mv::Keyboard::Key::l_bracket:  key_sym = XK_bracketleft;   break;
        case mv::Keyboard::Key::r_bracket:  key_sym = XK_bracketright;  break;
        case mv::Keyboard::Key::comma:      key_sym = XK_comma;         break;
        case mv::Keyboard::Key::period:     key_sym = XK_period;        break;
        case mv::Keyboard::Key::apostrophe: key_sym = XK_apostrophe;    break;
        case mv::Keyboard::Key::backslash:  key_sym = XK_backslash;     break;
        case mv::Keyboard::Key::grave:      key_sym = XK_grave;         break;
        case mv::Keyboard::Key::space:      key_sym = XK_space;         break;
        case mv::Keyboard::Key::sk_return:  key_sym = XK_Return;        break;
        case mv::Keyboard::Key::backspace:  key_sym = XK_BackSpace;     break;
        case mv::Keyboard::Key::tab:        key_sym = XK_Tab;           break;
        case mv::Keyboard::Key::prior:      key_sym = XK_Prior;         break;
        case mv::Keyboard::Key::next:       key_sym = XK_Next;          break;
        case mv::Keyboard::Key::end:        key_sym = XK_End;           break;
        case mv::Keyboard::Key::home:       key_sym = XK_Home;          break;
        case mv::Keyboard::Key::insert:     key_sym = XK_Insert;        break;
        case mv::Keyboard::Key::sk_delete:  key_sym = XK_Delete;        break;
        case mv::Keyboard::Key::pause:      key_sym = XK_Pause;         break;
        case mv::Keyboard::Key::f1:         key_sym = XK_F1;            break;
        case mv::Keyboard::Key::f2:         key_sym = XK_F2;            break;
        case mv::Keyboard::Key::f3:         key_sym = XK_F3;            break;
        case mv::Keyboard::Key::f4:         key_sym = XK_F4;            break;
        case mv::Keyboard::Key::f5:         key_sym = XK_F5;            break;
        case mv::Keyboard::Key::f6:         key_sym = XK_F6;            break;
        case mv::Keyboard::Key::f7:         key_sym = XK_F7;            break;
        case mv::Keyboard::Key::f8:         key_sym = XK_F8;            break;
        case mv::Keyboard::Key::f9:         key_sym = XK_F9;            break;
        case mv::Keyboard::Key::f10:        key_sym = XK_F10;           break;
        case mv::Keyboard::Key::f11:        key_sym = XK_F11;           break;
        case mv::Keyboard::Key::f12:        key_sym = XK_F12;           break;
        case mv::Keyboard::Key::f13:        key_sym = XK_F13;           break;
        case mv::Keyboard::Key::f14:        key_sym = XK_F14;           break;
        case mv::Keyboard::Key::f15:        key_sym = XK_F15;           break;
        case mv::Keyboard::Key::left:       key_sym = XK_Left;          break;
        case mv::Keyboard::Key::right:      key_sym = XK_Right;         break;
        case mv::Keyboard::Key::up:         key_sym = XK_Up;            break;
        case mv::Keyboard::Key::down:       key_sym = XK_Down;          break;
        case mv::Keyboard::Key::a:          key_sym = XK_a;             break;
        case mv::Keyboard::Key::b:          key_sym = XK_b;             break;
        case mv::Keyboard::Key::c:          key_sym = XK_c;             break;
        case mv::Keyboard::Key::d:          key_sym = XK_d;             break;
        case mv::Keyboard::Key::e:          key_sym = XK_e;             break;
        case mv::Keyboard::Key::f:          key_sym = XK_f;             break;
        case mv::Keyboard::Key::g:          key_sym = XK_g;             break;
        case mv::Keyboard::Key::h:          key_sym = XK_h;             break;
        case mv::Keyboard::Key::i:          key_sym = XK_i;             break;
        case mv::Keyboard::Key::j:          key_sym = XK_j;             break;
        case mv::Keyboard::Key::k:          key_sym = XK_k;             break;
        case mv::Keyboard::Key::l:          key_sym = XK_l;             break;
        case mv::Keyboard::Key::m:          key_sym = XK_m;             break;
        case mv::Keyboard::Key::n:          key_sym = XK_n;             break;
        case mv::Keyboard::Key::o:          key_sym = XK_o;             break;
        case mv::Keyboard::Key::p:          key_sym = XK_p;             break;
        case mv::Keyboard::Key::q:          key_sym = XK_q;             break;
        case mv::Keyboard::Key::r:          key_sym = XK_r;             break;
        case mv::Keyboard::Key::s:          key_sym = XK_s;             break;
        case mv::Keyboard::Key::t:          key_sym = XK_t;             break;
        case mv::Keyboard::Key::u:          key_sym = XK_u;             break;
        case mv::Keyboard::Key::v:          key_sym = XK_v;             break;
        case mv::Keyboard::Key::w:          key_sym = XK_w;             break;
        case mv::Keyboard::Key::x:          key_sym = XK_x;             break;
        case mv::Keyboard::Key::y:          key_sym = XK_y;             break;
        case mv::Keyboard::Key::z:          key_sym = XK_z;             break;
        case mv::Keyboard::Key::sk_0:       key_sym = XK_0;             break;
        case mv::Keyboard::Key::sk_1:       key_sym = XK_1;             break;
        case mv::Keyboard::Key::sk_2:       key_sym = XK_2;             break;
        case mv::Keyboard::Key::sk_3:       key_sym = XK_3;             break;
        case mv::Keyboard::Key::sk_4:       key_sym = XK_4;             break;
        case mv::Keyboard::Key::sk_5:       key_sym = XK_5;             break;
        case mv::Keyboard::Key::sk_6:       key_sym = XK_6;             break;
        case mv::Keyboard::Key::sk_7:       key_sym = XK_7;             break;
        case mv::Keyboard::Key::sk_8:       key_sym = XK_8;             break;
        case mv::Keyboard::Key::sk_9:       key_sym = XK_9;             break;
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