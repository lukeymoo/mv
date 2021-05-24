#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 2, binding = 0) uniform sampler2D texture_sampler;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec4 in_uv;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_uv;

void main() {
    vec2 uv_formatted = vec2(in_uv.x, in_uv.y);
    out_color = texture(texture_sampler, uv_formatted);
}


