#ifndef HEADERS_MVWINDOW_H_
#define HEADERS_MVWINDOW_H_

#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XInput2.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vulkan/vulkan.hpp>

#include "mvDevice.h"
#include "mvKeyboard.h"
#include "mvMouse.h"
#include "mvSwap.h"
#include "mvTimer.h"
#include "mvWindow.h"

const size_t MAX_IN_FLIGHT = 3;
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

constexpr std::array<const char *, 1> requested_validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char *, 3> requested_instance_extensions = {
    "VK_EXT_debug_utils",
    "VK_KHR_surface",
    "VK_KHR_xlib_surface",
};

constexpr std::array<const char *, 2> requested_device_extensions = {
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1",
};

namespace mv {
  class MWindow {
  public:
    // constructor
    MWindow(int w, int h, std::string title) {
      this->window_width = w;
      this->window_height = h;
      this->title = title;

      // connect to x server
      display = std::make_unique<Display *>(XOpenDisplay(""));

      if (!(*display))
        throw std::runtime_error("[+] failed to open connection to x server");

      // auto conn = XGetXCBConnection(*display);

      // xcb_screen_t *screen = nullptr;
      // xcb_screen_iterator_t scr_iter = xcb_setup_roots_iterator(xcb_get_setup(xcb_conn.get()));
      // for (; scr_iter.rem; xcb_scr_num--, xcb_screen_next(&scr_iter)) {
      //   std::cout << "screen => " << scr_iter.index << "\n";
      //   if (xcb_scr_num <= 0) {
      //     screen = scr_iter.data;
      //     break;
      //   }
      // }

      // create window
      screen = DefaultScreen(*display);

      // clang-format off

      // create window
      window = XCreateSimpleWindow(*display,
                                  RootWindow(*display, screen),
                                  10, 10,
                                  window_width, window_height,
                                  1,
                                  WhitePixel(*display, screen),
                                  BlackPixel(*display, screen));
      // clang-format on


      // remove window border
      // struct {
      //   unsigned long flags = 0;
      //   unsigned long functions = 0;
      //   unsigned long decorations = 0;
      //   long input_mode = 0;
      //   unsigned long status = 0;
      // } hints;
      // hints.flags = 2;
      // hints.decorations = 0;

      // Atom borders_atom = XInternAtom(display, "_MOTIF_WM_HINTS", 1);
      // // ensure we got the atom
      // if (!borders_atom) {
      //   throw std::runtime_error("Failed to fetch motif window manager hints atom");
      // }

      // Atom del_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
      // XSetWMProtocols(display, window, &del_window, 1);

      // // remove decorations
      // XChangeProperty(display, window, borders_atom, borders_atom, 32, PropModeReplace,
      //                 (unsigned char *)&hints, 5);

      // clang-format off
      long evt_mask = EnterWindowMask         |
                      LeaveWindowMask         |
                      FocusChangeMask         |
                      ButtonPressMask         |
                      ButtonReleaseMask       |
                      ExposureMask            |
                      KeyPressMask            |
                      KeyReleaseMask          |
                      NoExpose                |
                      SubstructureNotifyMask;
      // clang-format on

      // configure events

      // regular events
      XSelectInput(*display, window, evt_mask);

      // xinput events
      int xi_major_rt = 0; // xinput opcode
      int xi_event_rt = 0;
      int xi_error_rt = 0;
      auto q_rt =
          XQueryExtension(*display, "XInputExtension", &xi_major_rt, &xi_event_rt, &xi_error_rt);
      if (!q_rt)
        throw std::runtime_error("failed to query xinput version");

      xinput_opcode = xi_major_rt;
      std::cout << "xinput major opcode => " << +xi_major_rt << "\n";

      int major = 2;
      int minor = 0;
      auto xi_q = XIQueryVersion(*display, &major, &minor);
      if (xi_q == BadRequest)
        throw std::runtime_error("system using outdate xinput, need at least Xinput2");
      else
        std::cout << "[+] xinput is at least version 2\n";

      XIEventMask eventmask;
      unsigned char mask[3] = {0, 0, 0};
      eventmask.deviceid = XIAllMasterDevices;
      eventmask.mask_len = sizeof(mask);
      eventmask.mask = mask;
      XISetMask(mask, XI_RawMotion);
      auto select_rt = XISelectEvents(*display, DefaultRootWindow(*display), &eventmask, 1);
      if (select_rt != Success)
        throw std::runtime_error("Failed to set event masks for xinput2");

      // change window title
      XStoreName(*display, window, title.c_str());


      // Display window
      XMapWindow(*display, window);
      XFlush(*display);

      // TODO
      // Check if auto repeat disabled so that if this fails, the user can do it themselves and
      // reboot the app

      // disable synthetic key release events
      int rs = 0;
      auto detectable_result = XkbSetDetectableAutoRepeat(*display, true, &rs);

      if (rs != 1 && !detectable_result) {
        throw std::runtime_error("Could not disable auto repeat");
      }

      std::cout << "detectable repeat status => " << rs << "\n";

      /*
        Initialize kbd & mouse handler
      */
      kbd = std::make_unique<mv::keyboard>();
      mouse = std::make_unique<mv::mouse>();

      // initialize smart pointers
      swapchain = std::make_unique<mv::Swap>();
      command_buffers = std::make_unique<std::vector<vk::CommandBuffer>>();
      frame_buffers = std::make_unique<std::vector<vk::Framebuffer>>();
      in_flight_fences = std::make_unique<std::vector<vk::Fence>>();
      wait_fences = std::make_unique<std::vector<vk::Fence>>();
      semaphores = std::make_unique<struct semaphores_struct>();
      depth_stencil = std::make_unique<struct depth_stencil_struct>();
      return;
    }
    ~MWindow() {
      if (!mv_device)
        return;
      // ensure gpu not using any resources
      if (mv_device->logical_device) {
        mv_device->logical_device->waitIdle();
      }

      // swapchain related resource cleanup
      swapchain->cleanup(*instance, *mv_device);

      // cleanup command buffers
      if (command_buffers) {
        if (!command_buffers->empty()) {
          mv_device->logical_device->freeCommandBuffers(*command_pool, *command_buffers);
        }
        command_buffers.reset();
      }

      // cleanup sync objects
      if (in_flight_fences) {
        for (auto &fence : *in_flight_fences) {
          if (fence) {
            mv_device->logical_device->destroyFence(fence, nullptr);
          }
        }
        in_flight_fences.reset();
      }
      // ''
      if (semaphores) {
        if (semaphores->present_complete)
          mv_device->logical_device->destroySemaphore(semaphores->present_complete, nullptr);
        if (semaphores->render_complete)
          mv_device->logical_device->destroySemaphore(semaphores->render_complete, nullptr);
        semaphores.reset();
      }

      if (render_pass) {
        mv_device->logical_device->destroyRenderPass(*render_pass, nullptr);
        render_pass.reset();
      }

      if (frame_buffers) {
        if (!frame_buffers->empty()) {
          for (auto &buffer : *frame_buffers) {
            if (buffer) {
              mv_device->logical_device->destroyFramebuffer(buffer, nullptr);
            }
          }
          frame_buffers.reset();
        }
      }

      if (depth_stencil) {
        if (depth_stencil->image) {
          mv_device->logical_device->destroyImage(depth_stencil->image, nullptr);
        }
        if (depth_stencil->view) {
          mv_device->logical_device->destroyImageView(depth_stencil->view, nullptr);
        }
        if (depth_stencil->mem) {
          mv_device->logical_device->freeMemory(depth_stencil->mem, nullptr);
        }
        depth_stencil.reset();
      }

      if (command_pool) {
        mv_device->logical_device->destroyCommandPool(*command_pool);
        command_pool.reset();
      }

      // custom logical device interface/container cleanup
      if (mv_device) {
        mv_device.reset();
      }

      if (instance) {
        instance->destroy();
        instance.reset();
      }

      if (display && window)
        XDestroyWindow(*display, window);
      if (display)
        XCloseDisplay(*display);
      return;
    } // end destructor

