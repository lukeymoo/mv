#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform ObjectUniform {
    mat4 model;
} ubo_obj;

layout(set = 1, binding = 0) uniform ViewUniform {
    mat4 mat;
} ubo_view;

layout(set = 2, binding = 0) uniform ProjectionUniform {
    mat4 mat;
} ubo_proj;

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_uv;

void main() {
    gl_PointSize = 2.0f;
    gl_Position = ubo_proj.mat * ubo_view.mat * ubo_obj.model * in_position;
    out_color = in_color;
    out_uv = in_uv;
}


