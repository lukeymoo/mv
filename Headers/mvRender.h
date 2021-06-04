#pragma once

#include <vulkan/vulkan.hpp>

#include <memory>
#include <unordered_map>

#include "mvImage.h"
#include "mvMisc.h"

namespace mv
{
  enum FramebufferType
  {
    eCoreFramebuffer = 0,
    eImGuiFramebuffer,
  };

  // Render pass defines
  enum RenderPassType
  {
    eCore = 0,
    eImGui,
  };

  // Containers for pipelines
  enum PipelineType
  {
    eMVPWSampler = 0, // M/V/P uniforms + color, uv, sampler
    eMVPNoSampler,    // M/V/P uniforms + color, uv

    eVPWSampler,  // V/P uniforms + color, uv, sampler
    eVPNoSampler, // V/P uniforms + color, uv

    // dynamic
    eMVPWSamplerDynamic,  // M/V/P uniforms + color, uv, sampler & dynamic states
    eMVPNoSamplerDynamic, // M/V/P uniforms + color, uv & dynamic states
    eVPSamplerDynamic,    // V/P uniforms + color, uv, sampler & dynamic states
    eVPNoSamplerDynamic,  // V/P uniforms + color, uv & dynamic states

    eTerrainCompute,
  };

  enum SemaphoreType
  {
    ePresentComplete = 0,
    eRenderComplete,
    eComputeComplete,
  };
};

// This is used to determine stride for indirect draw calls
struct IndirectDrawCommand
{
  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;
};

class StateMachine;
class GuiHandler;

class Renderer
{
  friend class Engine;

public:
  explicit Renderer (vk::PhysicalDevice *p_PhysicalDevice,
                     vk::Device *p_LogicalDevice,
                     Swap *p_Swapchain,
                     GuiHandler *p_Gui,
                     StateMachine *p_StateMachine,
                     QueueIndexInfo p_QueueIndices);
  ~Renderer ();

  vk::PhysicalDevice *physicalDevice = nullptr;
  vk::Device *logicalDevice = nullptr;
  Swap *swapchain = nullptr;
  GuiHandler *gui = nullptr;
  StateMachine *stateMachine = nullptr;
  QueueIndexInfo queueIdx;

  mv::PipelineType currentlyBound;

  using PipelineMap = std::unordered_map<mv::PipelineType, vk::Pipeline>;
  // umap { mv::PipelineType, vk::Pipeline }
  std::unique_ptr<PipelineMap> pipelines;

  using PipelineLayoutMap = std::unordered_map<mv::PipelineType, vk::PipelineLayout>;
  // umap { mv::PipelineType, vk::PipelineLayout }
  std::unique_ptr<PipelineLayoutMap> pipelineLayouts;

  std::unique_ptr<Image> depthStencil;

  using RenderPassMap = std::unordered_map<mv::RenderPassType, vk::RenderPass>;
  // umap { mv::RenderPassType, vk::RenderPass }
  std::unique_ptr<RenderPassMap> renderPasses;

  using FramebufferMap = std::unordered_map<mv::FramebufferType, std::vector<vk::Framebuffer>>;
  // umap { mv::FramebufferType, vector<vk::Framebuffer> }
  std::unique_ptr<FramebufferMap> framebuffers;

  using CommandPoolMap
      = std::unordered_map<vk::QueueFlagBits,
                           std::pair<vk::CommandPool, std::vector<vk::CommandBuffer>>>;
  // umap { vk::QueueFlagBits, pair<vk::CommandPool, vector\<vk::CommandBuffer>> }
  std::unique_ptr<CommandPoolMap> commandPools;

  using CommandQueueMap = std::unordered_map<vk::QueueFlagBits, vk::Queue>;
  // umap { vk::QueueFlagBits, vk::Queue }
  std::unique_ptr<CommandQueueMap> commandQueues;

  std::unique_ptr<std::vector<vk::Fence>> inFlightFences;
  std::unique_ptr<std::vector<vk::Fence>> waitFences; // not allocated, just a ptr

  using SemaphoreMap = std::unordered_map<mv::SemaphoreType, vk::Semaphore>;
  // umap { mv::SemaphoreType, vk::Semaphore }
  std::unique_ptr<SemaphoreMap> semaphores;

  vk::ClearColorValue defaultClearColor = std::array<float, 4> ({ { 0.0f, 0.0f, 0.0f, 1.0f } });

public:
  // Create...
  // command buffers
  // sync primitives
  // depth stencil image
  // core render passes
  // core framebuffers
  void initialize (void);

  // cleanup for exit
  void cleanup (void);

  // cleanup for swap chain recreation
  void reset (void);

protected:
private:
  // create command buffers per enumerated queue type in commandPools
  void createCommandBuffers (void);

  // create semaphores/fences used in render
  void createSynchronizationPrimitives (void);

  void setupDepthStencil (void);

  // core render passes
  void setupRenderPass (void);

  // core framebuffers
  void setupFramebuffers (void);

  void prepareUniforms (void);

  void prepareLayouts (void);
  void prepareGraphicsPipelines (void);
  void prepareComputePipelines (void);
  void recordCommandBuffer (uint32_t imageIndex);
  void computePass (uint32_t p_ImageIndex);

  // Returns false if swapchain recreate is needed
  bool draw (size_t &p_CurrentFrame, uint32_t &p_CurrentImageIndex);

protected:
  inline vk::DescriptorSetLayout getLayout (vk::DescriptorType p_DescriptorType);

  inline void createCommandPools (vk::QueueFlags p_QueueFlags);
  inline void getCommandQueues (vk::QueueFlags p_QueueFlags);
  inline void createPipelineLayout (PipelineLayoutMap &p_LayoutMap,
                                    mv::PipelineType p_PipelineType,
                                    std::vector<vk::DescriptorSetLayout> &p_Layout);
  // cleanup
  inline void destroyCommandPools (void) noexcept;
  inline void destroyFramebuffers (void) noexcept;
  inline void destroyDepthStencil (void) noexcept;
  inline void destroyPipelines (void) noexcept;
  inline void destroyPipelineLayouts (void) noexcept;
  inline void destroyRenderPasses (void) noexcept;
  inline void destroySemaphores (void) noexcept;
  inline void destroyFences (void) noexcept;
};