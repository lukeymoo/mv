#include "mvHelper.h"

// Create quick one time submit command buffer
vk::CommandBuffer mv::Helper::beginCommandBuffer(const vk::Device &p_LogicalDevice,
                                                 const vk::CommandPool &p_CommandPool)
{
    if (!p_LogicalDevice)
        throw std::runtime_error(
            "Attempted to create one time submit command buffer but logical device is nullptr :: Helper tools");
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

void mv::Helper::endCommandBuffer(const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool,
                                  const vk::CommandBuffer &p_CommandBuffer, const vk::Queue &p_GraphicsQueue)
{
    // sanity check
    if (!p_CommandPool)
        throw std::runtime_error(
            "attempted to end command buffer recording but command pool is nullptr :: Helper tools");

    if (!p_CommandBuffer)
        throw std::runtime_error("attempted to end command buffer recording but the buffer passed "
                                 "as parameter is nullptr :: Helper tools");

    if (!p_GraphicsQueue)
        throw std::runtime_error(
            "attempted to end command buffer recording but graphics queue is nullptr :: Helper tools");

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