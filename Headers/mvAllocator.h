#ifndef HEADERS_MVALLOCATOR_H_
#define HEADERS_MVALLOCATOR_H_

#include <vulkan/vulkan.hpp>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "mvDevice.h"

namespace mv
{
    struct Allocator
    {
        // forward decl
        struct Container;

        // owns
        std::unique_ptr<std::vector<Container>> containers;
        std::unique_ptr<std::unordered_map<std::string, vk::DescriptorSetLayout>> layouts;

        // references
        std::weak_ptr<mv::Device> mv_device;

        // infos
        uint32_t current_pool = 0;

        // disallow copy
        Allocator(const Allocator &) = delete;
        Allocator &operator=(const Allocator &) = delete;

        // allow move
        Allocator(Allocator &&) = default;
        Allocator &operator=(Allocator &&) = default;

        Allocator(std::weak_ptr<mv::Device> mv_device)
        {
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);
            if (!m_dvc)
                throw std::runtime_error("Invalid reference to mv device handler creating allocator :: descriptor handler");

            this->mv_device = mv_device;
            return;
        }
        ~Allocator()
        {
            cleanup();
            return;
        }

        void cleanup(void)
        {
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);
            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, cleaning up :: descriptor handler");

            if (layouts)
            {
                if (!layouts->empty())
                {
                    auto destroy_layout = [&, this](std::pair<std::string, vk::DescriptorSetLayout> entry) {
                        if (entry.second)
                            m_dvc->logical_device->destroyDescriptorSetLayout(entry.second);
                    };

                    std::all_of(layouts->begin(), layouts->end(), destroy_layout);
                }
                layouts.reset();
            }

