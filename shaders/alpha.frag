#version 450
#include "common.glsl"
#include "sprite_sample.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vInstance;

layout(location = 0) out vec4 outColor;

// Non-additive particles, blended SRC_ALPHA, ONE_MINUS_SRC_ALPHA over the resolved HDR.
// pc.firstInstance = first alpha instance; pc.t0 = G-buffer albedo, pc.t1 = G-buffer depth, pc.t2 = irradiance
// (possibly reduced), pc.t3 = irradiance sampler (0 NEAREST / 1 LINEAR), as the resolve.
// Lit like the scene (irradiance under the particle; ambient over sky), hidden behind nearer opaque sprites,
// then the resolve's fog.
void main() {
    SpriteInstance inst = instances[vInstance];
    // Sample everything before discard (implicit derivatives).
    vec4 albedo = sampleSprite(inst.albedo, inst, vUV) * inst.color;
    vec3 emissive = sampleSprite(inst.emissive, inst, vUV).rgb;
    vec2 screenUV = gl_FragCoord.xy / u.viewport;
    vec4 scene = texture(sampler2D(textures[pc.t0], samplers[0]), screenUV);
    float sceneDepth = texture(sampler2D(textures[pc.t1], samplers[0]), screenUV).r;
    vec3 irradiance = texture(sampler2D(textures[pc.t2], samplers[pc.t3]), screenUV).rgb;

    bool opaque = scene.a >= 0.5;
    if (opaque && sceneDepth < inst.depth - 1e-3) discard; // an opaque sprite in front (e.g. foreground layer)

    vec3 light = opaque ? irradiance : u.ambientColor * u.ambientIntensity;
    vec3 rgb = albedo.rgb * light + emissive;
    vec4 flash = unpackUnorm4x8(inst.flash); // hit flash, as in gbuffer.frag
    rgb = mix(rgb, flash.rgb, flash.a);
    outColor = vec4(applyFog(rgb, inst.depth, screenUV), albedo.a);
}
