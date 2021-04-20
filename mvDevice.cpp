#include "mvDevice.h"

mv::Device::Device(VkPhysicalDevice physical_device)
{
    assert(physical_device);
    this->physical_device = physical_device;

    vkGetPhysicalDeviceProperties(this->physical_device, &properties);
    vkGetPhysicalDeviceFeatures(this->physical_device, &enabled_features);
    vkGetPhysicalDeviceMemoryProperties(this->physical_device, &memory_properties);

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(this->physical_device, &queue_family_count, nullptr);
    assert(queue_family_count > 0);
    queue_family_properties.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(this->physical_device, &queue_family_count, queue_family_properties.data());

    // Get list of supported extensions
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(this->physical_device, nullptr, &ext_count, nullptr);
    if (ext_count > 0)
    {
        std::vector<VkExtensionProperties> extensions(ext_count);
        if (vkEnumerateDeviceExtensionProperties(this->physical_device, nullptr, &ext_count, &extensions.front()) == VK_SUCCESS)
        {
            for (auto ext : extensions)
            {
                supported_extensions.push_back(ext.extensionName);
            }
        }
    }
}

mv::Device::~Device(void)
{
    if (m_command_pool)
    {
        vkDestroyCommandPool(device, m_command_pool, nullptr);
    }
    if (device) // logical device
    {
        vkDestroyDevice(device, nullptr);
    }
}

VkResult mv::Device::create_logical_device(VkPhysicalDeviceFeatures enabled_features, std::vector<const char *> requested_extensions)
{
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    const float def_queue_prio(0.0f);

    // Look for graphics queue
    queue_family_indices.graphics = get_queue_family_index(VK_QUEUE_GRAPHICS_BIT);
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.pNext = nullptr;
    queue_info.flags = 0;
    queue_info.queueCount = 1;
    queue_info.queueFamilyIndex = queue_family_indices.graphics;
    queue_info.pQueuePriorities = &def_queue_prio;

    queue_create_infos.push_back(queue_info);

    std::vector<const char *> device_extensions(requested_extensions);

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = nullptr;
    device_create_info.flags = 0;

    // device layers deprecated
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = nullptr;

    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &enabled_features;

    if (device_extensions.size() > 0)
    {
        for (const auto &ext : device_extensions)
        {
            if (!extension_supported(ext))
            {
                std::string m = "Failed to find device extension ";
                m += ext;
                throw std::runtime_error(m.c_str());
            }
        }
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
        device_create_info.ppEnabledExtensionNames = device_extensions.data();
    }

    this->enabled_features = enabled_features;

    VkResult result = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);

    // Create command pool
    m_command_pool = create_command_pool(queue_family_indices.graphics);

    return result;
}

uint32_t mv::Device::get_queue_family_index(VkQueueFlags queue_flag)
{
    for (const auto &queue_property : queue_family_properties)
    {
        if (queue_property.queueFlags & queue_flag)
        {
            return (&queue_property - &queue_family_properties[0]);
        }
    }

    throw std::runtime_error("Could not find requested queue family");
}

bool mv::Device::extension_supported(std::string ext)
{
    return (std::find(supported_extensions.begin(), supported_extensions.end(), ext) != supported_extensions.end());
}

VkCommandPool mv::Device::create_command_pool(uint32_t queue_index, VkCommandPoolCreateFlags create_flags)
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.pNext = nullptr;
    pool_info.flags = create_flags;
    pool_info.queueFamilyIndex = queue_index;

    VkCommandPool cmdpool;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &cmdpool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool");
    }
    return cmdpool;
}

uint32_t mv::Device::get_memory_type(uint32_t type_bits, VkMemoryPropertyFlags properties, VkBool32 *mem_type_found) const
{
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
    {
        if ((type_bits & 1) == 1)
        {
            if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                if (mem_type_found)
                {
                    *mem_type_found = true;
                }
                return i;
            }
        }
        type_bits >>= 1;
    }

    if (mem_type_found)
    {
        *mem_type_found = false;
        return 0;
    }
    else
    {
        throw std::runtime_error("Could not find a matching memory type");
    }
}

VkFormat mv::Device::get_supported_depth_format(VkPhysicalDevice physical_device)
{
    std::vector<VkFormat> depth_formats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    for (auto &format : depth_formats)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &format_properties);
        if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
            {
                continue;
            }
        }
        return format;
    }
    throw std::runtime_error("Failed to find good format");
}

VkResult mv::Device::create_buffer(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_property_flags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data)
{
    VkBufferCreateInfo buffer_info = mv::initializer::buffer_create_info(usage_flags, size);
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buffer_info, nullptr, buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer");
    }

    // allocate memory for buffer
    VkMemoryRequirements memreqs = {};
    VkMemoryAllocateInfo alloc_info = mv::initializer::memory_allocate_info();

    vkGetBufferMemoryRequirements(device, *buffer, &memreqs);
    alloc_info.allocationSize = memreqs.size;
    alloc_info.memoryTypeIndex = get_memory_type(memreqs.memoryTypeBits, memory_property_flags);

    // Allocate memory
    if (vkAllocateMemory(device, &alloc_info, nullptr, memory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    // Bind newly allocated memory to buffer
    if (vkBindBufferMemory(device, *buffer, *memory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to bind buffer memory");
    }

    // If data was passed to creation, load it
    if (data != nullptr)
    {
        void *mapped;
        if (vkMapMemory(device, *memory, 0, size, 0, &mapped) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map buffer memory during creation");
        }
        // TODO
        // add manual flush
        // if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        // {
        // }
        memcpy(mapped, data, size);
        vkUnmapMemory(device, *memory);
    }

    return VK_SUCCESS;
}

VkResult mv::Device::create_buffer(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_property_flags, mv::Buffer *buffer, VkDeviceSize size, void *data)
{
    buffer->device = device;

    // create buffer
    VkBufferCreateInfo buffer_info = mv::initializer::buffer_create_info(usage_flags, size);
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer->buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Helper failed to create buffer");
    }

    // create memory
    VkMemoryRequirements memreqs = {};
    VkMemoryAllocateInfo allocInfo = mv::initializer::memory_allocate_info();

    vkGetBufferMemoryRequirements(device, buffer->buffer, &memreqs);
    allocInfo.allocationSize = memreqs.size;
    allocInfo.memoryTypeIndex = get_memory_type(memreqs.memoryTypeBits, memory_property_flags);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &buffer->memory) != VK_SUCCESS)
    {
        throw std::runtime_error("Helper failed to allocate memory for buffer");
    }

    buffer->alignment = memreqs.alignment;
    buffer->size = size;
    buffer->usage_flags = usage_flags;
    buffer->memory_property_flags = memory_property_flags;

    // copy if necessary
    if (data != nullptr)
    {
        if (buffer->map() != VK_SUCCESS)
        {
            throw std::runtime_error("Helper failed to map buffer");
        }
        memcpy(buffer->mapped, data, size);
        // TODO
        // add manual flush
        // if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        // {
        // }
        buffer->unmap();
    }

    buffer->setup_descriptor();
    
    return buffer->bind();
}