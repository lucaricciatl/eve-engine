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
    int debugMode = int(cameraUBO.shadingParams.x + 0.5);
    bool enableShadows = cameraUBO.shadingParams.y > 0.5;
    bool enableSpecular = cameraUBO.shadingParams.z > 0.5;

    vec3 viewDir = normalize(cameraUBO.cameraPosition.xyz - fragWorldPos);

    float metallic = clamp(objectData.materialParams.x, 0.0, 1.0);
    float roughness = clamp(objectData.materialParams.y, 0.02, 1.0);
    float specularStrength = clamp(objectData.materialParams.z, 0.0, 1.0);
    float opacity = clamp(objectData.materialParams.w, 0.0, 1.0);
    float shininess = mix(8.0, 128.0, 1.0 - roughness);
    vec4 sampledAlbedo = texture(albedoTexture, fragUV);
    vec3 baseColor = fragColor.rgb * sampledAlbedo.rgb;
    float alpha = fragColor.a * sampledAlbedo.a * opacity;

    int lightCount = int(cameraUBO.lightParams.x + 0.5);
    lightCount = clamp(lightCount, 1, 4);
    vec3 specularColor = mix(vec3(1.0), baseColor, metallic) * specularStrength;
    vec3 firstDiffuse = vec3(0.0);
    vec3 firstSpecular = vec3(0.0);
    vec3 firstLightColor = vec3(0.0);
    float firstLightIntensity = 0.0;
    vec3 firstLightDir = vec3(0.0, -1.0, 0.0);
    float firstAttenuation = 1.0;
    float firstDiff = 0.0;
    float firstSpec = 0.0;
    vec3 otherDiffuse = vec3(0.0);
    vec3 otherSpecular = vec3(0.0);
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
        vec3 halfVector = normalize(lightDir + viewDir);
        float spec = pow(max(dot(N, halfVector), 0.0), shininess);

        vec3 diffuse = baseColor * diff * lightColor * lightIntensity;
        vec3 specular = enableSpecular ? lightColor * lightIntensity * specularColor * spec : vec3(0.0);

        if (i == 0) {
            firstDiffuse = diffuse;
            firstSpecular = specular;
            firstLightColor = lightColor;
            firstLightIntensity = lightIntensity;
            firstLightDir = lightDir;
            firstAttenuation = attenuation;
            firstDiff = diff;
            firstSpec = spec;
        } else {
            otherDiffuse += diffuse * attenuation;
            otherSpecular += specular * attenuation;
        }

        float cosTheta = dot(viewDir, lightDir);
        float phase = 0.75 * (1.0 + cosTheta * cosTheta);
        fogScatter += lightColor * lightIntensity * 0.08 * phase * attenuation;
    }

    vec3 ambient = baseColor * 0.1;
    vec3 emissive = objectData.emissiveParams.rgb * objectData.emissiveParams.w;
    float shadow = enableShadows ? computeShadow(N, firstLightDir) : 0.0;
    float visibility = enableShadows ? (1.0 - shadow) : 1.0;
    vec3 color = ambient + (firstDiffuse + firstSpecular) * firstAttenuation * visibility + otherDiffuse + otherSpecular + emissive;
    float fogAmount = 0.0;
    bool isMirror = objectData.mirrorParams.x > 0.5;

    bool isGlass = alpha < 0.98;
    if (isGlass && !isMirror) {
        float ndotv = max(dot(N, viewDir), 0.0);
        float fresnel = pow(1.0 - ndotv, 5.0);
        float glassVisibility = mix(1.0, visibility, 0.45);
        vec3 glassTint = mix(vec3(1.0), baseColor, 0.65);
        vec3 glassDiffuse = baseColor * 0.12 * firstDiff;
        vec3 glassSpec = firstLightColor * firstLightIntensity * glassTint * (firstSpec * 1.2 + fresnel * 0.9) * specularStrength;
        vec3 glassAmbient = baseColor * 0.03;
        color = glassAmbient + (glassDiffuse + glassSpec) * firstAttenuation * glassVisibility;
        alpha = clamp(alpha + fresnel * 0.35, 0.08, 0.85);
    }

    if (isMirror) {
        vec3 planeNormal = cameraUBO.reflectionPlane.xyz;
        float planeLen = length(planeNormal);
        if (planeLen > 1e-4) {
            planeNormal /= planeLen;
            float planeD = cameraUBO.reflectionPlane.w;
            float distanceToPlane = dot(planeNormal, fragWorldPos) + planeD;
            vec3 reflectedPos = fragWorldPos - 2.0 * distanceToPlane * planeNormal;
            vec4 proj = cameraUBO.reflectionViewProj * vec4(reflectedPos, 1.0);
            vec3 ndc = proj.xyz / max(proj.w, 0.0001);
            vec2 reflectionUV = ndc.xy * 0.5 + 0.5;
            reflectionUV = clamp(reflectionUV, vec2(0.0), vec2(1.0));
            vec3 reflectedColor = texture(albedoTexture, reflectionUV).rgb;
            color = mix(reflectedColor, baseColor, 0.08);
            alpha = 1.0;
        }
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
        vec3 fogTint = cameraUBO.fogColor.rgb + fogScatter;
        color = mix(color, fogTint, fogAmount);
    }

    // Apply blur effect (depth-based blur simulation)
    if (cameraUBO.blurParams.x > 0.5) {
        float blurRadius = cameraUBO.blurParams.y;
        float blurSigma = cameraUBO.blurParams.z;
        int blurType = int(cameraUBO.blurParams.w + 0.5);
        
        // Calculate distance-based blur factor
        float distanceToCamera = length(cameraUBO.cameraPosition.xyz - fragWorldPos);
        float focusDistance = 8.0; // Focus at 8 units
        float blurFactor = abs(distanceToCamera - focusDistance) / 20.0;
        blurFactor = clamp(blurFactor * blurRadius * 0.1, 0.0, 1.0);
        
        // Apply blur as a color desaturation + softening effect
        // This simulates depth of field without actual post-processing
        vec3 blurredColor = color;
        
        if (blurType == 0) {
            // Gaussian-style: soften by averaging with nearby normal variations
            float luminance = dot(color, vec3(0.299, 0.587, 0.114));
            vec3 desaturated = vec3(luminance);
            blurredColor = mix(color, desaturated, blurFactor * 0.3);
            // Add slight color bleeding
            blurredColor = mix(blurredColor, blurredColor * 1.05, blurFactor * 0.2);
        } else if (blurType == 1) {
            // Box blur style: flatten contrast
            vec3 midGray = vec3(0.5);
            blurredColor = mix(color, mix(color, midGray, 0.3), blurFactor);
        } else if (blurType == 2) {
            // Radial blur style: shift towards center brightness
            float centerDist = length(fragUV - vec2(0.5));
            float radialFactor = centerDist * blurFactor;
            blurredColor = mix(color, color * (1.0 - radialFactor * 0.5), blurFactor);
        }
        
        color = blurredColor;
    }

    outColor = vec4(color, alpha);
}
