#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrTexture;

layout(push_constant) uniform PushConstants {
    float exposure;
    float gamma;
    float whitePoint;
    int algorithm;
} params;

// Reinhard simple
vec3 reinhardSimple(vec3 x) {
    return x / (1.0 + x);
}

// Reinhard extended (with white point)
vec3 reinhardExtended(vec3 x, float whitePoint) {
    vec3 numerator = x * (1.0 + (x / vec3(whitePoint * whitePoint)));
    return numerator / (1.0 + x);
}

// ACES Filmic (approximation)
vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ACES (more accurate)
vec3 aces(vec3 x) {
    mat3 inputMatrix = mat3(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777
    );
    mat3 outputMatrix = mat3(
        1.60475, -0.10208, -0.00327,
        -0.53108,  1.10813, -0.07276,
        -0.07367, -0.00605,  1.07602
    );
    vec3 v = inputMatrix * x;
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return clamp(outputMatrix * (a / b), 0.0, 1.0);
}

// Uncharted 2 Filmic
vec3 uncharted2Tonemap(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 uncharted2(vec3 x) {
    float W = 11.2;
    return uncharted2Tonemap(2.0 * x) / uncharted2Tonemap(vec3(W));
}

// Lottes
vec3 lottes(vec3 x) {
    vec3 a = x * (x + 0.0245786) - 0.000090537;
    vec3 b = x * (0.983729 * x + 0.4329510) + 0.238081;
    return a / b;
}

// Uchimura (Gran Turismo style)
vec3 uchimura(vec3 x) {
    float P = 1.0;   // max brightness
    float a = 1.0;   // contrast
    float m = 0.22;  // linear section start
    float l = 0.4;   // linear section length
    float c = 1.33;  // black tightness
    float b = 0.0;   // pedestal
    
    float l0 = ((P - m) * l) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;
    
    vec3 w0 = 1.0 - smoothstep(vec3(0.0), vec3(m), x);
    vec3 w2 = step(vec3(m + l0), x);
    vec3 w1 = 1.0 - w0 - w2;
    
    vec3 T = m * pow(x / m, vec3(c)) + b;
    vec3 S = P - (P - S1) * exp(CP * (x - S0));
    vec3 L = m + a * (x - m);
    
    return T * w0 + L * w1 + S * w2;
}

void main() {
    vec3 hdrColor = texture(hdrTexture, inUV).rgb;
    
    // Apply exposure
    hdrColor *= params.exposure;
    
    // Tone mapping
    vec3 mapped;
    switch (params.algorithm) {
        case 0: // None
            mapped = clamp(hdrColor, 0.0, 1.0);
            break;
        case 1: // Reinhard
            mapped = reinhardSimple(hdrColor);
            break;
        case 2: // Reinhard Extended
            mapped = reinhardExtended(hdrColor, params.whitePoint);
            break;
        case 3: // ACES
            mapped = aces(hdrColor);
            break;
        case 4: // ACES Approx
            mapped = acesFilm(hdrColor);
            break;
        case 5: // Uncharted 2
            mapped = uncharted2(hdrColor);
            break;
        case 6: // Lottes
            mapped = lottes(hdrColor);
            break;
        case 7: // Uchimura
            mapped = uchimura(hdrColor);
            break;
        default:
            mapped = acesFilm(hdrColor);
            break;
    }
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / params.gamma));
    
    outColor = vec4(mapped, 1.0);
}
