#include "mvDevice.h"

mv::Device::Device(const vk::PhysicalDevice &p_PhysicalDevice, std::vector<std::string> &p_RequestedDeviceExtensions)
{
    this->requestedPhysicalDeviceExtensions = p_RequestedDeviceExtensions;

    // clang-format off
    physicalProperties                      = p_PhysicalDevice.getProperties();
    physicalMemoryProperties                = p_PhysicalDevice.getMemoryProperties();
    queueFamilyProperties                   = p_PhysicalDevice.getQueueFamilyProperties();
    physicalFeatures                        = p_PhysicalDevice.getFeatures();
    physicalFeatures2                       = p_PhysicalDevice.getFeatures2();
    extendedFeatures.extendedDynamicState   = VK_TRUE;
    physicalFeatures2.pNext                 = &extendedFeatures;
    // clang-format on

    if (queueFamilyProperties.empty())
        throw std::runtime_error("Failed to find any queue family properties :: logical handler");

    // get supported physical device extensions
    physicalDeviceExtensions = p_PhysicalDevice.enumerateDeviceExtensionProperties();

    if (physicalDeviceExtensions.empty() && !requestedPhysicalDeviceExtensions.empty())
        throw std::runtime_error("Failed to find device extensions :: logical handler");

    auto checkIfSupported = [&, this](const std::string requested_ext) {
        bool e = false;
        for (const auto &supportedExtension : physicalDeviceExtensions)
        {
            if (strcmp(supportedExtension.extensionName, requested_ext.c_str()) == 0)
            {
                std::cout << "\t\tFound device extension => " << supportedExtension.extensionName << "\n";
                e = true;
                break;
            }
        }
        return e;
    };

    bool haveAllExtensions = std::all_of(requestedPhysicalDeviceExtensions.begin(),
                                         requestedPhysicalDeviceExtensions.end(), checkIfSupported);

    if (!haveAllExtensions)
        throw std::runtime_error("Failed to find all requested device extensions");

    return;
}

mv::Device::~Device()
{
    if (commandPool)
    {
        logicalDevice->destroyCommandPool(*commandPool);
        commandPool.reset();
    }
    if (logicalDevice)
    {
        logicalDevice->destroy();
        logicalDevice.reset();
    }
    return;
}

void mv::Device::createLogicalDevice(const vk::PhysicalDevice &p_PhysicalDevice)
{
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

    constexpr float defaultQueuePriority = 0.0f;

    // Look for graphics queue
    queueIdx.graphics = getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);

    vk::DeviceQueueCreateInfo queueInfo;
    queueInfo.queueCount = 1;
    queueInfo.queueFamilyIndex = queueIdx.graphics;
    queueInfo.pQueuePriorities = &defaultQueuePriority;

    queueCreateInfos.push_back(queueInfo);

    std::vector<const char *> enabledExtensions;
    for (const auto &req : requestedPhysicalDeviceExtensions)
    {
        enabledExtensions.push_back(req.c_str());
    }
    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.pNext = &physicalFeatures2;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

    // if pnext is phys features2, must be nullptr
    // device_create_info.pEnabledFeatures = &physical_features;

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // create logical device
    logicalDevice = std::make_unique<vk::Device>(p_PhysicalDevice.createDevice(deviceCreateInfo));

    // create command pool with graphics queue
    commandPool = std::make_unique<vk::CommandPool>(createCommandPool(queueIdx.graphics));

    // retreive graphics queue
    graphicsQueue = std::make_unique<vk::Queue>(logicalDevice->getQueue(queueIdx.graphics, 0));

    return;
}

uint32_t mv::Device::getQueueFamilyIndex(vk::QueueFlagBits p_QueueFlagBits) const
{
    for (const auto &queueProperty : queueFamilyProperties)
    {
        if ((queueProperty.queueFlags & p_QueueFlagBits))
        {
            return (&queueProperty - &queueFamilyProperties[0]);
        }
    }

    throw std::runtime_error("Could not find requested queue family");
}

vk::CommandPool mv::Device::createCommandPool(uint32_t p_QueueIndex, vk::CommandPoolCreateFlags p_CreateFlags)
{
    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.flags = p_CreateFlags;
    poolInfo.queueFamilyIndex = p_QueueIndex;

    return logicalDevice->createCommandPool(poolInfo);
}

