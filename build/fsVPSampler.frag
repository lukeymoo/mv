#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 2, binding = 0) uniform sampler2D textureSampler;
layout (set = 3, binding = 0) uniform sampler2D textureNormal;

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec4 inUV;
layout (location = 2) in vec4 inNormal;

layout (location = 0) out vec4 outColor;

void
main ()
{
  vec3 n = texture (textureNormal, inNormal.xy).xyz * 2.0 - 1.0;
  outColor = texture (textureSampler, inUV.xy);
}
