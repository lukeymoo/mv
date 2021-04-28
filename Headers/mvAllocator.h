#ifndef HEADERS_MVALLOCATOR_H_
#define HEADERS_MVALLOCATOR_H_

#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>
#include <stdexcept>

namespace mv
{
    struct Allocator
    {
        VkDevice *device = nullptr;
        std::vector<VkDescriptorPool> cleared_pools; // completely cleared pools
        std::vector<VkDescriptorPool> usable_pools;  // pool with unused space
        std::vector<VkDescriptorPool> full_pools;    // completely full pools

        Allocator(void) {}
        ~Allocator(void)
        {
            if (!cleared_pools.empty())
            {
                for (auto &pool : cleared_pools)
                {
                    if (pool)
                    {
                        vkDestroyDescriptorPool(*device, pool, nullptr);
                        pool = nullptr;
                    }
                }
            }
            if (!usable_pools.empty())
            {
                for (auto &pool : usable_pools)
                {
                    if (pool)
                    {
                        vkDestroyDescriptorPool(*device, pool, nullptr);
                        pool = nullptr;
                    }
                }
            }
            if (!full_pools.empty())
            {
                for (auto &pool : full_pools)
                {
                    if (pool)
                    {
                        vkDestroyDescriptorPool(*device, pool, nullptr);
                        pool = nullptr;
                    }
                }
            }
            return;
        }

        /*
            Initializes allocator
        */
        void init(void)
        {
            return;
        }

        /*
            Called when fetching a pool to allocate from
        */
        VkDescriptorPool get_pool(VkDescriptorType type)
        {
            VkDescriptorPool pool = nullptr;

            // if there is a completely free pool
            // prefer it
            if (!cleared_pools.empty())
            {
                pool = cleared_pools.back();
                cleared_pools.pop_back();
            }
            // next in line is to check if an already in use pool is available & not yet full
            else if (!usable_pools.empty())
            {
                pool = usable_pools.back();
                usable_pools.pop_back();
            }
            // next is to create a new pool since none are available
            else
            {
                pool = allocate_pool(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2000);
            }
        }

        /*
            Called when no pools exist ready for use
            Creates a new pool
        */
        VkDescriptorPool allocate_pool(VkDescriptorType type, uint32_t count)
        {
            VkDescriptorPool pool = nullptr;

            VkDescriptorPoolSize p_size = {};
            p_size.type = type;
            p_size.descriptorCount = count;

            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.pNext = nullptr;
            pool_info.flags = 0; // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
            pool_info.poolSizeCount = 1;
            pool_info.pPoolSizes = &p_size;
            pool_info.maxSets = count;

            if (vkCreateDescriptorPool(*device, &pool_info, nullptr, &pool) != VK_SUCCESS)
            {
                throw std::runtime_error("Descriptor pool allocator failed to allocate pool");
            }

            return pool;
        }
    };
};

#endif