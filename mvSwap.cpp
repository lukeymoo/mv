#include "mvSwap.h"

Swap::Swap(void)
{
}
Swap::~Swap()
{
}

void Swap::init(GLFWwindow *p_GLFWwindow, const vk::Instance &p_Instance,
                const vk::PhysicalDevice &p_PhysicalDevice)
{
    // create window
    VkSurfaceKHR tempSurface = nullptr;
    glfwCreateWindowSurface(p_Instance, p_GLFWwindow, nullptr, &tempSurface);

    // store surface
    surface = tempSurface;

    // get queue family properties
    std::vector<vk::QueueFamilyProperties> queueProperties =
        p_PhysicalDevice.getQueueFamilyProperties();
    if (queueProperties.size() < 1)
        throw std::runtime_error("No command queue families found on device");

    // iterate retrieved queues, look for present support
    std::vector<vk::Bool32> supportsPresent(queueProperties.size());
    for (uint32_t i = 0; i < queueProperties.size(); i++)
    {
        supportsPresent[i] = p_PhysicalDevice.getSurfaceSupportKHR(i, surface);
    }

    uint32_t tempGraphicsIdx = UINT32_MAX;
    uint32_t tempPresentIdx = UINT32_MAX;
    for (uint32_t i = 0; i < queueProperties.size(); i++)
    {
        // found graphics support
        if ((queueProperties[i].queueFlags & vk::QueueFlagBits::eGraphics))
        {
            // save index
            if (tempGraphicsIdx == UINT32_MAX)
            {
                tempGraphicsIdx = i;
            }
            // if also has present, we're done
            if (supportsPresent[i] == VK_TRUE)
            {
                tempPresentIdx = i;
                break;
            }
        }
    }
    // if none had graphics + present support
    // TODO
    // allow multi queue operations
    // for now we throw
    if (tempPresentIdx == UINT32_MAX)
        throw std::runtime_error(
            "No queue families supported both graphics and present");
    // for (uint32_t i = 0; i < queue_count; i++)
    // {
    //     if (supports_present[i] == VK_TRUE)
    //     {
    //         t_present_idx = i;
    //     }
    // }

    // failed to find queues
    if (tempGraphicsIdx == UINT32_MAX || tempPresentIdx == UINT32_MAX)
        throw std::runtime_error("Failed to find graphics AND present support");

    // save graphics queue index
    graphicsIndex = tempGraphicsIdx;

    // get supported surface formats
    std::vector<vk::SurfaceFormatKHR> surfaceFormats =
        p_PhysicalDevice.getSurfaceFormatsKHR(surface);

    if (surfaceFormats.size() < 1)
        throw std::runtime_error("No supported surface formats found");

    if ((surfaceFormats.size() == 1) &&
        (surfaceFormats.at(0).format == vk::Format::eUndefined))
    {
        colorFormat = vk::Format::eB8G8R8A8Unorm;
        colorSpace = surfaceFormats[0].colorSpace;
    }
    else
    {
        // iterate over the list of available surface format and
        // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
        bool found_B8G8R8A8_UNORM = false;
        for (auto &&format : surfaceFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Unorm)
            {
                colorFormat = format.format;
                colorSpace = format.colorSpace;
                found_B8G8R8A8_UNORM = true;
                break;
            }
        }

        // in case VK_FORMAT_B8G8R8A8_UNORM is not available
        // select the first available color format
        if (!found_B8G8R8A8_UNORM)
        {
            colorFormat = surfaceFormats[0].format;
            colorSpace = surfaceFormats[0].colorSpace;
        }
    }

    return;
}

