#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 3, binding = 0) uniform sampler2D textureSampler;
layout (set = 4, binding = 0) uniform sampler2D textureNormal;

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec4 inUV;
layout (location = 2) in vec4 inNormal;

layout (location = 0) out vec4 outColor;

void
main ()
{
  outColor = texture (textureSampler, inUV.xy);
}
