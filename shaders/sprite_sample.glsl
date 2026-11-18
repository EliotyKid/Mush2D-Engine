// Fragment-only (fwidth). Include after common.glsl.
// Sprite texture fetch. Sharp: NEAREST. Smooth: LINEAR with "sharp bilinear" uv remap (texel interiors stay solid, only ~1 screen
// pixel at texel borders blends), clamped into the sprite's own uvRect so unpadded atlas neighbours never bleed in. Else LINEAR.
// Samples unconditionally (fwidth needs uniform control flow); u.pixelMode is uniform.
vec4 sampleSprite(uint index, SpriteInstance inst, vec2 uv) {
    bool px = (inst.flags & 1u) != 0u;
    uint s = (px && u.pixelMode == 0u) ? 0u : 1u;
    if (px && u.pixelMode == 1u) {
        vec2 ts = vec2(textureSize(sampler2D(textures[nonuniformEXT(index)], samplers[1]), 0));
        vec2 p  = uv * ts;
        vec2 fw = max(fwidth(p), vec2(1e-5));
        vec2 c  = floor(p + 0.5);
        p = c + clamp((p - c) / fw, -0.5, 0.5);
        // uvRect.zw can be negative (LDtk flipped tiles), so take the rect's real min/max corners
        vec2 a  = inst.uvRect.xy, b = inst.uvRect.xy + inst.uvRect.zw;
        vec2 lo = min(a, b) + 0.5 / ts;
        vec2 hi = max(max(a, b) - 0.5 / ts, lo);
        uv = clamp(p / ts, lo, hi);
    }
    return texture(sampler2D(textures[nonuniformEXT(index)], samplers[nonuniformEXT(s)]), uv);
}
