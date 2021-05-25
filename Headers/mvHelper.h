#pragma once

#include <iostream>
#include <string>
#include <regex>
#include <queue>
#include <vector>

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
    void logMessage(std::pair<MessagePriority, std::string> p_Message);
    void logMessage(MessagePriority p_MessagePriority, std::string p_Message);
    void logMessage(std::string p_Message);
    std::vector<std::pair<MessagePriority, std::string>> getMessages(void);

  private:
    std::vector<std::pair<MessagePriority, std::string>> messages;
}; // End class LogHandler

/*
    General Helper Methods
*/
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

    std::vector<std::string> tokenize(const std::string p_ToSplit,
                                        const std::regex p_Regex);
}; // namespace Helper