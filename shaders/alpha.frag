#version 450
#include "common.glsl"
#include "sprite_sample.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vInstance;

layout(location = 0) out vec4 outColor;

vec4 tex(uint index, uint s) {
    return texture(sampler2D(textures[nonuniformEXT(index)], samplers[nonuniformEXT(s)]), vUV);
}

// Unlit, final colour: blended SRC_ALPHA, ONE_MINUS_SRC_ALPHA over the resolved HDR (non-additive particles).
// Same depth fog as the resolve, so particles sit in the fogged layers.
void main() {
    SpriteInstance inst = instances[vInstance];
    vec4 albedo = sampleSprite(inst.albedo, inst, vUV) * inst.color;
    vec3 rgb = albedo.rgb + sampleSprite(inst.emissive, inst, vUV).rgb;
    vec4 flash = unpackUnorm4x8(inst.flash); // hit flash, as in gbuffer.frag
    rgb = mix(rgb, flash.rgb, flash.a);
    outColor = vec4(applyFog(rgb, inst.depth, gl_FragCoord.xy / u.viewport), albedo.a);
}