    // delete copy operations
    MWindow(const MWindow &) = delete;
    MWindow &operator=(const MWindow &) = delete;
    // delete move operations
    MWindow(MWindow &&) = delete;
    MWindow &operator=(MWindow &&) = delete;

    void create_instance(void);

    // calls all other initialization functions
    void prepare(void);

    // produces mouse/keyboard handler events
    // based on xcb events
    void handle_x_event();

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
    std::unique_ptr<mv::mouse> mouse;
    std::unique_ptr<mv::keyboard> kbd;

  protected:
    std::unique_ptr<Display *> display;
    Window window;
    int screen;
    int xinput_opcode = 0;

    bool running = true;

    std::string title;
    uint32_t window_width = 0;
    uint32_t window_height = 0;

    std::unique_ptr<vk::Instance> instance;
    std::unique_ptr<vk::PhysicalDevice> physical_device;
    // contains logical device & graphics queue
    std::unique_ptr<mv::Device> mv_device;
    std::unique_ptr<vk::CommandPool> command_pool;
    std::unique_ptr<vk::RenderPass> render_pass;

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

    struct semaphores_struct {
      vk::Semaphore present_complete;
      vk::Semaphore render_complete;
    };
    std::unique_ptr<struct semaphores_struct> semaphores;

    struct depth_stencil_struct {
      vk::Image image;
      vk::DeviceMemory mem;
      vk::ImageView view;
    };
    std::unique_ptr<struct depth_stencil_struct> depth_stencil;
  }; // namespace mv
};   // end namespace mv

VKAPI_ATTR
VkBool32 VKAPI_CALL
debug_message_processor(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                        VkDebugUtilsMessageTypeFlagsEXT message_type,
                        const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

#endif