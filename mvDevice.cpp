#include "mvDevice.h"

mv::Device::Device(VkPhysicalDevice physicalDevice)
{
    assert(physicalDevice);
    this->physicalDevice = physicalDevice;

    vkGetPhysicalDeviceProperties(this->physicalDevice, &properties);
    vkGetPhysicalDeviceFeatures(this->physicalDevice, &enabledFeatures);
    vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memoryProperties);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(this->physicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(this->physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    // Get list of supported extensions
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(this->physicalDevice, nullptr, &extCount, nullptr);
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateDeviceExtensionProperties(this->physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        {
            for (auto ext : extensions)
            {
                supportedExtensions.push_back(ext.extensionName);
            }
        }
    }
}

mv::Device::~Device(void)
{
    if (m_CommandPool)
    {
        vkDestroyCommandPool(device, m_CommandPool, nullptr);
    }
    if (device) // logical device
    {
        vkDestroyDevice(device, nullptr);
    }
}

VkResult mv::Device::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> requestedExtensions)
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    const float defQueuePrio(0.0f);

    // Look for graphics queue
    queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.pNext = nullptr;
    queueInfo.flags = 0;
    queueInfo.queueCount = 1;
    queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
    queueInfo.pQueuePriorities = &defQueuePrio;

    queueCreateInfos.push_back(queueInfo);

    std::vector<const char *> deviceExtensions(requestedExtensions);

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;

    // device layers deprecated
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;

    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    if (deviceExtensions.size() > 0)
    {
        for (const auto &ext : deviceExtensions)
        {
            if (!extensionSupported(ext))
            {
                std::string m = "Failed to find device extension ";
                m += ext;
                throw std::runtime_error(m.c_str());
            }
        }
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    this->enabledFeatures = enabledFeatures;

    VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

    // Create command pool
    m_CommandPool = createCommandPool(queueFamilyIndices.graphics);

    return result;
}

uint32_t mv::Device::getQueueFamilyIndex(VkQueueFlags queueFlag)
{
    for (const auto &queueProperty : queueFamilyProperties)
    {
        if (queueProperty.queueFlags & queueFlag)
        {
            return (&queueProperty - &queueFamilyProperties[0]);
        }
    }

    throw std::runtime_error("Could not find requested queue family");
}

bool mv::Device::extensionSupported(std::string ext)
{
    return (std::find(supportedExtensions.begin(), supportedExtensions.end(), ext) != supportedExtensions.end());
}

VkCommandPool mv::Device::createCommandPool(uint32_t queueIndex, VkCommandPoolCreateFlags createFlags)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = createFlags;
    poolInfo.queueFamilyIndex = queueIndex;

    VkCommandPool cmdpool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &cmdpool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool");
    }
    return cmdpool;
}

uint32_t mv::Device::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound) const
{
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                if (memTypeFound)
                {
                    *memTypeFound = true;
                }
                return i;
            }
        }
        typeBits >>= 1;
    }

    if (memTypeFound)
    {
        *memTypeFound = false;
        return 0;
    }
    else
    {
        throw std::runtime_error("Could not find a matching memory type");
    }
}

VkFormat mv::Device::getSupportedDepthFormat(VkPhysicalDevice physicalDevice)
{
    std::vector<VkFormat> depthFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    for (auto &format : depthFormats)
    {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
            {
                continue;
            }
        }
        return format;
    }
    throw std::runtime_error("Failed to find good format");
}