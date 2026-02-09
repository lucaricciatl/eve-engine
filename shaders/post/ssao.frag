#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D depthTexture;
layout(set = 0, binding = 1) uniform sampler2D normalTexture;
layout(set = 0, binding = 2) uniform sampler2D noiseTexture;

layout(set = 0, binding = 3) uniform SSAOParams {
    mat4 projection;
    mat4 invProjection;
    vec4 samples[64];
    float radius;
    float bias;
    float intensity;
    float power;
    int sampleCount;
    vec2 noiseScale;
    float padding;
} params;

// Reconstruct view-space position from depth
vec3 getViewPos(vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = params.invProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

void main() {
    vec3 fragPos = getViewPos(inUV);
    vec3 normal = normalize(texture(normalTexture, inUV).rgb * 2.0 - 1.0);
    vec3 randomVec = normalize(texture(noiseTexture, inUV * params.noiseScale).xyz);
    
    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    
    for (int i = 0; i < params.sampleCount; ++i) {
        // Get sample position in view space
        vec3 sampleDir = TBN * params.samples[i].xyz;
        vec3 samplePos = fragPos + sampleDir * params.radius;
        
        // Project sample position to screen space
        vec4 offset = params.projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        
        // Get depth at sample position
        float sampleDepth = getViewPos(offset.xy).z;
        
        // Range check and accumulate
        float rangeCheck = smoothstep(0.0, 1.0, params.radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + params.bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / float(params.sampleCount));
    occlusion = pow(occlusion, params.power) * params.intensity;
    
    outAO = occlusion;
}
