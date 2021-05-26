#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "mvMisc.h"
#include "mvInput.h"
#include "mvSwap.h"

const size_t MAX_IN_FLIGHT = 3;

#define WINDOW_WIDTH 2560
#define WINDOW_HEIGHT 1440

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

// GLFW CALLBACKS -- DEFINED IN ENGINE CPP
void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods);
void mouseMotionCallback(GLFWwindow *window, double xpos, double ypos);
void mouseScrollCallback(GLFWwindow *p_GLFWwindow, double p_XOffset,
                         double p_YOffset);
void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void glfwErrCallback(int error, const char *desc);

class Window
{
    friend struct Collection;
    friend class Image;
    friend class Swap;

  public:
    // delete copy operations
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    // delete move operations
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    Window(int p_WindowWidth, int p_WindowHeight, std::string p_WindowTitle);
    ~Window();

    // Create Vulkan instance
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
    vk::ShaderModule createShaderModule(
        const std::vector<char> &p_ShaderCharBuffer);

  protected:
    void createLogicalDevice(void);

    /*
      Helper
    */
    vk::CommandPool createCommandPool(
        uint32_t p_QueueIndex,
        vk::CommandPoolCreateFlags p_CreateFlags =
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    uint32_t getMemoryType(uint32_t p_TypeBits,
                           vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                           vk::Bool32 *p_IsMemoryTypeFound = nullptr) const;

    // Returns an index to queue families which supports provided queue flag bit
    uint32_t getQueueFamilyIndex(vk::QueueFlagBits p_QueueFlagBit) const;

    // Gets depth format supported by physical device
    vk::Format getSupportedDepthFormat(void) const;

  public:
    bool good_init = true;
    vk::ClearColorValue defaultClearColor =
        std::array<float, 4>({{0.0f, 0.0f, 0.0f, 1.0f}});

    // handlers
    Mouse mouse;
    Keyboard keyboard;

  public:
    // Instance/Device extension functions -- must be loaded
    PFN_vkCmdSetPrimitiveTopologyEXT pfn_vkCmdSetPrimitiveTopology = nullptr;

    vk::DebugUtilsMessengerEXT vulkanHandleDebugCallback;
    PFN_vkCreateDebugUtilsMessengerEXT pfn_vkCreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT pfn_vkDestroyDebugUtilsMessengerEXT = nullptr;

    // Glfw
    GLFWwindow *window = nullptr;
    std::string title;
    uint32_t windowWidth = 0;
    uint32_t windowHeight = 0;

    // Final list of extensions/layers
    std::vector<std::string> instanceExtensions;

    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device logicalDevice;
    vk::Queue graphicsQueue;
    vk::CommandPool commandPool;
    std::unordered_map<RenderPassType, vk::RenderPass> renderPasses;

    Swap swapchain;

    std::vector<vk::CommandBuffer> commandBuffers;
    std::vector<vk::Framebuffer> coreFramebuffers; // core engine frame buffers
    std::vector<vk::Framebuffer> guiFramebuffers;  // ImGui frame buffers
    std::vector<vk::Fence> inFlightFences;
    std::vector<vk::Fence> waitFences; // not allocated

    struct SemaphoresStruct
    {
        vk::Semaphore presentComplete;
        vk::Semaphore renderComplete;
    } semaphores;

    struct DepthStencilStruct
    {
        vk::Image image;
        vk::DeviceMemory mem;
        vk::ImageView view;
    } depthStencil;

    // clang-format off
    // info structures
    struct
    {
        uint32_t graphics = UINT32_MAX;
        uint32_t compute = UINT32_MAX;
        uint32_t transfer = UINT32_MAX;
    } queueIdx;
    std::vector<std::string>                          requestedLogicalDeviceExtensions;
    vk::PhysicalDeviceFeatures                        physicalFeatures;
    vk::PhysicalDeviceFeatures2                       physicalFeatures2;
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedFeatures;
    vk::PhysicalDeviceProperties                      physicalProperties;
    vk::PhysicalDeviceMemoryProperties                physicalMemoryProperties;
    std::vector<vk::ExtensionProperties>              physicalDeviceExtensions;
    std::vector<vk::QueueFamilyProperties>            queueFamilyProperties;
    // clang-format on
}; // end window

// Instance creation/destruction callback
VKAPI_ATTR
VkBool32 VKAPI_CALL debug_message_processor(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

// Persistent vulkan debug callback
VKAPI_ATTR
VkBool32 VKAPI_CALL generalDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                         VkDebugUtilsMessageTypeFlagsEXT messageType,
                                         const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void* userData);