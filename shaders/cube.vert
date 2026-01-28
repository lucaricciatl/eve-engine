#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec4 fragLightSpacePos;
layout(location = 4) out vec2 fragUV;

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

layout(push_constant) uniform ObjectPush {
    mat4 model;
    vec4 baseColor;
    vec4 materialParams;
    vec4 emissiveParams;
    vec4 mirrorParams;
} objectData;

void main() {
    vec4 worldPos = objectData.model * vec4(inPosition, 1.0);
    gl_Position = cameraUBO.proj * cameraUBO.view * worldPos;
    fragLightSpacePos = cameraUBO.lightViewProj * worldPos;
    fragColor = objectData.baseColor;
    fragNormal = mat3(transpose(inverse(objectData.model))) * inNormal;
    fragWorldPos = worldPos.xyz;
    fragUV = inUV;
}
