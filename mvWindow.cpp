#include "mvWindow.h"

void mv::MWindow::prepare(void) {
  // creates...
  // physical device
  // logical device
  // swapchain surface
  // passes... vulkan handles to swapchain handler
  init_vulkan();

  swapchain->init(window, *instance, *physical_device);
  command_pool =
      std::make_unique<vk::CommandPool>(mv_device->create_command_pool(swapchain->graphics_index));

  swapchain->create(*physical_device, *mv_device, window_width, window_height);

  create_command_buffers();

  create_synchronization_primitives();

  setup_depth_stencil();

  setup_render_pass();

  // create_pipeline_cache();

  setup_framebuffer();

  // load device extension functions
  pfn_vkCmdSetPrimitiveTopology = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(
      vkGetInstanceProcAddr(*instance, "vkCmdSetPrimitiveTopologyEXT"));
  if (!pfn_vkCmdSetPrimitiveTopology)
    throw std::runtime_error("Failed to load extended dynamic state extensions");
  return;
}

void mv::MWindow::init_vulkan(void) {
  // creates vulkan instance with specified instance extensions/layers
  create_instance();

  std::vector<vk::PhysicalDevice> physical_devices = instance->enumeratePhysicalDevices();

  if (physical_devices.size() < 1)
    throw std::runtime_error("No physical devices found");

  // Select device
  physical_device = std::make_unique<vk::PhysicalDevice>(std::move(physical_devices.at(0)));
  // resize devices container
  physical_devices.erase(physical_devices.begin());

  // get device info
  physical_properties = physical_device->getProperties();
  physical_features = physical_device->getFeatures();
  physical_memory_properties = physical_device->getMemoryProperties();

  std::vector<std::string> tmp;
  for (const auto &l : requested_device_extensions) {
    std::cout << "\t[-] Requesting device extension => " << l << "\n";
    tmp.push_back(l);
  }

  // create logical device handler mv::Device
  mv_device = std::make_unique<mv::Device>(*physical_device, tmp);

  // create logical device & graphics queue
  mv_device->create_logical_device(*physical_device);

  // get format
  depth_format = mv_device->get_supported_depth_format(*physical_device);

  // no longer pass references to swapchain, pass reference on per function basis now
  // swapchain->map(std::weak_ptr<vk::Instance>(instance),
  //                std::weak_ptr<vk::PhysicalDevice>(physical_device),
  //                std::weak_ptr<mv::Device>(mv_device));

  // Create synchronization objects
  vk::SemaphoreCreateInfo semaphore_info;
  semaphores->present_complete = mv_device->logical_device->createSemaphore(semaphore_info);
  semaphores->render_complete = mv_device->logical_device->createSemaphore(semaphore_info);

  return;
}

void mv::MWindow::create_instance(void) {
  // Ensure we have validation layers
#ifndef NDEBUG
  check_validation_support();
#endif

  // Ensure we have all requested instance extensions
  check_instance_ext();

  vk::ApplicationInfo app_info;
  app_info.pApplicationName = "Bloody Day";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "Moogin";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_MAKE_VERSION(1, 2, 0);

/* If debugging enabled */
#ifndef NDEBUG
  vk::DebugUtilsMessengerCreateInfoEXT debugger_settings{};
  debugger_settings.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
  debugger_settings.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
  debugger_settings.pfnUserCallback = debug_message_processor;
  debugger_settings.pUserData = nullptr;

  // convert string request to const char*
  std::vector<const char *> req_layer;
  for (auto &layer : requested_validation_layers) {
    std::cout << "\t[-] Requesting layer => " << layer << "\n";
    req_layer.push_back(layer);
  }
  std::vector<const char *> req_inst_ext;
  for (auto &ext : f_req) {
    std::cout << "\t[-] Requesting instance extension => " << ext << "\n";
    req_inst_ext.push_back(ext.c_str());
  }

  vk::InstanceCreateInfo create_info;
  create_info.pNext = &debugger_settings;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledLayerCount = static_cast<uint32_t>(req_layer.size());
  create_info.ppEnabledLayerNames = req_layer.data();
  create_info.enabledExtensionCount = static_cast<uint32_t>(req_inst_ext.size());
  create_info.ppEnabledExtensionNames = req_inst_ext.data();
#endif
#ifdef NDEBUG /* Debugging disabled */
  std::vector<const char *> req_inst_ext;
  for (auto &ext : f_req) {
    std::cout << "\t[-] Requesting instance extension => " << ext << "\n";
    req_inst_ext.push_back(ext.c_str());
  }

  vk::InstanceCreateInfo create_info = {};
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = static_cast<uint32_t>(req_inst_ext.size());
  create_info.ppEnabledExtensionNames = req_inst_ext.data();
#endif

  instance = std::make_unique<vk::Instance>(vk::createInstance(create_info));
  // double check instance(prob a triple check at this point)
  if (!*instance)
    throw std::runtime_error("Failed to create vulkan instance");

  return;
}

