#version 450
#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// AgX (Troy Sobotka), polynomial fit by Benjamin Wrensch. Input: linear Rec.709.
vec3 agxContrast(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

vec3 agx(vec3 c) {
    const mat3 inset = mat3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                            0.0784335999999992, 0.878468636469772, 0.0784336,
                            0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const mat3 outset = mat3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                             -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                             -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    const float minEv = -12.47393, maxEv = 4.026069;
    c = inset * c;
    c = clamp(log2(max(c, vec3(1e-10))), minEv, maxEv);
    c = agxContrast((c - minEv) / (maxEv - minEv));
    c = outset * c;
    return pow(max(c, vec3(0)), vec3(2.2)); // back to linear
}

vec3 linearToSrgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}

void main() {
    // pc.t0 = native HDR (resolved scene + additive), pc.t2 = bloom level 0 (LINEAR).
    vec3 c = texture(sampler2D(textures[pc.t0], samplers[0]), vUV).rgb;
    c += texture(sampler2D(textures[pc.t2], samplers[1]), vUV).rgb * u.bloomIntensity;
    c *= u.exposure;
    c *= vec3(1.0 + 0.3 * u.temperature, 1.0, 1.0 - 0.3 * u.temperature); // temperature: +warm / -cool
    c = clamp(agx(c), 0, 1);
    c = pow(max(c * u.gain + u.lift * (1.0 - c), vec3(0)), 1.0 / max(u.gamma, vec3(1e-3)));
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(luma), c, u.saturation) * u.fade;
    outColor = vec4(linearToSrgb(clamp(c, 0, 1)), 1); // swapchain is UNORM
}
