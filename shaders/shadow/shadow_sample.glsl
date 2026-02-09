// Shadow sampling utility functions
// Include this in main lighting shaders

#ifndef SHADOW_SAMPLE_GLSL
#define SHADOW_SAMPLE_GLSL

// Constants
const int MAX_CASCADES = 4;
const float SHADOW_BIAS = 0.0005;

// Poisson disk samples for PCF
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

// Get cascade index based on view space depth
int getCascadeIndex(float viewDepth, vec4 cascadeSplits) {
    int cascadeIndex = 0;
    for (int i = 0; i < MAX_CASCADES - 1; ++i) {
        if (viewDepth > cascadeSplits[i]) {
            cascadeIndex = i + 1;
        }
    }
    return cascadeIndex;
}

// Transform world position to shadow map coordinates
vec3 worldToShadowCoord(vec3 worldPos, mat4 lightSpaceMatrix) {
    vec4 shadowCoord = lightSpaceMatrix * vec4(worldPos, 1.0);
    shadowCoord /= shadowCoord.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
    return shadowCoord.xyz;
}

// Basic shadow test (hard shadows)
float sampleShadowBasic(sampler2DArray shadowMap, vec3 shadowCoord, int cascadeIndex) {
    float currentDepth = shadowCoord.z;
    float shadowMapDepth = texture(shadowMap, vec3(shadowCoord.xy, cascadeIndex)).r;
    return currentDepth - SHADOW_BIAS > shadowMapDepth ? 0.0 : 1.0;
}

// PCF shadow sampling
float sampleShadowPCF(sampler2DArray shadowMap, vec3 shadowCoord, int cascadeIndex, 
                       float filterRadius, int sampleCount) {
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    
    for (int i = 0; i < sampleCount; ++i) {
        vec2 offset = poissonDisk[i] * filterRadius * texelSize;
        float shadowMapDepth = texture(shadowMap, vec3(shadowCoord.xy + offset, cascadeIndex)).r;
        shadow += shadowCoord.z - SHADOW_BIAS > shadowMapDepth ? 0.0 : 1.0;
    }
    
    return shadow / float(sampleCount);
}

// PCF with random rotation (reduces banding)
float sampleShadowPCFRotated(sampler2DArray shadowMap, vec3 shadowCoord, int cascadeIndex,
                              float filterRadius, int sampleCount, float randomAngle) {
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    
    float cosAngle = cos(randomAngle);
    float sinAngle = sin(randomAngle);
    mat2 rotation = mat2(cosAngle, sinAngle, -sinAngle, cosAngle);
    
    for (int i = 0; i < sampleCount; ++i) {
        vec2 offset = rotation * poissonDisk[i] * filterRadius * texelSize;
        float shadowMapDepth = texture(shadowMap, vec3(shadowCoord.xy + offset, cascadeIndex)).r;
        shadow += shadowCoord.z - SHADOW_BIAS > shadowMapDepth ? 0.0 : 1.0;
    }
    
    return shadow / float(sampleCount);
}

// PCSS (Percentage Closer Soft Shadows) - variable penumbra
float sampleShadowPCSS(sampler2DArray shadowMap, vec3 shadowCoord, int cascadeIndex,
                        float lightSize, int blockerSamples, int pcfSamples) {
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    float currentDepth = shadowCoord.z;
    
    // Step 1: Find average blocker depth
    float blockerSum = 0.0;
    int blockerCount = 0;
    
    float searchRadius = lightSize / currentDepth;
    for (int i = 0; i < blockerSamples; ++i) {
        vec2 offset = poissonDisk[i] * searchRadius * texelSize;
        float shadowMapDepth = texture(shadowMap, vec3(shadowCoord.xy + offset, cascadeIndex)).r;
        
        if (shadowMapDepth < currentDepth - SHADOW_BIAS) {
            blockerSum += shadowMapDepth;
            blockerCount++;
        }
    }
    
    // No blockers means fully lit
    if (blockerCount == 0) {
        return 1.0;
    }
    
    float avgBlockerDepth = blockerSum / float(blockerCount);
    
    // Step 2: Calculate penumbra width
    float penumbraWidth = (currentDepth - avgBlockerDepth) * lightSize / avgBlockerDepth;
    
    // Step 3: PCF with penumbra-based filter radius
    return sampleShadowPCF(shadowMap, shadowCoord, cascadeIndex, penumbraWidth, pcfSamples);
}

