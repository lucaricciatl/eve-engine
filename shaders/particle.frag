#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 centered = fragUV - vec2(0.5);
    float falloff = clamp(1.0 - dot(centered, centered) * 4.0, 0.0, 1.0);
    float alpha = fragColor.a * falloff;
    if (alpha <= 0.01) {
        discard;
    }
    outColor = vec4(fragColor.rgb * falloff, alpha);
}
