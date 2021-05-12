#include "mvDevice.h"

void mv::Device::create_logical_device(const vk::PhysicalDevice &p_dvc) {
  std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;

  constexpr float def_queue_prio = 0.0f;

  // Look for graphics queue
  queue_idx.graphics = get_queue_family_index(vk::QueueFlagBits::eGraphics);

  vk::DeviceQueueCreateInfo queue_info;
  queue_info.queueCount = 1;
  queue_info.queueFamilyIndex = queue_idx.graphics;
  queue_info.pQueuePriorities = &def_queue_prio;

  queue_create_infos.push_back(queue_info);

  std::vector<const char *> e_ext;
  for (const auto &req : requested_physical_device_exts) {
    e_ext.push_back(req.c_str());
  }
  vk::DeviceCreateInfo device_create_info;
  device_create_info.pNext = &physical_features2;
  device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
  device_create_info.pQueueCreateInfos = queue_create_infos.data();

  // if pnext is phys features2, must be nullptr
  // device_create_info.pEnabledFeatures = &physical_features;

  device_create_info.enabledExtensionCount = static_cast<uint32_t>(e_ext.size());
  device_create_info.ppEnabledExtensionNames = e_ext.data();

  // create logical device
  logical_device = std::make_unique<vk::Device>(p_dvc.createDevice(device_create_info));

  // Create command pool with graphics queue family
  command_pool = std::make_unique<vk::CommandPool>(create_command_pool(queue_idx.graphics));

  // retreive graphics queue
  graphics_queue = std::make_unique<vk::Queue>(logical_device->getQueue(queue_idx.graphics, 0));

  return;
}

uint32_t mv::Device::get_queue_family_index(vk::QueueFlagBits queue_flag_bit) const {
  for (const auto &queue_property : queue_family_properties) {
    if ((queue_property.queueFlags & queue_flag_bit)) {
      return (&queue_property - &queue_family_properties[0]);
    }
  }

  throw std::runtime_error("Could not find requested queue family");
}

vk::CommandPool mv::Device::create_command_pool(uint32_t queue_index,
                                                vk::CommandPoolCreateFlags create_flags) {
  vk::CommandPoolCreateInfo pool_info;
  pool_info.flags = create_flags;
  pool_info.queueFamilyIndex = queue_index;

  return logical_device->createCommandPool(pool_info);
}

uint32_t mv::Device::get_memory_type(uint32_t type_bits, vk::MemoryPropertyFlags properties,
                                     vk::Bool32 *mem_type_found) const {
  for (uint32_t i = 0; i < physical_memory_properties.memoryTypeCount; i++) {
    if ((type_bits & 1) == 1) {
      if ((physical_memory_properties.memoryTypes[i].propertyFlags & properties)) {
        if (mem_type_found) {
          *mem_type_found = true;
        }
        return i;
      }
    }
    type_bits >>= 1;
  }

  if (mem_type_found) {
    *mem_type_found = false;
    return 0;
  } else {
    throw std::runtime_error("Could not find a matching memory type");
  }
}

vk::Format mv::Device::get_supported_depth_format(const vk::PhysicalDevice &p_dvc) {

  std::vector<vk::Format> depth_formats = {vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat,
                                           vk::Format::eD24UnormS8Uint, vk::Format::eD16UnormS8Uint,
                                           vk::Format::eD16Unorm};

  for (auto &format : depth_formats) {
    vk::FormatProperties format_properties = p_dvc.getFormatProperties(format);
    if (format_properties.optimalTilingFeatures &
        vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
      if (!(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)) {
        continue;
      }
    }
    return format;
  }
  throw std::runtime_error("Failed to find good format");
}

void mv::Device::create_buffer(vk::BufferUsageFlags usage_flags,
                               vk::MemoryPropertyFlags memory_property_flags, vk::DeviceSize size,
                               vk::Buffer *buffer, vk::DeviceMemory *memory, void *data) const {
  std::cout << "\t[-] Allocating buffer of size => " << static_cast<uint32_t>(size) << std::endl;
  vk::BufferCreateInfo buffer_info;
  buffer_info.usage = usage_flags;
  buffer_info.size = size;
  buffer_info.sharingMode = vk::SharingMode::eExclusive;

  // create vulkan buffer
  *buffer = logical_device->createBuffer(buffer_info);

  // allocate memory for buffer
  vk::MemoryRequirements memreqs;
  vk::MemoryAllocateInfo alloc_info;

  memreqs = logical_device->getBufferMemoryRequirements(*buffer);

  alloc_info.allocationSize = memreqs.size;
  alloc_info.memoryTypeIndex = get_memory_type(memreqs.memoryTypeBits, memory_property_flags);

  // Allocate memory
  *memory = logical_device->allocateMemory(alloc_info);

  // Bind newly allocated memory to buffer
  logical_device->bindBufferMemory(*buffer, *memory, 0);

  // If data was passed to creation, load it
  if (data != nullptr) {
    void *mapped = logical_device->mapMemory(*memory, 0, size);

    memcpy(mapped, data, size);

    logical_device->unmapMemory(*memory);
  }

  return;
}

void mv::Device::create_buffer(vk::BufferUsageFlags usage_flags,
                               vk::MemoryPropertyFlags memory_property_flags, mv::Buffer *buffer,
                               vk::DeviceSize size, void *data) const {

  std::cout << "\t[-] Allocating buffer of size => " << static_cast<uint32_t>(size) << std::endl;

  // create buffer
  vk::BufferCreateInfo buffer_info;
  buffer_info.usage = usage_flags;
  buffer_info.size = size;
  buffer_info.sharingMode = vk::SharingMode::eExclusive;

  buffer->buffer = std::make_unique<vk::Buffer>(logical_device->createBuffer(buffer_info));

  vk::MemoryRequirements memreqs;
  vk::MemoryAllocateInfo alloc_info;

  memreqs = logical_device->getBufferMemoryRequirements(*buffer->buffer);

  alloc_info.allocationSize = memreqs.size;
  alloc_info.memoryTypeIndex = get_memory_type(memreqs.memoryTypeBits, memory_property_flags);

  // allocate memory
  *buffer->memory = logical_device->allocateMemory(alloc_info);

  buffer->alignment = memreqs.alignment;
  buffer->size = size;
  buffer->usage_flags = usage_flags;
  buffer->memory_property_flags = memory_property_flags;

  // bind buffer & memory
  buffer->bind(*this);

  buffer->setup_descriptor();

  // copy if necessary
  if (data != nullptr) {
    buffer->map(*this);

    memcpy(buffer->mapped, data, size);

    buffer->unmap(*this);
  }

  return;
}