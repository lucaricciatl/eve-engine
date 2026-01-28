#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in flat float fragShape;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 centered = fragUV - vec2(0.5);
    float circleFalloff = clamp(1.0 - dot(centered, centered) * 4.0, 0.0, 1.0);
    float squareFalloff = clamp(1.0 - max(abs(centered.x), abs(centered.y)) * 2.0, 0.0, 1.0);
    float diamondFalloff = clamp(1.0 - (abs(centered.x) + abs(centered.y)) * 1.6, 0.0, 1.0);
    int shape = int(fragShape + 0.5);
    float falloff = circleFalloff;
    if (shape == 1) {
        falloff = squareFalloff;
    } else if (shape == 2) {
        falloff = diamondFalloff;
    }
    float alpha = fragColor.a * falloff;
    if (alpha <= 0.01) {
        discard;
    }
    outColor = vec4(fragColor.rgb * falloff, alpha);
}
