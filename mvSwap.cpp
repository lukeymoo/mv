#include "mvSwap.h"

void mv::Swap::init(Display *display, Window window, const vk::Instance &inst,
                    const vk::PhysicalDevice &p_dvc) {

  vk::XlibSurfaceCreateInfoKHR surface_info;
  surface_info.dpy = display;
  surface_info.window = window;

  // Create surface for vulkan
  surface = std::make_unique<vk::SurfaceKHR>(inst.createXlibSurfaceKHR(surface_info, nullptr));

  // get queue family properties
  std::vector<vk::QueueFamilyProperties> queue_properties = p_dvc.getQueueFamilyProperties();
  if (queue_properties.size() < 1)
    throw std::runtime_error("No command queue families found on device");

  // iterate retrieved queues, look for present support
  std::vector<vk::Bool32> supports_present(queue_properties.size());
  for (uint32_t i = 0; i < queue_properties.size(); i++) {
    supports_present[i] = p_dvc.getSurfaceSupportKHR(i, *surface);
  }

  uint32_t t_graphics_idx = UINT32_MAX;
  uint32_t t_present_idx = UINT32_MAX;
  for (uint32_t i = 0; i < queue_properties.size(); i++) {
    // found graphics support
    if ((queue_properties[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
      // save index
      if (t_graphics_idx == UINT32_MAX) {
        t_graphics_idx = i;
      }
      // if also has present, we're done
      if (supports_present[i] == VK_TRUE) {
        t_present_idx = i;
        break;
      }
    }
  }
  // if none had graphics + present support
  // TODO
  // allow multi queue operations
  // for now we throw
  if (t_present_idx == UINT32_MAX)
    throw std::runtime_error("No queue families supported both graphics and present");
  // for (uint32_t i = 0; i < queue_count; i++)
  // {
  //     if (supports_present[i] == VK_TRUE)
  //     {
  //         t_present_idx = i;
  //     }
  // }

  // failed to find queues
  if (t_graphics_idx == UINT32_MAX || t_present_idx == UINT32_MAX)
    throw std::runtime_error("Failed to find graphics AND present support");

  // save graphics queue index
  graphics_index = t_graphics_idx;

  // get supported surface formats
  std::vector<vk::SurfaceFormatKHR> surface_formats = p_dvc.getSurfaceFormatsKHR(*surface);

  if (surface_formats.size() < 1)
    throw std::runtime_error("No supported surface formats found");

  if ((surface_formats.size() == 1) && (surface_formats.at(0).format == vk::Format::eUndefined)) {
    color_format = vk::Format::eB8G8R8A8Unorm;
    color_space = surface_formats[0].colorSpace;
  } else {
    // iterate over the list of available surface format and
    // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
    bool found_B8G8R8A8_UNORM = false;
    for (auto &&surface_format : surface_formats) {
      if (surface_format.format == vk::Format::eB8G8R8A8Unorm) {
        color_format = surface_format.format;
        color_space = surface_format.colorSpace;
        found_B8G8R8A8_UNORM = true;
        break;
      }
    }

    // in case VK_FORMAT_B8G8R8A8_UNORM is not available
    // select the first available color format
    if (!found_B8G8R8A8_UNORM) {
      color_format = surface_formats[0].format;
      color_space = surface_formats[0].colorSpace;
    }
  }

  return;
}

void mv::Swap::create(const vk::PhysicalDevice &p_dvc, const mv::Device &m_dvc, uint32_t &w,
                      uint32_t &h) {

  // get surface capabilities
  vk::SurfaceCapabilitiesKHR capabilities;
  vk::Result res = p_dvc.getSurfaceCapabilitiesKHR(*surface, &capabilities);
  switch (res) {
    case vk::Result::eSuccess:
      break;
    default:
      throw std::runtime_error(
          "Failed to get surface capabilities, does the window no longer exist?");
      break;
  }

  // get surface present modes
  std::vector<vk::PresentModeKHR> present_modes = p_dvc.getSurfacePresentModesKHR(*surface);

  if (present_modes.size() < 1)
    throw std::runtime_error("Failed to find any surface present modes");

  vk::PresentModeKHR selected_present_mode = vk::PresentModeKHR::eImmediate;

  // ensure we have desired present mode
  bool has_mode = false;
  for (const auto &mode : present_modes) {
    if (mode == selected_present_mode) {
      has_mode = true;
      break;
    }
  }

  // eFIFO guaranteed by spec
  if (!has_mode)
    selected_present_mode = vk::PresentModeKHR::eFifo;

  // get the surface width|height reported by vulkan
  // store in our interface member variables for future use
  w = capabilities.currentExtent.width;
  h = capabilities.currentExtent.height;
  swap_extent.width = w;
  swap_extent.height = h;

  // Window rw;
  // int rx, ry;           // returned x/y
  // uint rwidth, rheight; // returned width/height
  // uint rborder;         // returned  border width
  // uint rdepth;

  // // Prefer to configure extent from a direct response from x server
  // if (XGetGeometry(display,
  //                  window, &rw, &rx, &ry,
  //                  &rwidth, &rheight, &rborder, &rdepth))
  // {
  //     *w = static_cast<uint32_t>(rwidth);
  //     *h = static_cast<uint32_t>(rheight);
  //     swap_extent.width = *w;
  //     swap_extent.height = *h;
  //     std::cout << "[+] Swap extent configured by x server response" << std::endl
  //               << "\tWidth -> " << *w << std::endl
  //               << "\tHeight-> " << *h << std::endl;
  // }
  // else // Use the extent given by vulkan surface capabilities check earlier
  // {
  //     std::cout << "[+] Swap extent defaulting to vulkan reported extent" << std::endl
  //               << "Width -> " << *w << std::endl
  //               << "Height-> " << *h << std::endl;
  // }

  // determine number of swap images
  uint32_t requested_image_count = capabilities.minImageCount + 2;
  // ensure not exceeding limit
  if ((capabilities.maxImageCount > 0) && (requested_image_count > capabilities.maxImageCount)) {
    requested_image_count = capabilities.maxImageCount;
  }

  // get transform
  vk::SurfaceTransformFlagBitsKHR selected_transform = capabilities.currentTransform;
  if (capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
    selected_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
  } else {
    selected_transform = capabilities.currentTransform;
  }

  // default ignore alpha(A = 1.0f)
  vk::CompositeAlphaFlagBitsKHR composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

  std::vector<vk::CompositeAlphaFlagBitsKHR> composite_alpha_flags = {
      vk::CompositeAlphaFlagBitsKHR::eOpaque,
      vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
      vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
      vk::CompositeAlphaFlagBitsKHR::eInherit,
  };

  for (const auto &flag : composite_alpha_flags) {
    if (capabilities.supportedCompositeAlpha & flag) {
      composite_alpha = flag;
      break;
    }
  }

  vk::SwapchainCreateInfoKHR swap_ci;
  swap_ci.surface = *surface;
  swap_ci.minImageCount = requested_image_count;
  swap_ci.imageFormat = color_format;
  swap_ci.imageColorSpace = color_space;
  swap_ci.imageExtent = swap_extent;
  swap_ci.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  swap_ci.preTransform = selected_transform;
  swap_ci.imageArrayLayers = 1;
  swap_ci.imageSharingMode = vk::SharingMode::eExclusive;
  swap_ci.queueFamilyIndexCount = 0; // only matters when concurrent sharing mode
  swap_ci.presentMode = selected_present_mode;
  swap_ci.clipped = VK_TRUE;
  swap_ci.compositeAlpha = composite_alpha;

  if (capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) {
    swap_ci.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
  }

  if (capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst) {
    swap_ci.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
  }

  // create swap chain
  swapchain = std::make_unique<vk::SwapchainKHR>(m_dvc.logical_device->createSwapchainKHR(swap_ci));

  // get swap chain images
  *images = m_dvc.logical_device->getSwapchainImagesKHR(*swapchain);

  if (images->size() < 1)
    throw std::runtime_error("Failed to retreive any swap chain images");

  // create our swapchain buffer objs representing swap image & swap image view
  buffers->resize(images->size());
  for (size_t i = 0; i < images->size(); i++) {
    vk::ImageViewCreateInfo view_ci;
    view_ci.format = color_format;
    view_ci.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                          vk::ComponentSwizzle::eB,
                          vk::ComponentSwizzle::eA}; // vk::ComponentSwizzle::eIdentity;
    view_ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount = 1;
    view_ci.viewType = vk::ImageViewType::e2D;

    // copy image handle to our swapchain buffer obj
    buffers->at(i).image = images->at(i);
    view_ci.image = buffers->at(i).image;
    // create image view
    buffers->at(i).view = m_dvc.logical_device->createImageView(view_ci);
  }
  return;
}

void mv::Swap::cleanup(const vk::Instance &inst, const mv::Device &m_dvc,
                       bool should_destroy_surface) {

  if (swapchain) {
    if (buffers) {
      for (auto &swap_buffer : *buffers) {
        m_dvc.logical_device->destroyImageView(swap_buffer.view);
      }
      buffers.reset();
      buffers = std::make_unique<std::vector<mv::swapchain_buffer>>();
    }
    m_dvc.logical_device->destroySwapchainKHR(*swapchain);
    swapchain.reset();
    swapchain = std::make_unique<vk::SwapchainKHR>();
  }

  if (should_destroy_surface) {
    if (surface) {
      inst.destroySurfaceKHR(*surface);
      surface.reset();
      surface = std::make_unique<vk::SurfaceKHR>();
    }
  }
  return;
}

// void mv::Swap::map(std::weak_ptr<vk::Instance> ins, std::weak_ptr<vk::PhysicalDevice> pdvc,
//                    std::weak_ptr<mv::Device> mdvc) {
//   // validate params
//   auto inst = ins.lock();
//   auto p_dvc = pdvc.lock();
//   auto m_dvc = mdvc.lock();

//   if (!inst)
//     throw std::runtime_error(
//         "invalid instance handle passed to swap chain, mapping :: swap handler");

//   if (!p_dvc)
//     throw std::runtime_error(
//         "invalid physical device handle passed to swap chain, mapping :: swap handler");

//   if (!m_dvc)
//     throw std::runtime_error(
//         "invalid mv device handle passed to swap chain, mapping :: swap handler");

//   this->instance = ins;
//   this->physical_device = pdvc;
//   this->mv_device = mdvc;
//   return;
// }