            if (containers)
            {
                if (!containers->empty())
                {
                    auto destroy_pool = [&, this](mv::Allocator::Container container) {
                        m_dvc->logical_device->destroyDescriptorPool(container.pool);
                    };

                    std::all_of(containers->begin(), containers->end(), destroy_pool);
                }
                containers.reset();
            }
            return;
        }

        // returns handle to current pool in use
        Container *get(void)
        {
            if (!containers || containers->empty())
                throw std::runtime_error("Containers never allocated, attempted to get ptr to current pool :: descriptor handler");

            return &containers->at(current_pool);
        }

        void create_layout(std::string layout_name,
                           vk::DescriptorType type,
                           uint32_t count,
                           vk::ShaderStageFlagBits shader_stage_flag_bits,
                           uint32_t binding)
        {
            vk::DescriptorSetLayoutBinding bind_info;
            bind_info.binding = binding;
            bind_info.descriptorType = type;
            bind_info.descriptorCount = count;
            bind_info.stageFlags = shader_stage_flag_bits;

            vk::DescriptorSetLayoutCreateInfo layout_info;
            layout_info.bindingCount = 1;
            layout_info.pBindings = &bind_info;

            create_layout(layout_name, layout_info);
        }

        void create_layout(std::string layout_name,
                           vk::DescriptorSetLayoutCreateInfo &create_info)
        {
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);

            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, creating set layout :: descriptor handler");

            bool layout_already_exists = false;

            if (!layouts)
            {
                layouts = std::make_unique<std::unordered_map<std::string, vk::DescriptorSetLayout>>();
            }
            else
            {
                // ensure name not already created
                for (const auto &map : *layouts)
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
            }

            // add to map
            layouts->insert({layout_name, m_dvc->logical_device->createDescriptorSetLayout(create_info)});
            std::cout << "[+] Descriptor set layout => " << layout_name << " created" << std::endl;
            return;
        }

        vk::DescriptorSetLayout get_layout(std::string layout_name)
        {
            if (!layouts)
                throw std::runtime_error("Requested layout but container never initialized => " + layout_name);

            for (const auto &map : *layouts)
            {
                if (map.first == layout_name)
                {
                    return map.second;
                }
            }
            throw std::runtime_error("Requested layout but not found => " + layout_name);
        }

        // auto retrieve currently in use pool
        void allocate_set(vk::DescriptorSetLayout &layout, vk::DescriptorSet &set)
        {
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);

            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, allocating descriptor set :: descriptor handler");

            Container *container = &containers->at(current_pool);

            // ensure pool exist
            if (!container->pool)
                throw std::runtime_error("No pool ever allocated, attempting to allocate descriptor set");

            std::cout << "[-] Allocating descriptor set" << std::endl;
            if (!layout)
            {
                std::ostringstream oss;
                oss << "Descriptor allocated was requested to allocate a set however the user provided nullptr for layout\n";
                oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
                throw std::runtime_error(oss.str().c_str());
            }

            vk::DescriptorSetAllocateInfo alloc_info;
            alloc_info.descriptorPool = container->pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &layout;

            vk::Result result = m_dvc->logical_device->allocateDescriptorSets(&alloc_info, &set);

            if (result == vk::Result::eSuccess)
            {
                // ensure allocator index is up to date
                current_pool = container->index;
            }
            else if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool)
            {
                if (result == vk::Result::eErrorOutOfPoolMemory)
                {
                    container->status = Container::Status::Full;
                }
                else
                {
                    container->status = Container::Status::Fragmented;
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
        void allocate_set(Container *container, vk::DescriptorSetLayout &layout, vk::DescriptorSet &set)
        {
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);

            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, allocating descriptor set :: descriptor handler");

            Container *container = &containers->at(current_pool);

            // ensure pool exist
            if (!container->pool)
                throw std::runtime_error("No pool ever allocated, attempting to allocate descriptor set");

            std::cout << "[-] Allocating descriptor set" << std::endl;
            if (!layout)
            {
                std::ostringstream oss;
                oss << "Descriptor allocated was requested to allocate a set however the user provided nullptr for layout\n";
                oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
                throw std::runtime_error(oss.str().c_str());
            }

            vk::DescriptorSetAllocateInfo alloc_info;
            alloc_info.descriptorPool = container->pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &layout;

            vk::Result result = m_dvc->logical_device->allocateDescriptorSets(&alloc_info, &set);
            if (result == vk::Result::eSuccess)
            {
                // ensure allocator index is up to date
                current_pool = container->index;
            }
            else if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool)
            {
                if (result == vk::Result::eErrorOutOfPoolMemory)
                {
                    container->status = Container::Status::Full;
                }
                else
                {
                    container->status = Container::Status::Fragmented;
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
        void update_set(vk::DescriptorBufferInfo &buffer_info, vk::DescriptorSet &dst_set, uint32_t dst_binding)
        {
            Container *container = &containers->at(current_pool);

            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);

            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, updating descriptor set :: descriptor handler");

            Container *container = &containers->at(current_pool);

            // ensure pool exist
            if (!container->pool)
                throw std::runtime_error("No pool ever allocated, attempting to update descriptor set :: descriptor handler");

            vk::WriteDescriptorSet update_info;
            update_info.dstBinding = dst_binding;
            update_info.dstSet = dst_set;
            update_info.descriptorType = vk::DescriptorType::eUniformBuffer;
            update_info.descriptorCount = 1;
            update_info.pBufferInfo = &buffer_info;

            m_dvc->logical_device->updateDescriptorSets(update_info, nullptr);
            current_pool = container->index;
            return;
        }

        // update set from specified descriptor pool
        void update_set(Container *container, vk::DescriptorBufferInfo &buffer_info, vk::DescriptorSet &dst_set, uint32_t dst_binding)
        {
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);

            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, updating descriptor set :: descriptor handler");

            vk::WriteDescriptorSet update_info;
            update_info.dstBinding = dst_binding;
            update_info.dstSet = dst_set;
            update_info.descriptorType = vk::DescriptorType::eUniformBuffer;
            update_info.descriptorCount = 1;
            update_info.pBufferInfo = &buffer_info;

            m_dvc->logical_device->updateDescriptorSets(update_info, nullptr);
            current_pool = container->index;
            return;
        }

        void update_set(Container *container, vk::DescriptorImageInfo &image_info, vk::DescriptorSet &dst_set, uint32_t dst_binding)
        {
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);

            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, updating descriptor set :: descriptor handler");

            vk::WriteDescriptorSet update_info;
            update_info.dstBinding = dst_binding;
            update_info.dstSet = dst_set;
            update_info.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            update_info.descriptorCount = 1;
            update_info.pImageInfo = &image_info;

            m_dvc->logical_device->updateDescriptorSets(update_info, nullptr);
            current_pool = container->index;
            return;
        }

        typedef struct ContainerInitStruct
        {
            ContainerInitStruct(void){};
            ~ContainerInitStruct(){};

            std::weak_ptr<mv::Device> device;

            uint32_t count; // max sets specified at pool allocation

            // READ ONLY USE
            mv::Allocator *parent_allocator = nullptr;          // ptr to allocator all pools have been allocated with
            std::vector<mv::Allocator::Container> *pools_array; // ptr to container for all pools
        } ContainerInitStruct;

        struct Container
        {
            enum Status
            {
                Clear,
                Usable,
                Fragmented,
                Full
            };

            // owns
            vk::DescriptorPool pool; // not unique_ptr because managed at higher level

            // references
            std::weak_ptr<mv::Device> mv_device;
            Allocator *parent_allocator = nullptr;
            std::vector<Container> *pools_array; // pointer to container for all pools

            // infos
            uint32_t index;          // index to self in pools_array
            uint32_t count;          // max sets requested from this pool on allocation
            vk::DescriptorType type; // type of descriptors this pool was created for
            Container *self = nullptr;
            Container::Status status = Status::Clear;

            // Disallow copy
            Container(const Container &) = delete;
            Container &operator=(const Container &) = delete;

            // Allow move
            Container(Container &&) = default;
            Container &operator=(Container &&) = delete;

            Container(ContainerInitStruct &init_struct)
            {
                // WILL NOT WORK...
                // this constructor has a different address due to implicit calls to copy/move when placing
                // this object into vector; `this->index` is specified when looping the container of descriptor pools
                // this constructor will look for itself and make note of where in the array it was found
                this->mv_device = init_struct.device;
                this->parent_allocator = init_struct.parent_allocator;
                this->pools_array = init_struct.pools_array;
                this->count = init_struct.count;
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
            std::shared_ptr<mv::Device> m_dvc = std::make_shared<mv::Device>(mv_device);

            if (!m_dvc)
                throw std::runtime_error("Failed to reference mv device handler, allocating descriptor pool :: descriptor handler");

            std::cout << "[+] Allocating descriptor pool of max sets => " << count << std::endl;
            std::vector<vk::DescriptorPoolSize> pool_sizes = {
                {vk::DescriptorType::eUniformBuffer, 1},
                {vk::DescriptorType::eCombinedImageSampler, 1}};

            if (pool_sizes.empty())
            {
                throw std::runtime_error("Unsupported descriptor pool type requested");
            }

            vk::DescriptorPoolCreateInfo pool_info;
            pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
            pool_info.pPoolSizes = pool_sizes.data();
            pool_info.maxSets = count;

            ContainerInitStruct init_struct;
            init_struct.device = mv_device;
            init_struct.parent_allocator = this;
            init_struct.pools_array = containers.get();
            init_struct.count = count;

            // assign `self` after move to vector
            Container np(init_struct);
            np.pool = m_dvc->logical_device->createDescriptorPool(pool_info);
            np.status = Container::Status::Clear;
            containers->push_back(np);
            // give object its addr & index
            containers->back().self = &containers->back();
            containers->back().index = containers->size() - 1;
            current_pool = containers->size() - 1;

            return &containers->back();
        }
    };
};

#endif