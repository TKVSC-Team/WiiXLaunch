#version 450 core

// Position/color passthrough is identical to rainbow.vert - this proves the
// same vertex plumbing (position + a repurposed vertex-color channel driving
// an animated value) can carry a genuinely custom fragment effect, not just
// interpolated flat color. v_Color.a is repurposed as a per-frame "time"
// value, written from the CPU into the vertex buffer every frame exactly the
// way vertex color already is - no new NVN binding (no UBO, no texture)
// needed at all.
layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main() {
    float t = v_Color.a;
    vec2 uv = gl_FragCoord.xy * 0.012;

    float v = 0.0;
    v += sin(uv.x * 3.0 + t);
    v += sin(uv.y * 3.3 - t * 1.3);
    v += sin((uv.x + uv.y) * 2.1 + t * 0.7);
    v += sin(length(uv - vec2(6.4, 3.6)) * 4.0 - t * 1.7);
    v *= 0.5;

    vec3 col = 0.5 + 0.5 * cos(6.28318 * (vec3(0.0, 0.33, 0.67) + v));
    o_Color = vec4(col, 1.0);
}
