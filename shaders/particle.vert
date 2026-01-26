#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

layout(std140, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 lightViewProj;
    vec4 cameraPosition;
    vec4 lightPosition;
    vec4 lightColorIntensity;
    vec4 shadingParams;
    vec4 fogColor;
    vec4 fogParams;
} cameraUBO;

void main() {
    vec4 worldPos = vec4(inPosition, 1.0);
    gl_Position = cameraUBO.proj * cameraUBO.view * worldPos;
    fragUV = inUV;
    fragColor = inColor;
}
