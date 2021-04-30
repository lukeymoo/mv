#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec4 in_uv;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_uv;

void main() {
    out_color = in_color;
    out_uv = in_uv;
}