uint32_t mv::Device::getMemoryType(uint32_t p_MemoryTypeBits, vk::MemoryPropertyFlags p_MemoryProperties,
                                   vk::Bool32 *p_IsMemoryTypeFound) const
{
    for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
    {
        if ((p_MemoryTypeBits & 1) == 1)
        {
            if ((physicalMemoryProperties.memoryTypes[i].propertyFlags & p_MemoryProperties))
            {
                if (p_IsMemoryTypeFound)
                {
                    *p_IsMemoryTypeFound = true;
                }
                return i;
            }
        }
        p_MemoryTypeBits >>= 1;
    }

    if (p_IsMemoryTypeFound)
    {
        *p_IsMemoryTypeFound = false;
        return 0;
    }
    else
    {
        throw std::runtime_error("Could not find a matching memory type");
    }
}

vk::Format mv::Device::getSupportedDepthFormat(const vk::PhysicalDevice &p_PhysicalDevice)
{

    std::vector<vk::Format> depthFormats = {vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat,
                                            vk::Format::eD24UnormS8Uint, vk::Format::eD16UnormS8Uint,
                                            vk::Format::eD16Unorm};

    for (auto &format : depthFormats)
    {
        vk::FormatProperties formatProperties = p_PhysicalDevice.getFormatProperties(format);
        if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage))
                continue;
        }
        return format;
    }
    throw std::runtime_error("Failed to find good format");
}

void mv::Device::createBuffer(vk::BufferUsageFlags p_BufferUsageFlags, vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                              vk::DeviceSize p_DeviceSize, vk::Buffer *p_VkBuffer, vk::DeviceMemory *p_DeviceMemory,
                              void *p_InitialData) const
{
    std::cout << "\t[-] Allocating buffer of size => " << static_cast<uint32_t>(p_DeviceSize) << std::endl;
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.usage = p_BufferUsageFlags;
    bufferInfo.size = p_DeviceSize;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    // create vulkan buffer
    *p_VkBuffer = logicalDevice->createBuffer(bufferInfo);

    // allocate memory for buffer
    vk::MemoryRequirements memRequirements;
    vk::MemoryAllocateInfo allocInfo;

    memRequirements = logicalDevice->getBufferMemoryRequirements(*p_VkBuffer);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = getMemoryType(memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

    // Allocate memory
    *p_DeviceMemory = logicalDevice->allocateMemory(allocInfo);

    // Bind newly allocated memory to buffer
    logicalDevice->bindBufferMemory(*p_VkBuffer, *p_DeviceMemory, 0);

    // If data was passed to creation, load it
    if (p_InitialData != nullptr)
    {
        void *mapped = logicalDevice->mapMemory(*p_DeviceMemory, 0, p_DeviceSize);

        memcpy(mapped, p_InitialData, p_DeviceSize);

        logicalDevice->unmapMemory(*p_DeviceMemory);
    }

    return;
}

void mv::Device::createBuffer(vk::BufferUsageFlags p_BufferUsageFlags, vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                              mv::Buffer *p_MvBuffer, vk::DeviceSize p_DeviceSize, void *p_InitialData) const
{

    std::cout << "\t[-] Allocating buffer of size => " << static_cast<uint32_t>(p_DeviceSize) << std::endl;

    // create buffer
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.usage = p_BufferUsageFlags;
    bufferInfo.size = p_DeviceSize;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    p_MvBuffer->buffer = std::make_unique<vk::Buffer>(logicalDevice->createBuffer(bufferInfo));

    vk::MemoryRequirements memRequirements;
    vk::MemoryAllocateInfo allocInfo;

    memRequirements = logicalDevice->getBufferMemoryRequirements(*p_MvBuffer->buffer);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = get_memory_type(memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

    // allocate memory
    *p_MvBuffer->memory = logicalDevice->allocateMemory(allocInfo);

    p_MvBuffer->alignment = memRequirements.alignment;
    p_MvBuffer->size = p_DeviceSize;
    p_MvBuffer->usageFlags = p_BufferUsageFlags;
    p_MvBuffer->memoryPropertyFlags = p_MemoryPropertyFlags;

    // bind buffer & memory
    p_MvBuffer->bind(*this);

    p_MvBuffer->setup_descriptor();

    // copy if necessary
    if (p_InitialData != nullptr)
    {
        p_MvBuffer->map(*this);

        memcpy(p_MvBuffer->mapped, p_InitialData, p_DeviceSize);

        p_MvBuffer->unmap(*this);
    }

    return;
}