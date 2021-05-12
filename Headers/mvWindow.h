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

constexpr std::array<const char *, 1> req_instance_extensions = {
    "VK_EXT_debug_utils",
};

constexpr std::array<const char *, 3> requested_device_extensions = {
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1",
    "VK_EXT_extended_dynamic_state",
};

namespace mv {
  // GLFW CALLBACKS -- DEFINED IN ENGINE CPP
  void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
  void mouse_motion_callback(GLFWwindow *window, double xpos, double ypos);
  void glfw_err_callback(int error, const char *desc);

  class MWindow {
  public:
    // constructor
    MWindow(int w, int h, std::string title) {
      this->window_width = w;
      this->window_height = h;
      this->title = title;

      glfwInit();

      uint32_t count = 0;
      const char **extensions = glfwGetRequiredInstanceExtensions(&count);
      if (!extensions)
        throw std::runtime_error("Failed to get required instance extensions");


      std::vector<std::string> req_ext;
      for (const auto &req : req_instance_extensions) {
        req_ext.push_back(req);
      }
      std::vector<std::string> glfw_requested;
      for (uint32_t i = 0; i < count; i++) {
        glfw_requested.push_back(extensions[i]);
      }

      // iterate glfw requested
      for (const auto &glfw_req : glfw_requested) {
        bool found = false;

        // iterate already requested list
        for (const auto &req : req_ext) {
          if (glfw_req == req) {
            found = true;
          }
        }

        // if not found, add to final list
        if (!found)
          f_req.push_back(glfw_req);
      }
      // concat existing requests with glfw requests into final list
      f_req.insert(f_req.end(), req_ext.begin(), req_ext.end());

      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
      window = glfwCreateWindow(window_width, window_height, title.c_str(), nullptr, nullptr);
      if (!window)
        throw std::runtime_error("Failed to create window");

      // set keyboard callback
      glfwSetKeyCallback(window, mv::key_callback);

      // set mouse motion callback
      glfwSetCursorPosCallback(window, mv::mouse_motion_callback);

      auto is_raw_supp = glfwRawMouseMotionSupported();

      if (!is_raw_supp) {
        throw std::runtime_error("Raw mouse motion not supported");
      } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, true);
      }

      /*
        Initialize kbd & mouse handler
      */
      kbd = std::make_unique<mv::keyboard>(window);
      mouse = std::make_unique<mv::mouse>(window);

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

      if (window)
        glfwDestroyWindow(window);
      glfwTerminate();
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

    std::unique_ptr<mv::Device> mv_device;

  protected:
    /*
      Instance/Device extension functions
    */
    PFN_vkCmdSetPrimitiveTopologyEXT pfn_vkCmdSetPrimitiveTopology;


    // Glfw
    GLFWwindow *window = nullptr;

    // final list of requested instance extensions
    std::vector<std::string> f_req;

    bool running = true;

    std::string title;
    uint32_t window_width = 0;
    uint32_t window_height = 0;

    std::unique_ptr<vk::Instance> instance;
    std::unique_ptr<vk::PhysicalDevice> physical_device;
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

  public:
  }; // end window

}; // end namespace mv

VKAPI_ATTR
VkBool32 VKAPI_CALL
debug_message_processor(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                        VkDebugUtilsMessageTypeFlagsEXT message_type,
                        const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

#endif