void mv::MWindow::create_command_buffers(void) {
  command_buffers->resize(swapchain->images->size());

  vk::CommandBufferAllocateInfo alloc_info;
  alloc_info.commandPool = *command_pool;
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers->size());

  *command_buffers = mv_device->logical_device->allocateCommandBuffers(alloc_info);

  if (command_buffers->size() < 1)
    throw std::runtime_error("Failed to allocate command buffers");
  return;
}

void mv::MWindow::create_synchronization_primitives(void) {
  vk::FenceCreateInfo fence_info;
  fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

  wait_fences->resize(swapchain->images->size(), nullptr);
  in_flight_fences->resize(MAX_IN_FLIGHT);

  for (auto &fence : *in_flight_fences) {
    fence = mv_device->logical_device->createFence(fence_info);
  }
  return;
}

void mv::MWindow::setup_depth_stencil(void) {
  vk::ImageCreateInfo image_ci;
  image_ci.imageType = vk::ImageType::e2D;
  image_ci.format = depth_format;
  image_ci.extent = vk::Extent3D{window_width, window_height, 1};
  image_ci.mipLevels = 1;
  image_ci.arrayLayers = 1;
  image_ci.samples = vk::SampleCountFlagBits::e1;
  image_ci.tiling = vk::ImageTiling::eOptimal;
  image_ci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

  // create depth stencil testing image
  depth_stencil->image = mv_device->logical_device->createImage(image_ci);

  // Allocate memory for image
  vk::MemoryRequirements mem_req;
  mem_req = mv_device->logical_device->getImageMemoryRequirements(depth_stencil->image);

  vk::MemoryAllocateInfo alloc_info;
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex =
      mv_device->get_memory_type(mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

  // allocate depth stencil image memory
  depth_stencil->mem = mv_device->logical_device->allocateMemory(alloc_info);

  // bind image and memory
  mv_device->logical_device->bindImageMemory(depth_stencil->image, depth_stencil->mem, 0);

  // Create view into depth stencil testing image
  vk::ImageViewCreateInfo iv_info;
  iv_info.viewType = vk::ImageViewType::e2D;
  iv_info.image = depth_stencil->image;
  iv_info.format = depth_format;
  iv_info.subresourceRange.baseMipLevel = 0;
  iv_info.subresourceRange.levelCount = 1;
  iv_info.subresourceRange.baseArrayLayer = 0;
  iv_info.subresourceRange.layerCount = 1;
  iv_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

  // if physical device supports high enough format add stenciling
  if (depth_format >= vk::Format::eD16UnormS8Uint) {
    iv_info.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
  }

  depth_stencil->view = mv_device->logical_device->createImageView(iv_info);
  return;
}

void mv::MWindow::setup_render_pass(void) {
  std::array<vk::AttachmentDescription, 2> attachments;
  // Color attachment
  attachments[0].format = swapchain->color_format;
  attachments[0].samples = vk::SampleCountFlagBits::e1;
  attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
  attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachments[0].initialLayout = vk::ImageLayout::eUndefined;
  attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

  // Depth attachment
  attachments[1].format = depth_format;
  attachments[1].samples = vk::SampleCountFlagBits::e1;
  attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
  attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
  attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachments[1].initialLayout = vk::ImageLayout::eUndefined;
  attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::AttachmentReference color_ref;
  color_ref.attachment = 0;
  color_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::AttachmentReference depth_ref;
  depth_ref.attachment = 1;
  depth_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::SubpassDescription subpass_desc;
  subpass_desc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass_desc.colorAttachmentCount = 1;
  subpass_desc.pColorAttachments = &color_ref;
  subpass_desc.pDepthStencilAttachment = &depth_ref;
  subpass_desc.inputAttachmentCount = 0;
  subpass_desc.pInputAttachments = nullptr;
  subpass_desc.preserveAttachmentCount = 0;
  subpass_desc.pPreserveAttachments = nullptr;
  subpass_desc.pResolveAttachments = nullptr;

  std::array<vk::SubpassDependency, 2> dependencies;
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
  dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
  dependencies[0].dstAccessMask =
      vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
  dependencies[1].srcAccessMask =
      vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
  dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

  vk::RenderPassCreateInfo render_pass_info;
  render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  render_pass_info.pAttachments = attachments.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass_desc;
  render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
  render_pass_info.pDependencies = dependencies.data();

  render_pass = std::make_unique<vk::RenderPass>(
      mv_device->logical_device->createRenderPass(render_pass_info));
  return;
}

// TODO
// Re add call to this method
// void mv::MWindow::create_pipeline_cache(void)
// {
//     vk::PipelineCacheCreateInfo pcinfo;
//     pipeline_cache =
//     std::make_shared<vk::PipelineCache>(mv_device->logical_device->createPipelineCache(pcinfo));
//     return;
// }

void mv::MWindow::setup_framebuffer(void) {
  std::array<vk::ImageView, 2> attachments;

  // each frame buffer uses same depth image
  attachments[1] = depth_stencil->view;

  vk::FramebufferCreateInfo framebuffer_info;
  framebuffer_info.renderPass = *render_pass;
  framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebuffer_info.pAttachments = attachments.data();
  framebuffer_info.width = swapchain->swap_extent.width;
  framebuffer_info.height = swapchain->swap_extent.height;
  framebuffer_info.layers = 1;

  // Framebuffer per swap image
  frame_buffers->resize(static_cast<uint32_t>(swapchain->images->size()));
  for (size_t i = 0; i < frame_buffers->size(); i++) {
    // different image views for each frame buffer
    attachments[0] = swapchain->buffers->at(i).view;
    frame_buffers->at(i) = mv_device->logical_device->createFramebuffer(framebuffer_info);
  }
  return;
}

void mv::MWindow::check_validation_support(void) {
  std::vector<vk::LayerProperties> inst_layers = vk::enumerateInstanceLayerProperties();

  if (inst_layers.size() == 0 && !requested_validation_layers.empty())
    throw std::runtime_error("No supported validation layers found");

  // look for missing requested layers
  std::string prelude = "The following instance layers were not found...\n";
  std::string failed;
  for (const auto &requested_layer : requested_validation_layers) {
    bool match = false;
    for (const auto &layer : inst_layers) {
      if (strcmp(requested_layer, layer.layerName) == 0) {
        match = true;
        break;
      }
    }
    if (!match) {
      failed += requested_layer;
      failed += "\n";
    }
  }

  // report missing layers if necessary
  if (!failed.empty()) {
    throw std::runtime_error(prelude + failed);
  } else {
    std::cout << "[+] System supports all requested validation layers" << std::endl;
  }
  return;
}

void mv::MWindow::check_instance_ext(void) {
  std::vector<vk::ExtensionProperties> inst_extensions = vk::enumerateInstanceExtensionProperties();

  // use f_req vector for instance extensions
  if (inst_extensions.size() < 1 && !f_req.empty())
    throw std::runtime_error("No instance extensions found");

  std::string prelude = "The following instance extensions were not found...\n";
  std::string failed;
  for (const auto &requested_extension : f_req) {
    bool match = false;
    for (const auto &available_extension : inst_extensions) {
      if (strcmp(requested_extension.c_str(), available_extension.extensionName) == 0) {
        match = true;
        break;
      }
    }
    if (!match) {
      failed += requested_extension;
      failed += "\n";
    }
  }

  if (!failed.empty()) {
    throw std::runtime_error(prelude + failed);
  }

  return;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_message_processor(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, [[maybe_unused]] void *user_data) {
  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::ostringstream oss;
    oss << std::endl
        << "Warning: " << callback_data->messageIdNumber << ", " << callback_data->pMessageIdName
        << std::endl
        << callback_data->pMessage << std::endl
        << std::endl;
    std::cout << oss.str();
  } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT &&
             (1 == 2)) // skip verbose messages
  {
    // Disabled by the impossible statement
    std::ostringstream oss;
    oss << std::endl
        << "Verbose message : " << callback_data->messageIdNumber << ", "
        << callback_data->pMessageIdName << std::endl
        << callback_data->pMessage << std::endl
        << std::endl;
    std::cout << oss.str();
  } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    std::ostringstream oss;
    oss << std::endl
        << "Error: " << callback_data->messageIdNumber << ", " << callback_data->pMessageIdName
        << std::endl
        << callback_data->pMessage << std::endl
        << std::endl;
    std::cout << oss.str();
  } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    std::ostringstream oss;
    oss << std::endl
        << "Info: " << callback_data->messageIdNumber << ", " << callback_data->pMessageIdName
        << std::endl
        << callback_data->pMessage << std::endl
        << std::endl;
  }
  return false;
}

