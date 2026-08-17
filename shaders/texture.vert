#version 450 core
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec4 a_Tint;

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Tint;

void main() {
    gl_Position = a_Position;
    v_UV = a_UV;
    v_Tint = a_Tint;
}
