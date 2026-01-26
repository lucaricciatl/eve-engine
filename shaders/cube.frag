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
    vec4 cameraPosition;
    vec4 lightPosition;
    vec4 lightColorIntensity;
    vec4 shadingParams;
    vec4 fogColor;
    vec4 fogParams;
} cameraUBO;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 1, binding = 0) uniform sampler2D albedoTexture;

layout(push_constant) uniform ObjectPush {
    mat4 model;
    vec4 baseColor;
    vec4 materialParams;
    vec4 emissiveParams;
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
    vec3 lightPos = cameraUBO.lightPosition.xyz;
    bool directionalLight = cameraUBO.lightPosition.w > 0.5;
    vec3 lightColor = cameraUBO.lightColorIntensity.rgb;
    float lightIntensity = cameraUBO.lightColorIntensity.w;
    int debugMode = int(cameraUBO.shadingParams.x + 0.5);
    bool enableShadows = cameraUBO.shadingParams.y > 0.5;
    bool enableSpecular = cameraUBO.shadingParams.z > 0.5;

    vec3 lightDir = normalize(lightPos - fragWorldPos);
    float diff = max(dot(N, lightDir), 0.0);

    float distanceToLight = length(lightPos - fragWorldPos);
    float attenuation = directionalLight ? 1.0
                                         : 1.0 / (1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight);

    vec3 viewDir = normalize(cameraUBO.cameraPosition.xyz - fragWorldPos);
    vec3 halfVector = normalize(lightDir + viewDir);

    float metallic = clamp(objectData.materialParams.x, 0.0, 1.0);
    float roughness = clamp(objectData.materialParams.y, 0.02, 1.0);
    float specularStrength = clamp(objectData.materialParams.z, 0.0, 1.0);
    float opacity = clamp(objectData.materialParams.w, 0.0, 1.0);
    float shininess = mix(8.0, 128.0, 1.0 - roughness);
    float spec = pow(max(dot(N, halfVector), 0.0), shininess);

    vec4 sampledAlbedo = texture(albedoTexture, fragUV);
    vec3 baseColor = fragColor.rgb * sampledAlbedo.rgb;
    float alpha = fragColor.a * sampledAlbedo.a * opacity;

    vec3 diffuse = baseColor * diff * lightColor * lightIntensity;
    vec3 specularColor = mix(vec3(1.0), baseColor, metallic) * specularStrength;
    vec3 specular = enableSpecular ? lightColor * lightIntensity * specularColor * spec : vec3(0.0);
    vec3 ambient = baseColor * 0.1;
    vec3 emissive = objectData.emissiveParams.rgb * objectData.emissiveParams.w;
    float shadow = enableShadows ? computeShadow(N, lightDir) : 0.0;
    float visibility = enableShadows ? (1.0 - shadow) : 1.0;
    vec3 color = ambient + (diffuse + specular) * attenuation * visibility + emissive;
    float fogAmount = 0.0;

    bool isGlass = alpha < 0.98;
    if (isGlass) {
        float ndotv = max(dot(N, viewDir), 0.0);
        float fresnel = pow(1.0 - ndotv, 5.0);
        float glassVisibility = mix(1.0, visibility, 0.45);
        vec3 glassDiffuse = baseColor * 0.08 * diff;
        vec3 glassSpec = lightColor * lightIntensity * (spec * 2.0 + fresnel * 1.4);
        vec3 glassAmbient = baseColor * 0.02;
        color = glassAmbient + (glassDiffuse + glassSpec) * attenuation * glassVisibility;
        alpha = clamp(alpha + fresnel * 0.4, 0.05, 0.9);
    }

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
    }

    if (debugMode == 1) {
        color = baseColor;
    } else if (debugMode == 2) {
        color = normalize(N) * 0.5 + 0.5;
    } else if (debugMode == 3) {
        color = vec3(visibility);
    } else if (debugMode == 4) {
        color = vec3(fogAmount);
    }

    if (cameraUBO.fogColor.a > 0.5 && debugMode != 4) {
        vec3 lightDirNormalized = normalize(lightPos - fragWorldPos);
        float cosTheta = dot(viewDir, lightDirNormalized);
        float phase = 0.75 * (1.0 + cosTheta * cosTheta);
        vec3 fogScatter = lightColor * lightIntensity * 0.08 * phase;
        vec3 fogTint = cameraUBO.fogColor.rgb + fogScatter;
        color = mix(color, fogTint, fogAmount);
    }
    outColor = vec4(color, alpha);
}
