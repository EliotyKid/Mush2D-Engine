#version 450
#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// pc.t0 = source texture, pc.t1 = 1 on the first level (adds pc.t2 = additive target, soft threshold), else 0.
vec3 fetch(vec2 uv) {
    vec3 c = texture(sampler2D(textures[pc.t0], samplers[1]), uv).rgb;
    if (pc.t1 != 0u) {
        c += texture(sampler2D(textures[pc.t2], samplers[1]), uv).rgb;
        float br = max(c.r, max(c.g, c.b));
        float rq = clamp(br - u.bloomThreshold + u.bloomKnee, 0.0, 2.0 * u.bloomKnee);
        rq = rq * rq / (4.0 * u.bloomKnee + 1e-4);
        c *= max(rq, br - u.bloomThreshold) / max(br, 1e-4);
    }
    return c;
}

// 13-tap filter (Jimenez, "Next Generation Post Processing in Call of Duty").
void main() {
    vec2 t = 1.0 / vec2(textureSize(sampler2D(textures[pc.t0], samplers[1]), 0));
    vec3 a = fetch(vUV + t * vec2(-2, -2)), b = fetch(vUV + t * vec2(0, -2)), c = fetch(vUV + t * vec2(2, -2));
    vec3 d = fetch(vUV + t * vec2(-1, -1)), e = fetch(vUV + t * vec2(1, -1));
    vec3 f = fetch(vUV + t * vec2(-2, 0)),  g = fetch(vUV),                   h = fetch(vUV + t * vec2(2, 0));
    vec3 i = fetch(vUV + t * vec2(-1, 1)),  j = fetch(vUV + t * vec2(1, 1));
    vec3 k = fetch(vUV + t * vec2(-2, 2)),  l = fetch(vUV + t * vec2(0, 2)), m = fetch(vUV + t * vec2(2, 2));
    vec3 r = (d + e + i + j) * 0.125 + (a + b + f + g) * 0.03125 + (b + c + g + h) * 0.03125
           + (f + g + k + l) * 0.03125 + (g + h + l + m) * 0.03125;
    outColor = vec4(r, 1);
}