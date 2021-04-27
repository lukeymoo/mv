#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform ObjectUniform {
    mat4 model;
    // TODO
    // uv, normals, textures, etc
} ubo_obj;

layout(set = 1, binding = 0) uniform ViewUniform {
    mat4 mat;
} ubo_view;

layout(set = 2, binding = 0) uniform ProjectionUniform {
    mat4 mat;
} ubo_proj;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = ubo_proj.mat * ubo_view.mat * ubo_obj.model * inPosition;
    //gl_Position = inPosition;
    fragColor = inColor;
}


