#version 450

layout(location = 0) out vec2 vUV; // 0,0 = top-left

void main() {
    vUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUV * 2 - 1, 0, 1);
}