std::vector<char> mv::MWindow::read_file(std::string filename) {
  size_t file_size;
  std::ifstream file;
  std::vector<char> buffer;

  // check if file exists
  try {
    std::filesystem::exists(filename);
    file.open(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      std::ostringstream oss;
      oss << "Failed to open file " << filename;
      throw std::runtime_error(oss.str());
    }

    // prepare buffer to hold shader bytecode
    file_size = (size_t)file.tellg();
    buffer.resize(file_size);

    // go back to beginning of file and read in
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();
  } catch (std::filesystem::filesystem_error &e) {
    std::ostringstream oss;
    oss << "Filesystem Exception : " << e.what() << std::endl;
    throw std::runtime_error(oss.str());
  } catch (std::exception &e) {
    std::ostringstream oss;
    oss << "Standard Exception : " << e.what();
    throw std::runtime_error(oss.str());
  } catch (...) {
    throw std::runtime_error("Unhandled exception while loading a file");
  }

  if (buffer.empty()) {
    throw std::runtime_error("File reading operation returned empty buffer :: shaders?");
  }

  return buffer;
}

vk::ShaderModule mv::MWindow::create_shader_module(const std::vector<char> &code) {
  vk::ShaderModuleCreateInfo module_info;
  module_info.codeSize = code.size();
  module_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

  vk::ShaderModule module = mv_device->logical_device->createShaderModule(module_info);

  return module;
}


