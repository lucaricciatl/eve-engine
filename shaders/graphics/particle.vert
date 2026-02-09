#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) in float inShape;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out flat float fragShape;

layout(std140, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 lightViewProj;
    mat4 reflectionViewProj;
    vec4 cameraPosition;
    vec4 reflectionPlane;
    vec4 lightPositions[4];
    vec4 lightColors[4];
    vec4 lightDirections[4];
    vec4 lightSpotAngles[4];
    vec4 lightAreaParams[4];
    vec4 lightParams;
    vec4 shadingParams;
    vec4 fogColor;
    vec4 fogParams;
} cameraUBO;

void main() {
    vec4 worldPos = vec4(inPosition, 1.0);
    gl_Position = cameraUBO.proj * cameraUBO.view * worldPos;
    fragUV = inUV;
    fragColor = inColor;
    fragShape = inShape;
}
