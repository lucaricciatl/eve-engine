#version 450

// Fullscreen triangle vertex shader
// No vertex input needed - positions are computed from vertex ID

layout(location = 0) out vec2 outUV;

void main() {
    // Generate fullscreen triangle vertices
    // This creates a triangle that covers the entire screen
    // Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}
