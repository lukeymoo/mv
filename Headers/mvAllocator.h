#ifndef HEADERS_MVALLOCATOR_H_
#define HEADERS_MVALLOCATOR_H_

#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>

namespace mv
{
    enum Status
    {
        Clear,
        Usable,
        Fragmented,
        Full
    };

    struct Allocator
    {
        VkDevice device;
        struct Container;
        std::vector<std::unique_ptr<Container>> containers;
        uint32_t current_pool = 0;

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

        // returns handle to current pool in use
        Container *get(void)
        {
            return containers.at(current_pool).get();
        }

        typedef struct ContainerInitStruct
        {
            ContainerInitStruct(){};
            ~ContainerInitStruct(){};
            VkDevice *device;
            Allocator *parent_allocator = nullptr;                // ptr to allocator all pools have been allocated with
            std::vector<std::unique_ptr<Container>> *pools_array; // ptr to container for all pools
            VkDescriptorType type;                                // type of descriptors this pool was allocated for
            uint32_t count;                                       // max sets specified at pool allocation
        } ContainerInitStruct;

        struct Container
        {
            // container should be passed a pointer to it's vector container
            // in the event a new pool is required to be allocated
            // this container can...
            // change its status flag to full
            // allocate a new container
            // call the same allocate_set function with the new handle
            // return ptr to new pool
            // if no new pool was required, container will return pointer to itself to allow reuse
            // TODO -- prevent infinite loop on potential to recreate pool over and over again

            VkDevice device = nullptr;
            Allocator *parent_allocator = nullptr;
            std::vector<std::unique_ptr<Container>> *pools_array; // pointer to container for all pools
            uint32_t index;                                       // index to self in pools_array
            VkDescriptorType type;                                // type of descriptors this pool was created for
            uint32_t count;                                       // max sets requested from this pool on allocation
            VkDescriptorPool pool;
            Status status = Status::Clear;
            // address to self, must be passed because of implicit calls to copy/move in vector manipulation
            // attempting to use `this` to acquire addr always ends up returning a pointer to object that existed prior to copy
            Container *self = nullptr;

            Container(ContainerInitStruct *init_struct)
            {
                // WILL NOT WORK...
                // this constructor has a different address due to implicit calls to copy/move when placing this object into vector
                // `this->index` is specified when looping the container of descriptor pools
                // this constructor will look for itself and make note of where in the array it was found
                this->device = *(init_struct->device);
                this->parent_allocator = init_struct->parent_allocator;
                this->pools_array = init_struct->pools_array;
                this->type = init_struct->type;
                this->count = init_struct->count;
            }
            // parent container `Allocator` does the cleanup of all it's child objects
            // Not here because as the container vector is resized/recreated with every
            // new pool allocation, there are implicit calls to destructors
            // such implicit calls would destroy pools potentially while in use or invalidate
            // descriptor sets that are pending use
            ~Container()
            {
            }
            void test(void)
            {
                std::cout << "This => " << self << " Index => " << index << std::endl;
                for (size_t i = 0; i < pools_array->size(); i++)
                {
                    std::cout << "pools_array element addr => " << pools_array->at(i).get()
                              << " Index => " << i << std::endl;
                }
                return;
            }

            // Allocate descriptor set
            Container *allocate_set(VkDescriptorSetLayout &layout, VkDescriptorSet &set)
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
                    // ensure allocator index is up to date
                    parent_allocator->current_pool = this->index;
                    return this;
                }
                else if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
                {
                    if (result == VK_ERROR_OUT_OF_POOL_MEMORY)
                    {
                        status = Status::Full;
                    }
                    else
                    {
                        status = Status::Fragmented;
                    }
                    // allocate new pool, store addr
                     auto new_pool = parent_allocator->allocate_pool(type, count);
                    // use new pool to allocate set
                    return new_pool->allocate_set(layout, set);
                }
                else
                {
                    throw std::runtime_error("Allocator failed to allocate descriptor set, fatal error");
                }
            }

            Container *update_set(VkDescriptorBufferInfo &buffer_info, VkDescriptorSet &dst_set, uint32_t dst_binding)
            {
                VkWriteDescriptorSet update_info = {};
                update_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                update_info.dstBinding = dst_binding;
                update_info.dstSet = dst_set;
                update_info.descriptorType = type;
                update_info.descriptorCount = 1;
                update_info.pBufferInfo = &buffer_info;

                vkUpdateDescriptorSets(device, 1, &update_info, 0, nullptr);
                parent_allocator->current_pool = this->index;
                return this;
            }
        };

        Container *allocate_pool(VkDescriptorType type, uint32_t count)
        {
            std::vector<VkDescriptorPoolSize> pool_sizes;

            if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                pool_sizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * count}};
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
                ContainerInitStruct init_struct;
                init_struct.device = &device;
                init_struct.parent_allocator = this;
                init_struct.pools_array = &containers;
                init_struct.type = type;
                init_struct.count = count;

                // assign self after move to vector

                Container np(&init_struct);
                np.pool = pool;
                np.status = Status::Clear;
                containers.push_back(std::make_unique<Container>(std::move(np)));
                // give object its addr & index
                containers.back()->self = containers.back().get();
                containers.back()->index = containers.size() - 1;
                current_pool = containers.size() - 1;

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