void Swap::create(const vk::PhysicalDevice &p_PhysicalDevice,
                  const vk::Device &p_LogicalDevice, uint32_t &p_WindowWidth,
                  uint32_t &p_WindowHeight)
{
    // get surface capabilities
    vk::SurfaceCapabilitiesKHR capabilities;
    vk::Result res =
        p_PhysicalDevice.getSurfaceCapabilitiesKHR(surface, &capabilities);
    switch (res)
    {
        case vk::Result::eSuccess:
            break;
        default:
            throw std::runtime_error("Failed to get surface capabilities, does "
                                     "the window no longer exist?");
            break;
    }

    // get surface present modes
    std::vector<vk::PresentModeKHR> presentModes =
        p_PhysicalDevice.getSurfacePresentModesKHR(surface);

    if (presentModes.size() < 1)
        throw std::runtime_error("Failed to find any surface present modes");

    vk::PresentModeKHR selectedPresentMode = vk::PresentModeKHR::eImmediate;

    // ensure we have desired present mode
    bool has_mode = false;
    for (const auto &mode : presentModes)
    {
        if (mode == selectedPresentMode)
        {
            has_mode = true;
            break;
        }
    }

    // eFIFO guaranteed by spec
    if (!has_mode)
        selectedPresentMode = vk::PresentModeKHR::eFifo;

    // get the surface width|height reported by vulkan
    // store in our interface member variables for future use
    p_WindowWidth = capabilities.currentExtent.width;
    p_WindowHeight = capabilities.currentExtent.height;
    swapExtent.width = p_WindowWidth;
    swapExtent.height = p_WindowHeight;

    // determine number of swap images
    uint32_t requestedImageCount = capabilities.minImageCount + 2;
    // ensure not exceeding limit
    if ((capabilities.maxImageCount > 0) &&
        (requestedImageCount > capabilities.maxImageCount))
    {
        requestedImageCount = capabilities.maxImageCount;
    }

    // get transform
    vk::SurfaceTransformFlagBitsKHR selectedTransform =
        capabilities.currentTransform;
    if (capabilities.supportedTransforms &
        vk::SurfaceTransformFlagBitsKHR::eIdentity)
    {
        selectedTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    }
    else
    {
        selectedTransform = capabilities.currentTransform;
    }

    // default ignore alpha(A = 1.0f)
    vk::CompositeAlphaFlagBitsKHR compositeAlpha =
        vk::CompositeAlphaFlagBitsKHR::eOpaque;

    std::vector<vk::CompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
        vk::CompositeAlphaFlagBitsKHR::eInherit,
    };

    for (const auto &flag : compositeAlphaFlags)
    {
        if (capabilities.supportedCompositeAlpha & flag)
        {
            compositeAlpha = flag;
            break;
        }
    }

    vk::SwapchainCreateInfoKHR swapInfo;
    swapInfo.surface = surface;
    swapInfo.minImageCount = requestedImageCount;
    swapInfo.imageFormat = colorFormat;
    swapInfo.imageColorSpace = colorSpace;
    swapInfo.imageExtent = swapExtent;
    swapInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapInfo.preTransform = selectedTransform;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapInfo.queueFamilyIndexCount = 0;
    swapInfo.presentMode = selectedPresentMode;
    swapInfo.clipped = VK_TRUE;
    swapInfo.compositeAlpha = compositeAlpha;

    if (capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
    {
        swapInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
    }

    if (capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst)
    {
        swapInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    // create swap chain
    std::cout << "Logical Device => " << p_LogicalDevice << "\n";
    swapchain = p_LogicalDevice.createSwapchainKHR(swapInfo);
    if (!swapchain)
        throw std::runtime_error("Failed to create swapchain");

    // get swap chain images
    std::vector<vk::Image> tempImages;
    tempImages = p_LogicalDevice.getSwapchainImagesKHR(swapchain);

    if (tempImages.size() < 1)
        throw std::runtime_error("Failed to retreive any swap chain images");

    buffers.resize(tempImages.size());
    for (auto &buffer : buffers)
    {
        // move temp images into swapchain buffer<vector>
        buffer.image = std::move(tempImages[&buffer - &buffers[0]]);
        vk::ImageViewCreateInfo viewInfo;
        viewInfo.image = buffer.image;
        viewInfo.format = colorFormat;
        viewInfo.components = {
            vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eG,
            vk::ComponentSwizzle::eB,
            vk::ComponentSwizzle::eA,
        }; // vk::ComponentSwizzle::eIdentity;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.viewType = vk::ImageViewType::e2D;

        buffer.view = p_LogicalDevice.createImageView(viewInfo);
        std::cout << "Creating swap image view => " << buffer.view << "\n";
    }
    return;
}

void Swap::cleanup(const vk::Instance &p_Instance,
                   const vk::Device &p_LogicalDevice,
                   bool p_ShouldDestroySurface)
{

    if (swapchain)
    {
        if (!buffers.empty())
        {
            for (auto &swapBuffer : buffers)
            {
                p_LogicalDevice.destroyImageView(swapBuffer.view);
            }
        }
        p_LogicalDevice.destroySwapchainKHR(swapchain);
    }

    if (p_ShouldDestroySurface)
    {
        if (surface)
        {
            p_Instance.destroySurfaceKHR(surface);
        }
    }
    return;
}
