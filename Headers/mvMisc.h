#pragma once

// Render pass defines
enum RenderPassType
{
    eCore = 0,
    eImGui,
};

// Containers for pipelines
enum PipelineTypes
{
    eMVPWSampler = 0,       // M/V/P uniforms + color, uv, sampler
    eMVPNoSampler,          // M/V/P uniforms + color, uv

    eVPWSampler,            // V/P uniforms + color, uv, sampler
    eVPNoSampler,           // V/P uniforms + color, uv

    // dynamic
    eMVPWSamplerDynamic,    // M/V/P uniforms + color, uv, sampler & dynamic states
    eMVPNoSamplerDynamic,   // M/V/P uniforms + color, uv & dynamic states
    eVPSamplerDynamic,      // V/P uniforms + color, uv, sampler & dynamic states
    eVPNoSamplerDynamic     // V/P uniforms + color, uv & dynamic states
};