#version 450

// Shadow pass only needs positions for depth
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

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

layout(push_constant) uniform ObjectPush {
    mat4 model;
    vec4 baseColor;
    vec4 materialParams;
    vec4 emissiveParams;
} objectData;

void main() {
    vec4 worldPos = objectData.model * vec4(inPosition, 1.0);
    gl_Position = cameraUBO.lightViewProj * worldPos;

    const float attributeInfluence = 1e-5;
    vec3 colorOffset = inColor * attributeInfluence;
    float normalOffset = dot(inNormal, vec3(attributeInfluence));
    gl_Position.xyz += colorOffset;
    gl_Position.w += normalOffset;
}
