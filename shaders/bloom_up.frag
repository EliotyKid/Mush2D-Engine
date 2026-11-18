#version 450
#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// pc.t0 = lower (smaller) level. Tent 3x3; the pipeline blends ONE, ONE into the level above.
void main() {
    vec2 t = 1.0 / vec2(textureSize(sampler2D(textures[pc.t0], samplers[1]), 0));
    #define S(x, y) texture(sampler2D(textures[pc.t0], samplers[1]), vUV + t * vec2(x, y)).rgb
    vec3 r = S(-1, -1) + 2.0 * S(0, -1) + S(1, -1)
           + 2.0 * S(-1, 0) + 4.0 * S(0, 0) + 2.0 * S(1, 0)
           + S(-1, 1) + 2.0 * S(0, 1) + S(1, 1);
    outColor = vec4(r / 16.0, 1);
}
