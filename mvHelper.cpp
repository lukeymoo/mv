#include "mvHelper.h"

void LogHandler::logMessage(std::pair<LogHandlerMessagePriority, std::string> p_Message)
{
    trim();
    messages.push_back(p_Message);
}

void LogHandler::logMessage(LogHandlerMessagePriority p_MessagePriority, std::string p_Message)
{
    trim();
    messages.push_back({p_MessagePriority, p_Message});
}

void LogHandler::logMessage(std::string p_Message)
{
    trim();
    messages.push_back({LogHandlerMessagePriority::eInfo, p_Message});
}

std::vector<std::pair<LogHandlerMessagePriority, std::string>> LogHandler::getMessages(void)
{
    return messages; // return copy of list
}

// Create quick one time submit command buffer
vk::CommandBuffer Helper::beginCommandBuffer(const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool)
{
    if (!p_LogicalDevice)
        throw std::runtime_error("Attempted to create one time submit command buffer but logical "
                                 "device is nullptr :: Helper tools");
    if (!p_CommandPool)
        throw std::runtime_error("attempted to create one time submit command buffer but command "
                                 "pool is not initialized :: Helper tools\n");

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = p_CommandPool;
    allocInfo.commandBufferCount = 1;

    // allocate command buffer
    std::vector<vk::CommandBuffer> cmdbuf = p_LogicalDevice.allocateCommandBuffers(allocInfo);

    if (cmdbuf.size() < 1)
        throw std::runtime_error("Failed to create one time submit command buffer");

    // if more than 1 was created clean them up
    // shouldn't happen
    if (cmdbuf.size() > 1)
    {
        std::vector<vk::CommandBuffer> toDestroy;
        for (auto &buf : cmdbuf)
        {
            if (&buf - &cmdbuf[0] > 0)
            {
                toDestroy.push_back(buf);
            }
        }
        p_LogicalDevice.freeCommandBuffers(p_CommandPool, toDestroy);
    }

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmdbuf.at(0).begin(beginInfo);

    return cmdbuf.at(0);
}

void Helper::endCommandBuffer(const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool,
                              const vk::CommandBuffer &p_CommandBuffer, const vk::Queue &p_GraphicsQueue)
{
    // sanity check
    if (!p_CommandPool)
        throw std::runtime_error("attempted to end command buffer recording "
                                 "but command pool is nullptr :: Helper tools");

    if (!p_CommandBuffer)
        throw std::runtime_error("attempted to end command buffer recording but the buffer passed "
                                 "as parameter is nullptr :: Helper tools");

    if (!p_GraphicsQueue)
        throw std::runtime_error("attempted to end command buffer recording but graphics queue is "
                                 "nullptr :: Helper tools");

    p_CommandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &p_CommandBuffer;

    // submit buffer
    p_GraphicsQueue.submit(submitInfo);
    p_GraphicsQueue.waitIdle();

    // free buffer
    p_LogicalDevice.freeCommandBuffers(p_CommandPool, p_CommandBuffer);
    return;
}

std::vector<std::string> Helper::tokenize(const std::string p_ToSplit, const std::regex p_Regex)
{
    std::sregex_token_iterator it{p_ToSplit.begin(), p_ToSplit.end(), p_Regex, -1};
    std::vector<std::string> tokenized{it, {}};

    // Trim '{' and '}'
    std::for_each(tokenized.begin(), tokenized.end(), [](std::string &token) {
        // {
        auto pos = token.find('{');
        while (pos != std::string::npos)
        {
            token.erase(pos, 1);
            pos = token.find('{');
        }

        // }
        pos = token.find('}');
        while (pos != std::string::npos)
        {
            token.erase(pos, 1);
            pos = token.find('}');
        }
    });

    // Remove empty strings
    tokenized.erase(
        std::remove_if(tokenized.begin(), tokenized.end(), [](const std::string &s) { return s.size() == 0; }),
        tokenized.end());

    return tokenized;
}

// template <typename T>
// uint32_t Helper::stouint32(T const &type)
// {

// }