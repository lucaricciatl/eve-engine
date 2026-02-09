#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform PushConstants {
    // Vignette
    float vignetteIntensity;
    float vignetteSmoothness;
    float vignetteRoundness;
    vec2 vignetteCenter;
    vec3 vignetteColor;
    
    // Chromatic aberration
    float chromaticIntensity;
    float chromaticOffset;
    int chromaticRadial;
    
    // Film grain
    float grainIntensity;
    float grainResponse;
    float time;
} params;

// Hash function for film grain
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Film grain noise
float filmGrain(vec2 uv, float t) {
    float noise = hash(uv + vec2(t, t));
    noise = (noise - 0.5) * 2.0;
    return noise;
}

void main() {
    vec3 color;
    
    // Chromatic aberration
    if (params.chromaticIntensity > 0.0) {
        vec2 direction;
        if (params.chromaticRadial > 0) {
            direction = normalize(inUV - vec2(0.5));
        } else {
            direction = vec2(1.0, 0.0);
        }
        
        float dist = length(inUV - vec2(0.5));
        vec2 offset = direction * params.chromaticOffset * params.chromaticIntensity * dist;
        
        color.r = texture(inputTexture, inUV + offset).r;
        color.g = texture(inputTexture, inUV).g;
        color.b = texture(inputTexture, inUV - offset).b;
    } else {
        color = texture(inputTexture, inUV).rgb;
    }
    
    // Vignette
    if (params.vignetteIntensity > 0.0) {
        vec2 uv = inUV - params.vignetteCenter;
        uv.x *= mix(1.0, params.vignetteRoundness, params.vignetteRoundness);
        
        float vignette = length(uv);
        vignette = 1.0 - smoothstep(1.0 - params.vignetteSmoothness, 1.0, vignette * params.vignetteIntensity * 2.0);
        
        color = mix(params.vignetteColor, color, vignette);
    }
    
    // Film grain
    if (params.grainIntensity > 0.0) {
        float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
        float grain = filmGrain(inUV * 1000.0, params.time);
        
        // Response curve - more grain in midtones
        float response = 1.0 - abs(luminance * 2.0 - 1.0);
        response = pow(response, params.grainResponse);
        
        color += grain * params.grainIntensity * response;
    }
    
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
