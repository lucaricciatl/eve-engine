#version 450

// Gaussian blur post-process shader
// Uses separable two-pass blur for efficiency

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;     // 1.0 / textureSize
    vec2 direction;     // (1,0) for horizontal, (0,1) for vertical
    float radius;       // Blur radius multiplier
    float sigma;        // Gaussian sigma (not used in optimized version)
    float padding1;
    float padding2;
} params;

// Optimized 9-tap Gaussian blur weights (sigma ~= 1.5)
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec3 result = texture(inputTexture, inUV).rgb * weights[0];
    
    for (int i = 1; i < 5; ++i) {
        vec2 offset = params.direction * params.texelSize * float(i) * params.radius;
        result += texture(inputTexture, inUV + offset).rgb * weights[i];
        result += texture(inputTexture, inUV - offset).rgb * weights[i];
    }
    
    outColor = vec4(result, 1.0);
}

