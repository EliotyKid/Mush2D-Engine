#version 450
#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// Irradiance only (resolve.frag applies albedo, emissive and fog at native resolution).
// pc.t1 = normal, pc.t3 = depth. G-buffer read with NEAREST (light may run at reduced res).
vec4 tex(uint index) { return texture(sampler2D(textures[index], samplers[0]), vUV); }

void main() {
    vec4 nm = tex(pc.t1); float depth = tex(pc.t3).r;
    // Reconstructed at parallax 1 (approximate on background layers).
    vec2 worldPos = u.cameraPos + vec2(vUV.x * 2 - 1, 1 - vUV.y * 2) * u.halfSize;

    vec3 N = normalize(vec3(nm.xy * 2 - 1, 1));
    vec3 lit = u.ambientColor * u.ambientIntensity * nm.a;              // AO
    for (uint i = 0; i < u.lightCount; ++i) {
        GpuLight L = lights[i];
        vec3 toL = vec3(L.posHeightRadius.xy - worldPos, L.posHeightRadius.z + abs(L.depthFlicker.x - depth));
        float d = length(toL.xy); float att = clamp(1 - d / L.posHeightRadius.w, 0, 1); att *= att;
        float flick = 1 + L.depthFlicker.y * sin(u.time * L.depthFlicker.z + float(i) * 1.7);
        vec3 Ldir = normalize(toL);
        float diff = max(dot(N, Ldir), 0);
        float rim = max(dot(N, vec3(-Ldir.xy, 0)), 0) * nm.b;           // light from behind
        lit += L.colorIntensity.rgb * L.colorIntensity.a * att * flick * (diff + rim);
    }
    outColor = vec4(lit, 1);
}