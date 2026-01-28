#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragLightSpacePos;
layout(location = 4) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(std140, set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 lightViewProj;
    mat4 reflectionViewProj;
    vec4 cameraPosition;
    vec4 reflectionPlane;
    vec4 lightPositions[4];
    vec4 lightColors[4];
    vec4 lightDirections[4];
    vec4 lightSpotAngles[4];
    vec4 lightAreaParams[4];
    vec4 lightParams;
    vec4 shadingParams;
    vec4 fogColor;
    vec4 fogParams;
} cameraUBO;

layout(set = 1, binding = 0) uniform sampler2D albedoTexture;

layout(push_constant) uniform ObjectPush {
    mat4 model;
    vec4 baseColor;
    vec4 materialParams;
    vec4 emissiveParams;
    vec4 mirrorParams;
} objectData;

float gridLine(float coord, float scale)
{
    float grid = abs(fract(coord * scale) - 0.5);
    return 1.0 - smoothstep(0.45, 0.5, grid);
}

void main() {
    vec4 sampledAlbedo = texture(albedoTexture, fragUV);
    vec3 base = fragColor.rgb * sampledAlbedo.rgb * 0.15;

    float lineX = gridLine(fragUV.x, 12.0);
    float lineY = gridLine(fragUV.y, 12.0);
    float line = max(lineX, lineY);

    vec3 lineColor = vec3(0.9, 0.9, 1.0);
    vec3 color = mix(base, lineColor, line);

    float fogAmount = 0.0;
    if (cameraUBO.fogColor.a > 0.5) {
        float density = max(cameraUBO.fogParams.x, 0.0);
        float heightFalloff = max(cameraUBO.fogParams.y, 0.0);
        float heightOffset = cameraUBO.fogParams.z;
        float startDistance = max(cameraUBO.fogParams.w, 0.0);
        float distanceToCamera = length(cameraUBO.cameraPosition.xyz - fragWorldPos);
        float fogDistance = max(distanceToCamera - startDistance, 0.0);
        float height = fragWorldPos.y - heightOffset;
        float heightFactor = exp(-heightFalloff * abs(height));
        fogAmount = 1.0 - exp(-density * heightFactor * fogDistance);
        fogAmount = clamp(fogAmount, 0.0, 1.0);

        vec3 viewDir = normalize(cameraUBO.cameraPosition.xyz - fragWorldPos);
        int lightCount = int(cameraUBO.lightParams.x + 0.5);
        lightCount = clamp(lightCount, 1, 4);
        vec3 fogScatter = vec3(0.0);
        for (int i = 0; i < lightCount; ++i) {
            vec3 lightPos = cameraUBO.lightPositions[i].xyz;
            vec3 lightColor = cameraUBO.lightColors[i].rgb;
            float lightIntensity = cameraUBO.lightColors[i].w;
            int lightType = int(cameraUBO.lightDirections[i].w + 0.5);
            vec3 lightForward = normalize(cameraUBO.lightDirections[i].xyz);
            vec3 lightDirNormalized = normalize(lightPos - fragWorldPos);
            float cosTheta = dot(viewDir, lightDirNormalized);
            float phase = 0.75 * (1.0 + cosTheta * cosTheta);
            float distanceToLight = length(lightPos - fragWorldPos);
            float attenuation = 1.0 / (1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight);
            float range = max(cameraUBO.lightSpotAngles[i].z, 0.0);
            if (range > 0.0) {
                attenuation *= clamp(1.0 - (distanceToLight / range), 0.0, 1.0);
            }

            if (lightType == 1) {
                float innerCos = cameraUBO.lightSpotAngles[i].x;
                float outerCos = cameraUBO.lightSpotAngles[i].y;
                float spotCos = dot(-lightDirNormalized, lightForward);
                float spotFactor = clamp((spotCos - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);
                attenuation *= spotFactor;
            } else if (lightType == 2) {
                float halfWidth = cameraUBO.lightAreaParams[i].x;
                float halfHeight = cameraUBO.lightAreaParams[i].y;
                float areaRadius = max(0.001, length(vec2(halfWidth, halfHeight)));
                float areaFalloff = areaRadius / (areaRadius + distanceToLight);
                float facing = max(dot(lightForward, -lightDirNormalized), 0.0);
                attenuation *= areaFalloff * facing;
            }
            fogScatter += lightColor * lightIntensity * 0.08 * phase * attenuation;
        }
        vec3 fogTint = cameraUBO.fogColor.rgb + fogScatter;
        color = mix(color, fogTint, fogAmount);
    }

    outColor = vec4(color, fragColor.a * sampledAlbedo.a);
}
