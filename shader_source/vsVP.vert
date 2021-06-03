#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform ProjectionUniform { mat4 mat; }
uboProj;

layout (set = 1, binding = 0) uniform ViewUniform { mat4 mat; }
uboView;

layout (location = 0) in vec4 inPosition;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 inUV;
layout (location = 3) in vec4 inNormal;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outUV;
layout (location = 2) out vec4 outNormal;

void
main ()
{
  gl_PointSize = 2.0f;
  gl_Position = uboProj.mat * uboView.mat * inPosition;
  outColor = inColor;
  outUV = inUV;
}
