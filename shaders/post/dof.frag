#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 0, binding = 1) uniform sampler2D depthTexture;

layout(push_constant) uniform PushConstants {
    float focusDistance;
    float focusRange;
    float maxBlurSize;
    float aperture;
    float focalLength;
    vec2 texelSize;
    int samples;
} params;

const float GOLDEN_ANGLE = 2.39996323;
const float PI = 3.14159265;

// Linear depth from depth buffer
float linearizeDepth(float depth, float near, float far) {
    return (2.0 * near * far) / (far + near - depth * (far - near));
}

// Circle of Confusion calculation
float getCoC(float depth) {
    // Simplified CoC based on thin lens equation
    float coc = (depth - params.focusDistance) / params.focusRange;
    return clamp(coc, -1.0, 1.0) * params.maxBlurSize;
}

// Bokeh disk sampling
vec3 sampleBokeh(vec2 uv, float cocRadius) {
    if (abs(cocRadius) < 0.5) {
        return texture(colorTexture, uv).rgb;
    }
    
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    
    float angle = 0.0;
    for (int i = 0; i < params.samples; ++i) {
        // Fibonacci spiral sampling
        float r = sqrt(float(i) / float(params.samples)) * cocRadius;
        angle += GOLDEN_ANGLE;
        
        vec2 offset = vec2(cos(angle), sin(angle)) * r * params.texelSize;
        vec2 sampleUV = uv + offset;
        
        float sampleDepth = texture(depthTexture, sampleUV).r;
        float sampleCoC = abs(getCoC(linearizeDepth(sampleDepth, 0.1, 1000.0)));
        
        // Weight based on CoC overlap
        float weight = clamp(sampleCoC, 0.5, 1.0);
        
        color += texture(colorTexture, sampleUV).rgb * weight;
        totalWeight += weight;
    }
    
    return color / totalWeight;
}

void main() {
    float depth = texture(depthTexture, inUV).r;
    float linearDepth = linearizeDepth(depth, 0.1, 1000.0);
    float coc = getCoC(linearDepth);
    
    vec3 color = sampleBokeh(inUV, abs(coc));
    
    outColor = vec4(color, 1.0);
}
