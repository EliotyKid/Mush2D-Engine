#version 450
#include "common.glsl"
#include "sprite_sample.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vInstance;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;   // RG normal, B rim, A AO
layout(location = 2) out vec4 outEmissive;
layout(location = 3) out float outDepth;

vec4 tex(uint index, uint s) {
    return texture(sampler2D(textures[nonuniformEXT(index)], samplers[nonuniformEXT(s)]), vUV);
}

void main() {
    SpriteInstance inst = instances[vInstance];
    // Sample everything before discard (implicit derivatives).
    vec4 albedo = sampleSprite(inst.albedo, inst, vUV) * inst.color;
    vec4 normalTex = sampleSprite(inst.normal, inst, vUV);
    vec4 mask = sampleSprite(inst.mask, inst, vUV);
    vec4 emissive = sampleSprite(inst.emissive, inst, vUV);
    if (albedo.a < 0.5) discard;

    vec3 n = normalTex.xyz * 2 - 1;
    if (inst.size.x < 0) n.x = -n.x;
    float rc = cos(inst.rotation), rs = sin(inst.rotation); // tangent-space xy turns with the sprite (as sprite.vert)
    n.xy = vec2(rc * n.x - rs * n.y, rs * n.x + rc * n.y);

    outAlbedo = vec4(albedo.rgb, 1);
    outNormal = vec4(n.xy * 0.5 + 0.5, mask.r, mask.g);
    // Hit flash: the lit albedo fades out and the flash colour comes in as emissive (unlit, it lights nothing).
    vec4 flash = unpackUnorm4x8(inst.flash);
    outAlbedo.rgb *= 1 - flash.a;
    outEmissive = vec4(mix(emissive.rgb, flash.rgb, flash.a), 1);
    outDepth = inst.depth;
}
