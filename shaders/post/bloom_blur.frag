#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    vec2 direction;
    float radius;
    float padding1;
    float padding2;
    float padding3;
} params;

// 9-tap Gaussian blur weights
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texSize = params.texelSize;
    vec3 result = texture(inputTexture, inUV).rgb * weights[0];
    
    for (int i = 1; i < 5; ++i) {
        vec2 offset = params.direction * texSize * float(i) * params.radius;
        result += texture(inputTexture, inUV + offset).rgb * weights[i];
        result += texture(inputTexture, inUV - offset).rgb * weights[i];
    }
    
    outColor = vec4(result, 1.0);
}
