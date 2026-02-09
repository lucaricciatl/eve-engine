#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D currentFrame;
layout(set = 0, binding = 1) uniform sampler2D historyFrame;
layout(set = 0, binding = 2) uniform sampler2D velocityTexture;
layout(set = 0, binding = 3) uniform sampler2D depthTexture;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    float feedback;
    float sharpening;
} params;

// Tonemap for temporal resolve (Karis)
vec3 tonemap(vec3 c) {
    return c / (1.0 + max(max(c.r, c.g), c.b));
}

vec3 untonemap(vec3 c) {
    return c / (1.0 - max(max(c.r, c.g), c.b));
}

// RGB to YCoCg color space (better for clamping)
vec3 RGB2YCoCg(vec3 rgb) {
    return vec3(
        0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
        0.5 * rgb.r - 0.5 * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}

vec3 YCoCg2RGB(vec3 ycocg) {
    return vec3(
        ycocg.x + ycocg.y - ycocg.z,
        ycocg.x + ycocg.z,
        ycocg.x - ycocg.y - ycocg.z
    );
}

// Catmull-Rom filtering for history sampling
vec4 sampleCatmullRom(sampler2D tex, vec2 uv, vec2 texSize) {
    vec2 position = uv * texSize;
    vec2 center = floor(position - 0.5) + 0.5;
    
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;
    
    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w2 = -1.5 * f3 + 2.0 * f2 + 0.5 * f;
    vec2 w3 = 0.5 * (f3 - f2);
    
    vec2 w12 = w1 + w2;
    vec2 tc12 = (center + w2 / w12) / texSize;
    vec2 tc0 = (center - 1.0) / texSize;
    vec2 tc3 = (center + 2.0) / texSize;
    
    return (texture(tex, vec2(tc0.x, tc0.y)) * w0.x + 
            texture(tex, vec2(tc12.x, tc0.y)) * w12.x + 
            texture(tex, vec2(tc3.x, tc0.y)) * w3.x) * w0.y +
           (texture(tex, vec2(tc0.x, tc12.y)) * w0.x + 
            texture(tex, vec2(tc12.x, tc12.y)) * w12.x + 
            texture(tex, vec2(tc3.x, tc12.y)) * w3.x) * w12.y +
           (texture(tex, vec2(tc0.x, tc3.y)) * w0.x + 
            texture(tex, vec2(tc12.x, tc3.y)) * w12.x + 
            texture(tex, vec2(tc3.x, tc3.y)) * w3.x) * w3.y;
}

void main() {
    // Get velocity for reprojection
    vec2 velocity = texture(velocityTexture, inUV).xy;
    vec2 historyUV = inUV - velocity;
    
    // Sample current frame with 3x3 neighborhood
    vec3 colorCenter = texture(currentFrame, inUV).rgb;
    vec3 colorTL = texture(currentFrame, inUV + vec2(-1, -1) * params.texelSize).rgb;
    vec3 colorTR = texture(currentFrame, inUV + vec2( 1, -1) * params.texelSize).rgb;
    vec3 colorBL = texture(currentFrame, inUV + vec2(-1,  1) * params.texelSize).rgb;
    vec3 colorBR = texture(currentFrame, inUV + vec2( 1,  1) * params.texelSize).rgb;
    vec3 colorT  = texture(currentFrame, inUV + vec2( 0, -1) * params.texelSize).rgb;
    vec3 colorB  = texture(currentFrame, inUV + vec2( 0,  1) * params.texelSize).rgb;
    vec3 colorL  = texture(currentFrame, inUV + vec2(-1,  0) * params.texelSize).rgb;
    vec3 colorR  = texture(currentFrame, inUV + vec2( 1,  0) * params.texelSize).rgb;
    
    // Convert to YCoCg for clamping
    vec3 colorYCoCg = RGB2YCoCg(tonemap(colorCenter));
    vec3 colorTLYCoCg = RGB2YCoCg(tonemap(colorTL));
    vec3 colorTRYCoCg = RGB2YCoCg(tonemap(colorTR));
    vec3 colorBLYCoCg = RGB2YCoCg(tonemap(colorBL));
    vec3 colorBRYCoCg = RGB2YCoCg(tonemap(colorBR));
    vec3 colorTYCoCg = RGB2YCoCg(tonemap(colorT));
    vec3 colorBYCoCg = RGB2YCoCg(tonemap(colorB));
    vec3 colorLYCoCg = RGB2YCoCg(tonemap(colorL));
    vec3 colorRYCoCg = RGB2YCoCg(tonemap(colorR));
    
    // Compute color bounding box
    vec3 boxMin = min(colorYCoCg, min(min(colorTLYCoCg, colorTRYCoCg), 
                     min(min(colorBLYCoCg, colorBRYCoCg), 
                     min(min(colorTYCoCg, colorBYCoCg), min(colorLYCoCg, colorRYCoCg)))));
    vec3 boxMax = max(colorYCoCg, max(max(colorTLYCoCg, colorTRYCoCg), 
                     max(max(colorBLYCoCg, colorBRYCoCg), 
                     max(max(colorTYCoCg, colorBYCoCg), max(colorLYCoCg, colorRYCoCg)))));
    
    // Variance clipping (tighter box based on variance)
    vec3 m1 = colorYCoCg + colorTLYCoCg + colorTRYCoCg + colorBLYCoCg + colorBRYCoCg +
              colorTYCoCg + colorBYCoCg + colorLYCoCg + colorRYCoCg;
    vec3 m2 = colorYCoCg * colorYCoCg + colorTLYCoCg * colorTLYCoCg + 
              colorTRYCoCg * colorTRYCoCg + colorBLYCoCg * colorBLYCoCg +
              colorBRYCoCg * colorBRYCoCg + colorTYCoCg * colorTYCoCg +
              colorBYCoCg * colorBYCoCg + colorLYCoCg * colorLYCoCg + colorRYCoCg * colorRYCoCg;
    
    vec3 mean = m1 / 9.0;
    vec3 variance = sqrt(abs(m2 / 9.0 - mean * mean));
    
    const float gamma = 1.0;
    boxMin = mean - gamma * variance;
    boxMax = mean + gamma * variance;
    
    // Sample history with Catmull-Rom
    vec3 historyColor;
    if (historyUV.x < 0.0 || historyUV.x > 1.0 || historyUV.y < 0.0 || historyUV.y > 1.0) {
        historyColor = colorCenter;
    } else {
        vec2 texSize = 1.0 / params.texelSize;
        historyColor = sampleCatmullRom(historyFrame, historyUV, texSize).rgb;
    }
    
    // Convert history to YCoCg and clamp
    vec3 historyYCoCg = RGB2YCoCg(tonemap(historyColor));
    historyYCoCg = clamp(historyYCoCg, boxMin, boxMax);
    historyColor = untonemap(YCoCg2RGB(historyYCoCg));
    
    // Blend current and history
    float blend = params.feedback;
    
    // Reduce blend factor for fast motion
    float velocityMag = length(velocity * vec2(1.0 / params.texelSize));
    blend = mix(blend, 0.5, clamp(velocityMag / 10.0, 0.0, 1.0));
    
    vec3 result = mix(colorCenter, historyColor, blend);
    
    // Apply sharpening
    if (params.sharpening > 0.0) {
        vec3 neighbors = colorT + colorB + colorL + colorR;
        vec3 sharpened = colorCenter * (1.0 + 4.0 * params.sharpening) - 
                        neighbors * params.sharpening;
        result = mix(result, sharpened, params.sharpening);
    }
    
    outColor = vec4(result, 1.0);
}
