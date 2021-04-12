#ifndef HEADERS_MVINIT_H_
#define HEADERS_MVINIT_H_

#include <vulkan/vulkan.h>

namespace mv
{
    namespace initializer
    {
        inline VkSemaphoreCreateInfo semaphoreCreateInfo()
        {
            VkSemaphoreCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            return info;
        }

        inline VkSubmitInfo submitInfo()
        {
            VkSubmitInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            return info;
        }

        inline VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool cmdPool,
                                                                     VkCommandBufferLevel level,
                                                                     uint32_t bufferCount)
        {
            VkCommandBufferAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.pNext = nullptr;
            ai.level = level;
            ai.commandBufferCount = bufferCount;
            ai.commandPool = cmdPool;
            return ai;
        }

        inline VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0)
        {
            VkFenceCreateInfo fi{};
            fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fi.flags = flags;
            return fi;
        }
    };
};

#endif