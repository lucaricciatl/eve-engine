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
    vec4 blurParams;  // x=enabled, y=radius, z=sigma, w=type
} cameraUBO;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 1, binding = 0) uniform sampler2D albedoTexture;

layout(push_constant) uniform ObjectPush {
    mat4 model;
    vec4 baseColor;
    vec4 materialParams;
    vec4 emissiveParams;
    vec4 mirrorParams;
} objectData;

float rand2(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float computeShadow(vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragLightSpacePos.xyz / fragLightSpacePos.w;
    if (projCoords.z > 1.0) {
        return 0.0;
    }

    vec2 uv = projCoords.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return 0.0;
    }

    float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
    float bias = max(0.0015 * (1.0 - ndotl), 0.00075);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    float angle = rand2(fragWorldPos.xy * 0.37) * 6.2831853;
    float c = cos(angle);
    float s = sin(angle);
    mat2 rot = mat2(c, -s, s, c);

    float shadow = 0.0;
    float weightSum = 0.0;
    const int radius = 2;
    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            vec2 offset = rot * vec2(x, y) * texelSize;
            float closestDepth = texture(shadowMap, uv + offset).r;
            float currentDepth = projCoords.z - bias;
            float tap = currentDepth > closestDepth ? 1.0 : 0.0;
            float weight = 1.0 - (length(vec2(x, y)) / float(radius + 1));
            shadow += tap * weight;
            weightSum += weight;
        }
    }

    return shadow / max(weightSum, 0.0001);
}

void main() {
    vec3 N = normalize(fragNormal);
    bool enableShadows = cameraUBO.shadingParams.y > 0.5;

    vec4 sampledAlbedo = texture(albedoTexture, fragUV);
    vec3 baseColor = fragColor.rgb * sampledAlbedo.rgb;

    vec3 viewDir = normalize(cameraUBO.cameraPosition.xyz - fragWorldPos);
    float rim = pow(1.0 - max(dot(N, viewDir), 0.0), 2.5);
    vec3 rimColor = mix(vec3(0.2, 0.4, 0.9), vec3(1.0, 0.7, 0.2), rim);

    int lightCount = int(cameraUBO.lightParams.x + 0.5);
    lightCount = clamp(lightCount, 1, 4);
    vec3 lit = vec3(0.0);
    float visibility = 1.0;
    vec3 fogScatter = vec3(0.0);

    for (int i = 0; i < lightCount; ++i) {
        vec3 lightPos = cameraUBO.lightPositions[i].xyz;
        vec3 lightColor = cameraUBO.lightColors[i].rgb;
        float lightIntensity = cameraUBO.lightColors[i].w;
        int lightType = int(cameraUBO.lightDirections[i].w + 0.5);
        vec3 lightForward = normalize(cameraUBO.lightDirections[i].xyz);
        vec3 lightDir = normalize(lightPos - fragWorldPos);
        float diff = max(dot(N, lightDir), 0.0);
        float distanceToLight = length(lightPos - fragWorldPos);
        float attenuation = 1.0 / (1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight);
        float range = max(cameraUBO.lightSpotAngles[i].z, 0.0);
        if (range > 0.0) {
            attenuation *= clamp(1.0 - (distanceToLight / range), 0.0, 1.0);
        }

        if (lightType == 1) {
            float innerCos = cameraUBO.lightSpotAngles[i].x;
            float outerCos = cameraUBO.lightSpotAngles[i].y;
            float cosTheta = dot(-lightDir, lightForward);
            float spotFactor = clamp((cosTheta - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);
            attenuation *= spotFactor;
        } else if (lightType == 2) {
            float halfWidth = cameraUBO.lightAreaParams[i].x;
            float halfHeight = cameraUBO.lightAreaParams[i].y;
            float areaRadius = max(0.001, length(vec2(halfWidth, halfHeight)));
            float areaFalloff = areaRadius / (areaRadius + distanceToLight);
            float facing = max(dot(lightForward, -lightDir), 0.0);
            attenuation *= areaFalloff * facing;
        }

        vec3 contribution = baseColor * diff * lightColor * lightIntensity * attenuation;
        if (i == 0) {
            float shadow = enableShadows ? computeShadow(N, lightDir) : 0.0;
            visibility = enableShadows ? (1.0 - shadow) : 1.0;
            lit += contribution * visibility;
        } else {
            lit += contribution;
        }

        float cosTheta = dot(viewDir, lightDir);
        float phase = 0.75 * (1.0 + cosTheta * cosTheta);
        fogScatter += lightColor * lightIntensity * 0.08 * phase * attenuation;
    }
    vec3 ambient = baseColor * 0.08;
    vec3 emissive = objectData.emissiveParams.rgb * objectData.emissiveParams.w;

    vec3 color = ambient + lit + rimColor * rim * 1.5 + emissive;

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

        vec3 fogTint = cameraUBO.fogColor.rgb + fogScatter;
        color = mix(color, fogTint, fogAmount);
    }

    outColor = vec4(color, fragColor.a * sampledAlbedo.a);
}
