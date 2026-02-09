#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D aoTexture;
layout(set = 0, binding = 1) uniform sampler2D depthTexture;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    float sharpness;
    float padding;
} params;

void main() {
    const int blurSize = 4;
    float centerDepth = texture(depthTexture, inUV).r;
    
    float result = 0.0;
    float totalWeight = 0.0;
    
    for (int x = -blurSize; x <= blurSize; ++x) {
        for (int y = -blurSize; y <= blurSize; ++y) {
            vec2 offset = vec2(float(x), float(y)) * params.texelSize;
            vec2 sampleUV = inUV + offset;
            
            float sampleAO = texture(aoTexture, sampleUV).r;
            float sampleDepth = texture(depthTexture, sampleUV).r;
            
            // Bilateral weight based on depth difference
            float depthDiff = abs(centerDepth - sampleDepth);
            float weight = exp(-depthDiff * params.sharpness);
            
            // Spatial weight (Gaussian)
            float dist = length(vec2(x, y));
            weight *= exp(-dist * dist / float(blurSize));
            
            result += sampleAO * weight;
            totalWeight += weight;
        }
    }
    
    outAO = result / totalWeight;
}
