#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 0, binding = 1) uniform sampler2D velocityTexture;
layout(set = 0, binding = 2) uniform sampler2D depthTexture;

layout(push_constant) uniform PushConstants {
    float intensity;
    float maxVelocity;
    float velocityScale;
    int samples;
    vec2 texelSize;
    float padding1;
    float padding2;
} params;

void main() {
    vec2 velocity = texture(velocityTexture, inUV).xy * params.velocityScale;
    
    // Clamp velocity magnitude
    float velocityMag = length(velocity);
    if (velocityMag > params.maxVelocity * params.texelSize.x) {
        velocity = normalize(velocity) * params.maxVelocity * params.texelSize.x;
    }
    
    // Early out for static pixels
    if (velocityMag < 0.0001) {
        outColor = texture(colorTexture, inUV);
        return;
    }
    
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    
    // Sample along the motion vector
    for (int i = 0; i < params.samples; ++i) {
        float t = float(i) / float(params.samples - 1) - 0.5;
        vec2 sampleUV = inUV + velocity * t * params.intensity;
        
        // Weight by distance from center
        float weight = 1.0 - abs(t * 2.0);
        weight = weight * weight;
        
        vec3 sampleColor = texture(colorTexture, sampleUV).rgb;
        color += sampleColor * weight;
        totalWeight += weight;
    }
    
    outColor = vec4(color / totalWeight, 1.0);
}
