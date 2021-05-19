#pragma once

#include <iostream>
#include <queue>
#include <vulkan/vulkan.hpp>

// Global static instance declared in main
class LogHandler
{
  public:
    LogHandler()
    {
    }
    ~LogHandler()
    {
    }

    enum MessagePriority
    {
        eInfo = 0,
        eError,
        eWarning,
    };

    inline void trim(void)
    {
        if (messages.size() > 500)
            messages.clear();
        return;
    }

    // TODO
    // Add multi thread support
    inline void logMessage(std::pair<MessagePriority, std::string> p_Message)
    {
        trim();
        messages.push_back(p_Message);
    }
    inline void logMessage(MessagePriority p_MessagePriority,
                           std::string p_Message)
    {
        trim();
        messages.push_back({p_MessagePriority, p_Message});
    }
    inline void logMessage(std::string p_Message)
    {
        trim();
        messages.push_back({MessagePriority::eInfo, p_Message});
    }
    inline std::vector<std::pair<MessagePriority, std::string>> getMessages(
        void)
    {
        return messages; // return copy of list
    }

  private:
    std::vector<std::pair<MessagePriority, std::string>> messages;
};

namespace Helper
{
    // Create quick one time submit command buffer
    vk::CommandBuffer beginCommandBuffer(const vk::Device &p_LogicalDevice,
                                         const vk::CommandPool &p_CommandPool);

    // End specified command buffer & call present
    void endCommandBuffer(const vk::Device &p_LogicalDevice,
                          const vk::CommandPool &p_CommandPool,
                          const vk::CommandBuffer &p_CommandBuffer,
                          const vk::Queue &p_GraphicsQueue);
}; // namespace Helper