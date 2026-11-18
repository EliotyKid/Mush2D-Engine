// Shared by the scene shaders. Layouts mirror SpriteBatch.hpp / SceneRenderer.{hpp,cpp}.
#extension GL_EXT_nonuniform_qualifier : require

// set 0 = Bindless
layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler samplers[2]; // 0 NEAREST, 1 LINEAR

struct SpriteInstance {
    vec2 position; vec2 size;   // size.x < 0 = mirrored
    vec4 uvRect; vec4 color;
    float rotation; float depth; vec2 parallax;
    uint albedo, normal, emissive, mask;
    uint flags; uint pad0, pad1, pad2; // bit0 NEAREST, bit1 ADDITIVE
};

struct GpuLight {
    vec4 posHeightRadius; // x, y, height, radius
    vec4 colorIntensity;
    vec4 depthFlicker;    // depth, amplitude, speed, 0
};

layout(std140, set = 1, binding = 0) uniform FrameUniforms {
    mat4 viewProj;      // no camera translation
    vec2 cameraPos;
    vec2 viewport;
    vec2 halfSize;      // visible half extent, world units
    float pixelsPerUnit;
    float time;
    uint lightCount;
    uint pixelArt;
    float fogDensity;
    float ambientIntensity;
    vec3 ambientColor; float exposure;
    vec3 fogColor; float fogGradientStart;
    float fogGradientEnd;
    float fogGradientStrength;
} u;

layout(std430, set = 1, binding = 1) readonly buffer Instances { SpriteInstance instances[]; };
layout(std430, set = 1, binding = 2) readonly buffer Lights { GpuLight lights[]; };

// Sprites: firstInstance. Fullscreen passes: t0..t3 = bindless indices / sampler choice.
layout(push_constant) uniform Push { uint firstInstance; uint t0, t1, t2, t3; } pc;
