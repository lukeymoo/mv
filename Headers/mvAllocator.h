#ifndef HEADERS_MVALLOCATOR_H_
#define HEADERS_MVALLOCATOR_H_

#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>
#include <stdexcept>

namespace mv
{
    enum Status
    {
        Clear,
        Usable,
        Full
    };

    struct Allocator
    {
        VkDevice device;

        Allocator(VkDevice &device)
        {
            this->device = device;
            return;
        }
        ~Allocator(void)
        {
            if (!containers.empty())
            {
                for (auto &container : containers)
                {
                    if (container->pool)
                    {
                        vkDestroyDescriptorPool(device, container->pool, nullptr);
                        container->pool = nullptr;
                    }
                }

                containers.clear();
            }
            return;
        }

        struct Container
        {
            VkDevice device = nullptr;

            Container(VkDevice &device)
            {
                this->device = device;
            }
            ~Container()
            {
            }

            VkDescriptorPool pool;
            Status status = Status::Clear;

            // Allocate descriptor set
            bool allocate_set(VkDescriptorSetLayout &layout, VkDescriptorSet &set)
            {
                VkResult result;
                VkDescriptorSetAllocateInfo alloc_info = {};
                alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                alloc_info.descriptorPool = pool;
                alloc_info.descriptorSetCount = 1;
                alloc_info.pSetLayouts = &layout;

                result = vkAllocateDescriptorSets(device, &alloc_info, &set);
                if (result == VK_SUCCESS)
                {
                    printf("Allocated descriptor set\n");
                    return true;
                }
                else if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
                {
                    // allocate new pool & use that
                    printf("Descriptor pool currently in use is now full or fragmented, creating new pool\n"); 
                }
                else
                {
                    throw std::runtime_error("Allocator failed to allocate descriptor set, fatal error");
                }
                return true;
            }
        };

        std::vector<std::unique_ptr<Container>> containers;

        Container *allocate_pool(VkDescriptorType type, uint32_t count)
        {
            std::vector<VkDescriptorPoolSize> pool_sizes;

            if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                pool_sizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}};
            }

            if (pool_sizes.empty())
            {
                throw std::runtime_error("Unsupported descriptor pool type requested");
            }

            VkResult result;
            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
            pool_info.pPoolSizes = pool_sizes.data();
            pool_info.maxSets = count;

            VkDescriptorPool pool = nullptr;
            result = vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
            if (result == VK_SUCCESS)
            {
                printf("Allocating descriptor pool with maxSets => %i\n", count);
                Container np(device);
                np.pool = pool;
                np.status = Status::Clear;
                containers.push_back(std::make_unique<Container>(np));
                return containers.back().get();
            }
            else
            {
                // case VK_ERROR_FRAGMENTATION_EXT:
                // case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                // case VK_ERROR_OUT_OF_HOST_MEMORY:
                throw std::runtime_error("Failed to allocate descriptor pool, system out of memory or excessive fragmentation");
            }
        }
    };
};

#endif