// VSM (Variance Shadow Maps) - requires separate moment shadow map
float sampleShadowVSM(sampler2DArray shadowMap, vec3 shadowCoord, int cascadeIndex,
                       float minVariance, float lightBleedReduction) {
    vec2 moments = texture(shadowMap, vec3(shadowCoord.xy, cascadeIndex)).rg;
    float currentDepth = shadowCoord.z;
    
    // Standard shadow test
    if (currentDepth <= moments.x) {
        return 1.0;
    }
    
    // Compute variance
    float variance = max(moments.y - moments.x * moments.x, minVariance);
    
    // Compute probabilistic upper bound
    float d = currentDepth - moments.x;
    float pMax = variance / (variance + d * d);
    
    // Light bleeding reduction
    return clamp((pMax - lightBleedReduction) / (1.0 - lightBleedReduction), 0.0, 1.0);
}

// ESM (Exponential Shadow Maps)
float sampleShadowESM(sampler2DArray shadowMap, vec3 shadowCoord, int cascadeIndex,
                       float exponent) {
    float shadowMapDepth = texture(shadowMap, vec3(shadowCoord.xy, cascadeIndex)).r;
    float currentDepth = shadowCoord.z;
    
    return clamp(exp(-exponent * (currentDepth - shadowMapDepth)), 0.0, 1.0);
}

// Cascade blending for smooth transitions
float getCascadeBlendFactor(float viewDepth, vec4 cascadeSplits, int cascadeIndex, float blendRange) {
    if (cascadeIndex >= MAX_CASCADES - 1) {
        return 0.0;
    }
    
    float splitEnd = cascadeSplits[cascadeIndex];
    float blendStart = splitEnd * (1.0 - blendRange);
    
    return smoothstep(blendStart, splitEnd, viewDepth);
}

// Main shadow sampling function with cascade selection and blending
float calculateShadow(sampler2DArray shadowMap, vec3 worldPos, float viewDepth,
                       mat4 lightSpaceMatrices[MAX_CASCADES], vec4 cascadeSplits,
                       float filterRadius, int sampleCount, float blendRange) {
    int cascadeIndex = getCascadeIndex(viewDepth, cascadeSplits);
    vec3 shadowCoord = worldToShadowCoord(worldPos, lightSpaceMatrices[cascadeIndex]);
    
    // Check bounds
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z > 1.0) {
        return 1.0;
    }
    
    float shadow = sampleShadowPCF(shadowMap, shadowCoord, cascadeIndex, filterRadius, sampleCount);
    
    // Blend between cascades
    float blendFactor = getCascadeBlendFactor(viewDepth, cascadeSplits, cascadeIndex, blendRange);
    if (blendFactor > 0.0 && cascadeIndex < MAX_CASCADES - 1) {
        vec3 nextShadowCoord = worldToShadowCoord(worldPos, lightSpaceMatrices[cascadeIndex + 1]);
        float nextShadow = sampleShadowPCF(shadowMap, nextShadowCoord, cascadeIndex + 1, filterRadius, sampleCount);
        shadow = mix(shadow, nextShadow, blendFactor);
    }
    
    return shadow;
}

// Debug: visualize cascade index as color
vec3 debugCascadeColor(int cascadeIndex) {
    const vec3 cascadeColors[4] = vec3[](
        vec3(1.0, 0.0, 0.0),  // Red
        vec3(0.0, 1.0, 0.0),  // Green
        vec3(0.0, 0.0, 1.0),  // Blue
        vec3(1.0, 1.0, 0.0)   // Yellow
    );
    return cascadeColors[cascadeIndex];
}

#endif // SHADOW_SAMPLE_GLSL
