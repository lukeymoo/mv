#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "mvInput.h"
#include "mvMisc.h"
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

class Window
{
  friend struct Collection;
  friend class Image;
  friend class Swap;

public:
  // delete copy operations
  Window (const Window &) = delete;
  Window &operator= (const Window &) = delete;

  // delete move operations
  Window (Window &&) = delete;
  Window &operator= (Window &&) = delete;

  Window (int p_WindowWidth, int p_WindowHeight, std::string p_WindowTitle);
  ~Window ();

public:
  bool good_init = true;

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

  std::unique_ptr<vk::Instance> instance;
  std::unique_ptr<vk::PhysicalDevice> physicalDevice;
  std::unique_ptr<vk::Device> logicalDevice;
  std::unique_ptr<Swap> swapchain;

  // clang-format off
    // info structures
    QueueIndexInfo queueIdx;
    std::vector<std::string>                          requestedLogicalDeviceExtensions;
    vk::PhysicalDeviceFeatures                        physicalFeatures;
    vk::PhysicalDeviceFeatures2                       physicalFeatures2;
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedFeatures;
    vk::PhysicalDeviceProperties                      physicalProperties;
    vk::PhysicalDeviceMemoryProperties                physicalMemoryProperties;
    std::vector<vk::ExtensionProperties>              physicalDeviceExtensions;
    std::vector<vk::QueueFamilyProperties>            queueFamilyProperties;
  // clang-format on

public:
  // Create Vulkan instance
  void createInstance (void);

  // calls all other initialization functions
  void prepare (void);

protected:
  void initVulkan (void);

  void checkValidationSupport (void);

  void checkInstanceExt (void);

  void createLogicalDevice (vk::QueueFlags p_RequestedQueueFlags);
}; // end window

// Instance creation/destruction callback
VKAPI_ATTR
VkBool32 VKAPI_CALL
initialDebugProcessor (VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                       VkDebugUtilsMessageTypeFlagsEXT message_type,
                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                       void *user_data);

// Persistent vulkan debug callback
VKAPI_ATTR
VkBool32 VKAPI_CALL generalDebugCallback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                          VkDebugUtilsMessageTypeFlagsEXT messageType,
                                          const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                                          void *userData);