#pragma once

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
enum PipelineTypes
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

// This is used to determine stride for indirect draw calls
struct IndirectDrawCommand
{
  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;
};