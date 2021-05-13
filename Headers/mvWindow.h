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

constexpr std::array<const char *, 1> requestedValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char *, 1> requestedInstanceExtensions = {
    "VK_EXT_debug_utils",
};

constexpr std::array<const char *, 3> requestedDeviceExtensions = {
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1",
    "VK_EXT_extended_dynamic_state",
};

namespace mv
{
    // GLFW CALLBACKS -- DEFINED IN ENGINE CPP
    void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    void mouseMotionCallback(GLFWwindow *window, double xpos, double ypos);
    void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    void glfwErrCallback(int error, const char *desc);

    class Window
    {
      public:
        // delete copy operations
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        // delete move operations
        Window(Window &&) = delete;
        Window &operator=(Window &&) = delete;

        Window(int p_WindowWidth, int p_WindowHeight, std::string p_WindowTitle);
        ~Window();

        void createInstance(void);

        // calls all other initialization functions
        void prepare(void);

      protected:
        void initVulkan(void);
        void checkValidationSupport(void);
        void checkInstanceExt(void);
        void createCommandBuffers(void);
        void createSynchronizationPrimitives(void);
        void setupDepthStencil(void);
        void setupRenderPass(void);
        // TODO re implement
        // void create_pipeline_cache(void);
        void setupFramebuffer(void);

        std::vector<char> readFile(std::string p_Filename);
        vk::ShaderModule createShaderModule(const std::vector<char> &p_ShaderCharBuffer);

      public:
        Timer timer;
        Timer fps;
        bool good_init = true;
        vk::ClearColorValue defaultClearColor = std::array<float, 4>({{0.0f, 0.0f, 0.0f, 1.0f}});

        // handlers
        Mouse mouse;
        Keyboard keyboard;
        std::unique_ptr<mv::Device> mvDevice;

      protected:
        // Instance/Device extension functions -- must be loaded
        PFN_vkCmdSetPrimitiveTopologyEXT pfn_vkCmdSetPrimitiveTopology;

        // Glfw
        GLFWwindow *window = nullptr;
        std::string title;
        uint32_t windowWidth = 0;
        uint32_t windowHeight = 0;

        // Final list of extensions/layers
        std::vector<std::string> instanceExtensions;

        // owns
        std::unique_ptr<vk::Instance> instance;
        std::unique_ptr<vk::PhysicalDevice> physicalDevice;
        std::unique_ptr<vk::CommandPool> commandPool;
        std::unique_ptr<vk::RenderPass> renderPass;

        std::unique_ptr<mv::Swap> swapchain;

        std::unique_ptr<std::vector<vk::CommandBuffer>> commandBuffers;
        std::unique_ptr<std::vector<vk::Framebuffer>> coreFramebuffers; // core engine frame buffers
        std::unique_ptr<std::vector<vk::Framebuffer>> guiFramebuffers;  // ImGui frame buffers
        std::unique_ptr<std::vector<vk::Fence>> inFlightFences;
        std::unique_ptr<std::vector<vk::Fence>> waitFences; // not allocated

        struct SemaphoresStruct
        {
            vk::Semaphore presentComplete;
            vk::Semaphore renderComplete;
        };
        std::unique_ptr<struct SemaphoresStruct> semaphores;

        struct DepthStencilStruct
        {
            vk::Image image;
            vk::DeviceMemory mem;
            vk::ImageView view;
        };
        std::unique_ptr<struct DepthStencilStruct> depthStencil;

        // clang-format off
        // info structures
        vk::Format                          depthFormat;
        vk::PipelineStageFlags              stageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::PhysicalDeviceFeatures          physicalFeatures;
        vk::PhysicalDeviceProperties        physicalProperties;
        vk::PhysicalDeviceMemoryProperties  physicalMemoryProperties;
        // clang-format on
    }; // end window

}; // end namespace mv

VKAPI_ATTR
VkBool32 VKAPI_CALL debug_message_processor(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                            VkDebugUtilsMessageTypeFlagsEXT message_type,
                                            const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

#endif