#ifndef HEADERS_MVALLOCATOR_H_
#define HEADERS_MVALLOCATOR_H_

#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "mvDevice.h"

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
        mv::Device *device;
        struct Container;
        uint32_t current_pool = 0;
        std::vector<std::unique_ptr<Container>> containers;
        std::unordered_map<std::string, std::unique_ptr<VkDescriptorSetLayout>> layouts_map;

        Allocator(mv::Device *device)
        {
            this->device = device;
            return;
        }
        ~Allocator(void)
        {
            cleanup();
            return;
        }

        void cleanup(void)
        {
            // cleanup descriptor layouts
            if (!layouts_map.empty())
            {
                for (auto &map : layouts_map)
                {
                    if (*(map.second.get())) // if valid layout
                    {
                        vkDestroyDescriptorSetLayout(device->device, *(map.second.get()), nullptr);
                        *(map.second.get()) = nullptr;
                    }
                }

                // clear map
                layouts_map.clear();
            }
            // cleanup descriptor pools
            if (!containers.empty())
            {
                for (auto &container : containers)
                {
                    if (container->pool)
                    {
                        vkDestroyDescriptorPool(device->device, container->pool, nullptr);
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
            // ensure a pool was allocated
            if (containers.size() == 0)
            {
                throw std::runtime_error("Requested pointer to currently active descriptor pool when none has been allocated!");
            }
            return containers.at(current_pool).get();
        }

        void create_layout(std::string layout_name, VkDescriptorType type, uint32_t count, VkPipelineStageFlags stage_flags, uint32_t binding)
        {
            VkDescriptorSetLayoutBinding bind_info = {};
            bind_info.binding = binding;
            bind_info.descriptorType = type;
            bind_info.descriptorCount = count;
            bind_info.stageFlags = stage_flags;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = 1;
            layout_info.pBindings = &bind_info;

            create_layout(layout_name, layout_info);
        }

        void create_layout(std::string layout_name, VkDescriptorSetLayoutCreateInfo &create_info)
        {
            bool layout_already_exists = false;
            // ensure name not already created
            for (const auto &map : layouts_map)
            {
                if (map.first == layout_name)
                {
                    layout_already_exists = true;
                    break;
                }
            }
            if (layout_already_exists)
            {
                std::cout << "[-] Request to create descriptor set layout => " << layout_name << " already exists; Skipping...\n";
                return;
            }

            // create layout
            VkDescriptorSetLayout tmp;
            if (vkCreateDescriptorSetLayout(device->device, &create_info, nullptr, &tmp) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create descriptor set layout");
            }

            // add to map
            layouts_map.insert({layout_name, std::make_unique<VkDescriptorSetLayout>(std::move(tmp))});
            std::cout << "[+] Descriptor set layout => " << layout_name << " created" << std::endl;
            return;
        }

        VkDescriptorSetLayout get_layout(std::string layout_name)
        {
            for (const auto &map : layouts_map)
            {
                if (map.first == layout_name)
                {
                    return *(map.second.get());
                }
            }

            throw std::runtime_error("Failed to find layout with specified name => " + layout_name);
        }

        // auto retrieve currently in use pool
        void allocate_set(VkDescriptorSetLayout &layout, VkDescriptorSet &set)
        {
            Container *container = containers.at(current_pool).get();

            std::cout << "[-] Allocating descriptor set" << std::endl;
            if (layout == nullptr)
            {
                std::ostringstream oss;
                oss << "Descriptor allocated was requested to allocate a set however the user provided nullptr for layout\n";
                oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
                throw std::runtime_error(oss.str().c_str());
            }

            VkResult result;
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = container->pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &layout;

            result = vkAllocateDescriptorSets(device->device, &alloc_info, &set);
            if (result == VK_SUCCESS)
            {
                // ensure allocator index is up to date
                current_pool = container->index;
            }
            else if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
            {
                if (result == VK_ERROR_OUT_OF_POOL_MEMORY)
                {
                    container->status = Status::Full;
                }
                else
                {
                    container->status = Status::Fragmented;
                }
                // allocate new pool
                auto new_pool = allocate_pool(container->count);
                // use new pool to allocate set
                allocate_set(new_pool, layout, set);
                // change passed container to new one
                container = new_pool;
            }
            else
            {
                throw std::runtime_error("Allocator failed to allocate descriptor set, fatal error");
            }
            return;
        }

        // allocate descriptor set from a specified pool
        void allocate_set(Container *container, VkDescriptorSetLayout &layout, VkDescriptorSet &set)
        {
            std::cout << "[-] Allocating descriptor set" << std::endl;
            if (layout == nullptr)
            {
                std::ostringstream oss;
                oss << "Descriptor allocated was requested to allocate a set however the user provided nullptr for layout\n";
                oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
                throw std::runtime_error(oss.str().c_str());
            }

            VkResult result;
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = container->pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &layout;

            result = vkAllocateDescriptorSets(device->device, &alloc_info, &set);
            if (result == VK_SUCCESS)
            {
                // ensure allocator index is up to date
                current_pool = container->index;
            }
            else if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
            {
                if (result == VK_ERROR_OUT_OF_POOL_MEMORY)
                {
                    container->status = Status::Full;
                }
                else
                {
                    container->status = Status::Fragmented;
                }
                // allocate new pool
                auto new_pool = allocate_pool(container->count);
                // use new pool to allocate set
                allocate_set(new_pool, layout, set);
                // change passed container to new one
                container = new_pool;
            }
            else
            {
                throw std::runtime_error("Allocator failed to allocate descriptor set, fatal error");
            }
        }

        // update set from auto retreived currently in use pool
        void update_set(VkDescriptorBufferInfo &buffer_info, VkDescriptorSet &dst_set, uint32_t dst_binding)
        {
            Container *container = containers.at(current_pool).get();
            
            VkWriteDescriptorSet update_info = {};
            update_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update_info.dstBinding = dst_binding;
            update_info.dstSet = dst_set;
            update_info.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            update_info.descriptorCount = 1;
            update_info.pBufferInfo = &buffer_info;

            vkUpdateDescriptorSets(device->device, 1, &update_info, 0, nullptr);
            current_pool = container->index;
            return;
        }

        // update set from specified descriptor pool
        void update_set(Container *container, VkDescriptorBufferInfo &buffer_info, VkDescriptorSet &dst_set, uint32_t dst_binding)
        {
            VkWriteDescriptorSet update_info = {};
            update_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update_info.dstBinding = dst_binding;
            update_info.dstSet = dst_set;
            update_info.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            update_info.descriptorCount = 1;
            update_info.pBufferInfo = &buffer_info;

            vkUpdateDescriptorSets(device->device, 1, &update_info, 0, nullptr);
            current_pool = container->index;
            return;
        }

        void update_set(Container *container, VkDescriptorImageInfo &image_info, VkDescriptorSet &dst_set, uint32_t dst_binding)
        {
            VkWriteDescriptorSet update_info = {};
            update_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update_info.dstBinding = dst_binding;
            update_info.dstSet = dst_set;
            update_info.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            update_info.descriptorCount = 1;
            update_info.pImageInfo = &image_info;

            vkUpdateDescriptorSets(device->device, 1, &update_info, 0, nullptr);
            current_pool = container->index;
            return;
        }

        typedef struct ContainerInitStruct
        {
            ContainerInitStruct(){};
            ~ContainerInitStruct(){};
            mv::Device *device;
            Allocator *parent_allocator = nullptr;                // ptr to allocator all pools have been allocated with
            std::vector<std::unique_ptr<Container>> *pools_array; // ptr to container for all pools
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

            mv::Device *device = nullptr;
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
                this->device = init_struct->device;
                this->parent_allocator = init_struct->parent_allocator;
                this->pools_array = init_struct->pools_array;
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
        };

        Container *allocate_pool(uint32_t count)
        {
            std::cout << "[+] Allocating descriptor pool of max sets => " << count << std::endl;
            std::vector<VkDescriptorPoolSize> pool_sizes = {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

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
            result = vkCreateDescriptorPool(device->device, &pool_info, nullptr, &pool);
            if (result == VK_SUCCESS)
            {
                ContainerInitStruct init_struct;
                init_struct.device = device;
                init_struct.parent_allocator = this;
                init_struct.pools_array = &containers;
                init_struct.count = count;

                // assign `self` after move to vector
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