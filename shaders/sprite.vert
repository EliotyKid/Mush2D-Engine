#version 450
#include "common.glsl"

layout(location = 0) out vec2 vUV;
layout(location = 1) flat out uint vInstance;

const vec2 corners[6] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(1, 1), vec2(0, 1), vec2(0, 0));

mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }

void main() {
    uint id = uint(gl_InstanceIndex) + pc.firstInstance;
    SpriteInstance inst = instances[id];
    vec2 corner = corners[gl_VertexIndex];

    vec2 local = (corner - 0.5) * abs(inst.size);
    local = rot(inst.rotation) * local;
    vec2 rel = inst.position - u.cameraPos * inst.parallax;  // parallax: camera offset per layer
    if (u.pixelArt != 0) {
        // Unrotated: snap the min corner, so odd-sized sprites keep whole-pixel edges. floor(x+0.5): no .5 ambiguity.
        vec2 h = inst.rotation == 0.0 ? 0.5 * abs(inst.size) : vec2(0.0);
        rel = floor((rel - h) * u.pixelsPerUnit + 0.5) / u.pixelsPerUnit + h;
    }
    gl_Position = u.viewProj * vec4(rel + local, 0, 1);       // viewProj without camera translation

    // Atlas rows go top-down (world bottom = v 1); mirrored sprites flip u.
    vec2 t = vec2(inst.size.x < 0 ? 1 - corner.x : corner.x, 1 - corner.y);
    vUV = inst.uvRect.xy + t * inst.uvRect.zw;
    vInstance = id;
}
