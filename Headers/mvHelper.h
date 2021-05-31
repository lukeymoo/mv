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
    LogHandler()
    {
    }
    ~LogHandler()
    {
    }

    inline void trim(void)
    {
        if (messages.size() > 500)
            messages.clear();
        return;
    }

    // TODO
    // Add multi thread support
    void logMessage(std::pair<LogHandlerMessagePriority, std::string> p_Message);
    void logMessage(LogHandlerMessagePriority p_MessagePriority, std::string p_Message);
    void logMessage(std::string p_Message);
    std::vector<std::pair<LogHandlerMessagePriority, std::string>> getMessages(void);

  private:
    std::vector<std::pair<LogHandlerMessagePriority, std::string>> messages;
}; // End class LogHandler

/*
    General Helper Methods
*/
namespace Helper
{
    // Create quick one time submit command buffer
    vk::CommandBuffer beginCommandBuffer(const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool);

    // End specified command buffer & call present
    void endCommandBuffer(const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool,
                          const vk::CommandBuffer &p_CommandBuffer, const vk::Queue &p_GraphicsQueue);

    std::vector<std::string> tokenize(const std::string p_ToSplit, const std::regex p_Regex);

    template <typename T>
    uint32_t stouint32(T const &type)
    {
        try
        {
            auto pos = std::stoull(type);
            auto trunc = pos & 0xff;
            return trunc;
        }
        catch (std::invalid_argument &e)
        {
            std::cout << "Invalid argument! Only Numerical values allowed!\n";
            throw std::invalid_argument(e.what());
        }
        catch (std::out_of_range &e)
        {
            std::cout << "Number too large\n";
            throw std::out_of_range(e.what());
        }
    }
}; // namespace Helper