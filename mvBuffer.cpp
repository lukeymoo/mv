#include "mvBuffer.h"

// Maps vulkan buffer/memory to member variable void* mapped
void mv::Buffer::map(const vk::Device &l_dvc, vk::DeviceSize psize, vk::DeviceSize poffset) {
  mapped = l_dvc.mapMemory(*memory, poffset, psize);
  if (!mapped)
    throw std::runtime_error("Failed to map memory :: buffer handler");
  return;
}

// Unmaps void* mapped from vulkan buffer/memory
void mv::Buffer::unmap(const vk::Device &l_dvc) {
  if (mapped) {
    l_dvc.unmapMemory(*memory);
    mapped = nullptr;
  }
}

// Bind allocated memory to buffer
void mv::Buffer::bind(const vk::Device &l_dvc, vk::DeviceSize poffset) {

  l_dvc.bindBufferMemory(*buffer, *memory, poffset);

  return;
}

// Configure default descriptor values
void mv::Buffer::setup_descriptor(vk::DeviceSize psize, vk::DeviceSize poffset) {
  descriptor.buffer = *buffer;
  descriptor.range = psize;
  descriptor.offset = poffset;
  return;
}

// Copy buffer to another mapped buffer pointed to via function param void* data
void mv::Buffer::copy_from(void *data, vk::DeviceSize size) {
  if (!mapped)
    throw std::runtime_error(
        "Requested to copy data to buffer but the buffer was never mapped :: buffer handler");

  memcpy(mapped, data, size);

  return;
}

// Unmaps void* mapped if it was previously mapped to vulkan buffer/memory
// Destroys buffer and frees allocated memory
void mv::Buffer::destroy(const vk::Device &l_dvc) {

  unmap(l_dvc);

  if (buffer) {
    l_dvc.destroyBuffer(*buffer);
    buffer.reset();
  }

  if (memory) {
    l_dvc.freeMemory(*memory);
    memory.reset();
  }
}