// static XEvent create_event(Display *display, Window window, const char *event_type) {
//   XEvent cev;

//   cev.xclient.type = ClientMessage;
//   cev.xclient.window = window;
//   cev.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
//   cev.xclient.format = 32;
//   cev.xclient.data.l[0] = XInternAtom(display, event_type, false);
//   cev.xclient.data.l[1] = CurrentTime;

//   return cev;
// }

// void mv::MWindow::handle_x_event(void) {
//   XEvent event;
//   XNextEvent(*display, &event);
//   mv::keyboard::key mv_key = mv::keyboard::key::invalid;

//   // query if xinput
//   if (event.xcookie.type == GenericEvent && event.xcookie.extension == xinput_opcode) {
//     // if raw motion, query pointer
//     XGetEventData(*display, &event.xcookie);

//     XGenericEventCookie *gen_cookie = &event.xcookie;
//     XIDeviceEvent *xi_device_evt = static_cast<XIDeviceEvent *>(gen_cookie->data);
//     // retrieve mouse deltas
//     if (event.xcookie.evtype == XI_RawMotion) {
//       mouse->process_device_event(xi_device_evt);
//     }
//     XFreeEventData(*display, &event.xcookie);
//   }

//   switch (event.type) {
//     case LeaveNotify:
//     case FocusOut:
//       {
//         mouse->on_mouse_leave();
//         // XUngrabPointer(*display, CurrentTime);
//         break;
//       }
//     case MapNotify:
//     case EnterNotify:
//       {
//         mouse->on_mouse_enter();
//         // confine cursor to interior of window
//         // mouse released on focus out or cursor leaving window
//         // XGrabPointer(*display, window, 1, 0, GrabModeAsync, GrabModeAsync, window, None,
//         //              CurrentTime);
//         break;
//       }
//     case ButtonPress:
//       {
//         if (event.xbutton.button == Button1) {
//           mouse->on_left_press(event.xbutton.x, event.xbutton.y);
//         } else if (event.xbutton.button == Button2) {
//           mouse->on_middle_press(event.xbutton.x, event.xbutton.y);
//         } else if (event.xbutton.button == Button3) {
//           mouse->on_right_press(event.xbutton.x, event.xbutton.y);
//         }
//         // Mouse wheel scroll up
//         else if (event.xbutton.button == Button4) {
//           mouse->on_wheel_up(event.xbutton.x, event.xbutton.y);
//         }
//         // Mouse wheel scroll down
//         else if (event.xbutton.button == Button5) {
//           mouse->on_wheel_down(event.xbutton.x, event.xbutton.y);
//         }
//         break;
//       }
//     case ButtonRelease:
//       {
//         if (event.xbutton.button == Button1) {
//           mouse->on_left_release(event.xbutton.x, event.xbutton.y);
//         } else if (event.xbutton.button == Button2) {
//           mouse->on_middle_release(event.xbutton.x, event.xbutton.y);
//         } else if (event.xbutton.button == Button3) {
//           mouse->on_right_release(event.xbutton.x, event.xbutton.y);
//         }
//         break;
//       }
//     case KeyPress:
//       { // try to convert to our mv::keyboard::key enum
//         mv_key = kbd->keycode_to_mvkey(*display, event.xkey.keycode);

//         // check quit case
//         if (mv_key == mv::keyboard::key::escape) {
//           XEvent q = create_event(*display, window, "WM_DELETE_WINDOW");
//           XSendEvent(*display, window, false, ExposureMask, &q);
//         }

//         if (mv_key) {
//           // send event only if not already signaled in key_states bitset
//           if (!is_keystate(mv_key)) {
//             kbd->on_key_press(mv_key);
//           }
//         }
//         break;
//       }
//     case KeyRelease:
//       {
//         mv_key = kbd->keycode_to_mvkey(*display, event.xkey.keycode);

//         if (mv_key) {
//           // x11 configured to not send repeat releases with xkb method in
//           // constructor
//           // so just process it
//           kbd->on_key_release(mv_key);
//         }
//         break;
//       }
//     case Expose:
//       {
//         break;
//       }
//     case ClientMessage:
//       {
//         std::cout << "[+] Exit requested" << std::endl;
//         running = false;
//         break;
//       }
//     default: // Unhandled events do nothing
//       {
//         break;
//       }
//   } // End of switch
//   return;
// }