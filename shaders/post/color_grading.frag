#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;
layout(set = 0, binding = 1) uniform sampler3D lutTexture;

layout(set = 0, binding = 2) uniform ColorGradingParams {
    float temperature;
    float tint;
    float saturation;
    float contrast;
    vec3 lift;
    float padding1;
    vec3 gamma;
    float padding2;
    vec3 gain;
    float lutContribution;
    mat3 channelMixer;
} params;

// Convert temperature to RGB
vec3 temperatureToRGB(float temp) {
    // Approximation based on Planck's law
    float t = temp / 100.0;
    vec3 color;
    
    if (t <= 66.0) {
        color.r = 1.0;
        color.g = 0.39008 * log(t) - 0.63184;
    } else {
        color.r = 1.292 * pow(t - 60.0, -0.1332);
        color.g = 1.129 * pow(t - 60.0, -0.0755);
    }
    
    if (t >= 66.0) {
        color.b = 1.0;
    } else if (t <= 19.0) {
        color.b = 0.0;
    } else {
        color.b = 0.54320 * log(t - 10.0) - 1.19625;
    }
    
    return clamp(color, 0.0, 1.0);
}

// Lift-Gamma-Gain color correction
vec3 liftGammaGain(vec3 color, vec3 lift, vec3 gamma, vec3 gain) {
    vec3 lerpLift = (1.0 - color) * lift;
    vec3 lerpGain = color * gain;
    vec3 result = lerpLift + lerpGain;
    return pow(max(result, vec3(0.0)), 1.0 / gamma);
}

// Saturation adjustment
vec3 adjustSaturation(vec3 color, float sat) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luminance), color, sat);
}

// Contrast adjustment
vec3 adjustContrast(vec3 color, float contrast) {
    return (color - 0.5) * contrast + 0.5;
}

void main() {
    vec3 color = texture(inputTexture, inUV).rgb;
    
    // White balance (temperature and tint)
    vec3 warmColor = temperatureToRGB(6500.0 + params.temperature * 50.0);
    color *= warmColor;
    color.g *= 1.0 + params.tint * 0.01; // Green/magenta shift
    
    // Channel mixer
    color = params.channelMixer * color;
    
    // Lift-Gamma-Gain
    color = liftGammaGain(color, params.lift, params.gamma, params.gain);
    
    // Saturation
    color = adjustSaturation(color, params.saturation);
    
    // Contrast
    color = adjustContrast(color, params.contrast);
    
    // LUT application
    if (params.lutContribution > 0.0) {
        vec3 lutColor = texture(lutTexture, color).rgb;
        color = mix(color, lutColor, params.lutContribution);
    }
    
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
