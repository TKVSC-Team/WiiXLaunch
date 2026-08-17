#version 450 core

// General-purpose "sprite/UI" shader: sample a bound texture, modulate by a
// per-vertex tint (which also carries alpha) - covers textured quads, flat
// color quads (white texture or tint alone), and real alpha transparency via
// v_Tint.a, all with the same shader. Anyone building a UI element on this
// pipeline binds their own texture + sets vertex tint/alpha; nothing else
// about the pipeline needs to change per-element.
layout(binding = 0) uniform sampler2D u_Texture;

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Tint;
layout(location = 0) out vec4 o_Color;

void main() {
    vec4 texColor = texture(u_Texture, v_UV);
    vec4 result = texColor * v_Tint;
    if (result.a < 0.05) {
        discard;
    }
    o_Color = result;
}
