#ifndef HEADERS_MVDEVICE_H_
#define HEADERS_MVDEVICE_H_

#include <vulkan/vulkan.hpp>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <algorithm>
#include <stdexcept>

#include <iostream>

#include "mvBuffer.h"

namespace mv
{
    struct Device
    {
        Device(std::weak_ptr<vk::PhysicalDevice> physical_device,
               std::vector<std::string> &requested_device_exts)
        {
            // validate param
            auto p_dvc = physical_device.lock();
            requested_physical_device_exts = requested_device_exts;

            if (!*p_dvc)
                throw std::runtime_error("Invalid physical device handle passed :: logical handler");

            this->physical_device = physical_device;
            this->requested_physical_device_exts = requested_device_exts;

            // fetch infos -- should prob just receive by reference from mv::MWindow
            physical_properties = p_dvc->getProperties();
            physical_features = p_dvc->getFeatures();
            physical_memory_properties = p_dvc->getMemoryProperties();
            queue_family_properties = p_dvc->getQueueFamilyProperties();

            if (queue_family_properties.size() < 1)
                throw std::runtime_error("Failed to find any queue family properties :: logical handler");

            // get supported physical device extensions
            physical_device_exts = p_dvc->enumerateDeviceExtensionProperties();

            if (physical_device_exts.size() < 1 && !requested_physical_device_exts.empty())
                throw std::runtime_error("Failed to find device extensions :: logical handler");

            auto check_if_supported = [&, this](const std::string requested_ext) {
                bool e = false;
                for (const auto &supp_ext : physical_device_exts)
                {
                    if (strcmp(supp_ext.extensionName, requested_ext.c_str()) == 0)
                    {
                        e = true;
                        break;
                    }
                }
                return e;
            };

            bool all_exist = std::all_of(requested_physical_device_exts.begin(),
                                         requested_physical_device_exts.end(),
                                         check_if_supported);

            if (!all_exist)
                throw std::runtime_error("Failed to find all requested device extensions");

            return;
        }
        ~Device()
        {
            if (command_pool)
            {
                logical_device->destroyCommandPool(*command_pool);
                command_pool.reset();
            }
            if (logical_device)
            {
                logical_device->destroy();
                logical_device.reset();
            }
            return;
        }

        // do not allow copy
        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;

        // allow move
        Device(Device &&) = default;
        Device &operator=(Device &&) = default;

        // owns
        std::shared_ptr<vk::Device> logical_device;
        std::shared_ptr<vk::Queue> graphics_queue;
        std::shared_ptr<vk::CommandPool> command_pool;

        // references
        std::weak_ptr<vk::PhysicalDevice> physical_device;

        // info structures
        vk::PhysicalDeviceFeatures physical_features;
        vk::PhysicalDeviceProperties physical_properties;
        vk::PhysicalDeviceMemoryProperties physical_memory_properties;

        std::vector<vk::ExtensionProperties> physical_device_exts;
        std::vector<vk::QueueFamilyProperties> queue_family_properties;

        std::vector<std::string> requested_physical_device_exts;

        struct
        {
            uint32_t graphics = UINT32_MAX;
            uint32_t compute = UINT32_MAX;
            uint32_t transfer = UINT32_MAX;
        } queue_idx;

        void create_logical_device(void);
        vk::CommandPool create_command_pool(uint32_t queue_index,
                                          vk::CommandPoolCreateFlags create_flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        uint32_t get_memory_type(uint32_t type_bits,
                                 vk::MemoryPropertyFlags properties,
                                 vk::Bool32 *mem_type_found = nullptr) const;

        uint32_t get_queue_family_index(vk::QueueFlagBits queue_flag_bit) const;

        vk::Format get_supported_depth_format(void);

        // create buffer with Vulkan objects
        void create_buffer(vk::BufferUsageFlags usage_flags,
                               vk::MemoryPropertyFlags memory_property_flags,
                               vk::DeviceSize size,
                               vk::Buffer *buffer,
                               vk::DeviceMemory *memory,
                               void *data = nullptr);

        // create buffer with custom mv::Buffer interface
        void create_buffer(vk::BufferUsageFlags usage_flags,
                               vk::MemoryPropertyFlags memory_property_flags,
                               mv::Buffer *buffer,
                               vk::DeviceSize size,
                               void *data = nullptr);
    };
};

#endif