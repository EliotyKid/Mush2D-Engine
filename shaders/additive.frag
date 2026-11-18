#version 450
#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vInstance;

layout(location = 0) out vec4 outColor;

vec4 tex(uint index, uint s) {
    return texture(sampler2D(textures[nonuniformEXT(index)], samplers[nonuniformEXT(s)]), vUV);
}

// Unlit, HDR: blended ONE, ONE over the lit scene. color may exceed 1 to feed bloom.
void main() {
    SpriteInstance inst = instances[vInstance];
    uint s = ((inst.flags & 1u) != 0u && u.pixelArt != 0u) ? 0u : 1u; // NEAREST only in pixel-art mode
    vec4 albedo = tex(inst.albedo, s);
    vec3 emissive = tex(inst.emissive, s).rgb;
    vec3 rgb = albedo.rgb * inst.color.rgb * inst.color.a + emissive;
    outColor = vec4(rgb * albedo.a, 1); // texture alpha masks the quad
}
