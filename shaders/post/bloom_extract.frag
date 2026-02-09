#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform PushConstants {
    float threshold;
    float softThreshold;
    float intensity;
    float padding;
} params;

// Quadratic threshold function for smooth falloff
vec3 quadraticThreshold(vec3 color, float threshold, float knee) {
    float brightness = max(max(color.r, color.g), color.b);
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.00001);
    return color * contribution;
}

void main() {
    vec3 color = texture(inputTexture, inUV).rgb;
    
    float knee = params.threshold * params.softThreshold;
    vec3 result = quadraticThreshold(color, params.threshold, knee);
    
    outColor = vec4(result * params.intensity, 1.0);
}
