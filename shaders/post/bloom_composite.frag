#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneTexture;
layout(set = 0, binding = 1) uniform sampler2D bloomTexture;

layout(push_constant) uniform PushConstants {
    float intensity;
    vec3 tint;
} params;

void main() {
    vec3 scene = texture(sceneTexture, inUV).rgb;
    vec3 bloom = texture(bloomTexture, inUV).rgb;
    
    vec3 result = scene + bloom * params.intensity * params.tint;
    
    outColor = vec4(result, 1.0);
}
