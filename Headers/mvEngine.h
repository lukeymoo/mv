#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <chrono>
#include <random>
#include <unordered_map>

#include "mvCamera.h"
#include "mvGui.h"
#include "mvMap.h"
#include "mvTimer.h"
#include "mvWindow.h"

class Allocator;
struct Collection;
class Container;
class MvBuffer;

using namespace std::chrono_literals;

class Engine : public Window
{
public:
  Engine (int w, int h, const char *title);
  ~Engine ();

  // delete copy
  Engine &operator= (const Engine &) = delete;
  Engine (const Engine &) = delete;

  Timer timer;
  Timer fps;

  Camera camera;                                 // camera manager(view/proj matrix handler)
  MapHandler mapHandler;                         // Map related methods
  std::unique_ptr<Allocator> allocator;          // descriptor pool/set manager
  std::unique_ptr<Collection> collectionHandler; // model/obj manager
  std::unique_ptr<GuiHandler> gui;               // ImGui manager

  std::unordered_map<PipelineTypes, vk::Pipeline> pipelines;
  std::unordered_map<PipelineTypes, vk::PipelineLayout> pipelineLayouts;

  PipelineTypes currentlyBound;

  void addNewModel (Container *pool, const char *filename);

  void recreateSwapchain (void);

  void go (void);
  // all the initial descriptor allocator & collection handler calls
  inline void goSetup (void);
  void recordCommandBuffer (uint32_t imageIndex);
  void draw (size_t &current_frame, uint32_t &current_image_index);

  // create buffer with Vulkan objects
  void createBuffer (vk::BufferUsageFlags p_BufferUsageFlags,
                     vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                     vk::DeviceSize p_DeviceSize,
                     vk::Buffer *p_VkBuffer,
                     vk::DeviceMemory *p_DeviceMemory,
                     void *p_InitialData = nullptr) const;

  // create buffer with custom Buffer interface
  void createBuffer (vk::BufferUsageFlags p_BufferUsageFlags,
                     vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                     MvBuffer *p_MvBuffer,
                     vk::DeviceSize p_DeviceSize,
                     void *p_InitialData = nullptr) const;

  void createStagingBuffer (vk::DeviceSize &p_BufferSize,
                            vk::Buffer &p_StagingBuffer,
                            vk::DeviceMemory &p_StagingMemory) const;

  void endCommandBuffer (const vk::CommandPool &p_CommandPool,
                         vk::CommandBuffer p_CommandBuffer) const;

protected:
  void prepareUniforms (void);

  void prepareLayouts (void);

  void prepareGraphicsPipelines (void);

  void prepareComputePipelines (void);

  void computePass (uint32_t p_ImageIndex);

  void cleanupSwapchain (void);
};
