#pragma once

#include <iostream>
#include <queue>
#include <regex>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

enum class LogHandlerMessagePriority
{
  eInfo = 0,
  eError,
  eWarning,
};

// Global static instance declared in main
class LogHandler
{
public:
  LogHandler () {}
  ~LogHandler () {}

  inline void
  trim (void)
  {
    if (messages.size () > 500)
      messages.clear ();
    return;
  }

  // TODO
  // Add multi thread support
  void logMessage (std::pair<LogHandlerMessagePriority, std::string> p_Message);
  void logMessage (LogHandlerMessagePriority p_MessagePriority, std::string p_Message);
  void logMessage (std::string p_Message);
  std::vector<std::pair<LogHandlerMessagePriority, std::string>> getMessages (void);

private:
  std::vector<std::pair<LogHandlerMessagePriority, std::string>> messages;
}; // End class LogHandler

/*
    General Helper Methods
*/
namespace Helper
{
  // Create quick one time submit command buffer
  vk::CommandBuffer beginCommandBuffer (const vk::Device &p_LogicalDevice,
                                        const vk::CommandPool &p_CommandPool);

  // End specified command buffer & call present
  void endCommandBuffer (const vk::Device &p_LogicalDevice,
                         const vk::CommandPool &p_CommandPool,
                         const vk::CommandBuffer &p_CommandBuffer,
                         const vk::Queue &p_GraphicsQueue);

  void createStagingBuffer (vk::DeviceSize &p_BufferSize,
                            vk::Buffer &p_StagingBuffer,
                            vk::DeviceMemory &p_StagingMemory);

  template <typename T>
  uint32_t
  stouint32 (T const &type)
  {
    try
      {
        auto pos = std::stoull (type);
        auto trunc = pos & 0xff;
        return trunc;
      }
    catch (std::invalid_argument &e)
      {
        std::cout << "Invalid argument! Only Numerical values allowed!\n";
        throw std::invalid_argument (e.what ());
      }
    catch (std::out_of_range &e)
      {
        std::cout << "Number too large\n";
        throw std::out_of_range (e.what ());
      }
  }

  vk::CommandPool createCommandPool (uint32_t p_QueueIndex,
                                     vk::CommandPoolCreateFlags p_CreateFlags
                                     = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

  vk::CommandPool createCommandPool (vk::QueueFlagBits p_QueueFlag,
                                     vk::CommandPoolCreateFlags p_CreateFlags
                                     = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

  uint32_t getMemoryType (const vk::PhysicalDevice &p_PhysicalDevice,
                          uint32_t p_MemoryTypeBits,
                          const vk::MemoryPropertyFlags p_MemoryProperties,
                          vk::Bool32 *p_IsMemoryTypeFound = nullptr);

  // Returns an index to queue families which supports provided queue flag bit
  uint32_t getQueueFamilyIndex (vk::QueueFlagBits p_QueueFlagBit);

  // Gets depth format supported by physical device
  vk::Format getSupportedDepthFormat (void);

  // for reading shader spv
  std::vector<char> readFile (std::string p_Filename);

  // creates vk shadermodule from readFile() return type
  vk::ShaderModule createShaderModule (const std::vector<char> &p_ShaderCharBuffer);

  // create buffer with Vulkan objects
  void createBuffer (vk::BufferUsageFlags p_BufferUsageFlags,
                     vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                     vk::DeviceSize p_DeviceSize,
                     vk::Buffer *p_VkBuffer,
                     vk::DeviceMemory *p_DeviceMemory,
                     void *p_InitialData = nullptr);

  // create buffer with custom Buffer interface
  void createBuffer (vk::BufferUsageFlags p_BufferUsageFlags,
                     vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                     MvBuffer *p_MvBuffer,
                     vk::DeviceSize p_DeviceSize,
                     void *p_InitialData = nullptr);
}; // namespace Helper