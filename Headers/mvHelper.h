#ifndef HEADERS_HELPER_H_
#define HEADERS_HELPER_H_

#include <vulkan/vulkan.hpp>

namespace mv
{
    namespace Helper
    {
        // Create quick one time submit command buffer
        vk::CommandBuffer beginCommandBuffer(const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool);

        // End specified command buffer & call present
        void endCommandBuffer(const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool,
                              const vk::CommandBuffer &p_CommandBuffer, const vk::Queue &p_GraphicsQueue);
    }; // namespace Helper
};     // namespace mv

#endif