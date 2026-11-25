#version 450
#include "common.glsl"
#include "sprite_sample.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vInstance;

layout(location = 0) out vec4 outColor;

vec4 tex(uint index, uint s) {
    return texture(sampler2D(textures[nonuniformEXT(index)], samplers[nonuniformEXT(s)]), vUV);
}

// Unlit, HDR: blended ONE, ONE over the lit scene. color may exceed 1 to feed bloom.
void main() {
    SpriteInstance inst = instances[vInstance];
    vec4 albedo = sampleSprite(inst.albedo, inst, vUV);
    vec3 emissive = sampleSprite(inst.emissive, inst, vUV).rgb;
    vec3 rgb = albedo.rgb * inst.color.rgb * inst.color.a + emissive;
    vec4 flash = unpackUnorm4x8(inst.flash); // hit flash, as in gbuffer.frag
    rgb = mix(rgb, flash.rgb, flash.a);
    outColor = vec4(rgb * albedo.a, 1); // texture alpha masks the quad
}
