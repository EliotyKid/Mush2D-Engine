#version 450
#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// Native-resolution resolve. pc.firstInstance = sampler for the irradiance (0 NEAREST / 1 LINEAR),
// pc.t0 = albedo, pc.t1 = emissive, pc.t2 = depth, pc.t3 = irradiance (possibly reduced).
vec4 gtex(uint index) { return texture(sampler2D(textures[index], samplers[0]), vUV); }

void main() {
    vec4 a = gtex(pc.t0);
    if (a.a < 0.5) { outColor = vec4(u.fogColor, 1); return; } // no sprite: sky

    vec3 irradiance = texture(sampler2D(textures[pc.t3], samplers[pc.firstInstance]), vUV).rgb;
    float depth = gtex(pc.t2).r;
    vec2 worldPos = u.cameraPos + vec2(vUV.x * 2 - 1, 1 - vUV.y * 2) * u.halfSize;

    vec3 color = a.rgb * irradiance + gtex(pc.t1).rgb;
    float fd = max(depth, 0);
    float fog = 1 - exp(-u.fogDensity * fd);
    fog = clamp(fog + u.fogGradientStrength * (1.0 - smoothstep(u.fogGradientStart, max(u.fogGradientEnd, u.fogGradientStart + 1e-4), worldPos.y)), 0, 1);
    outColor = vec4(mix(color, u.fogColor, fog), 1);
}
