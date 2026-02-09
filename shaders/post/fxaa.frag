#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    float qualitySubpix;
    float qualityEdgeThreshold;
} params;

#define FXAA_EDGE_THRESHOLD_MIN 0.0312
#define FXAA_SEARCH_STEPS 16

float rgb2luma(vec3 rgb) {
    return sqrt(dot(rgb, vec3(0.299, 0.587, 0.114)));
}

void main() {
    vec3 colorCenter = texture(inputTexture, inUV).rgb;
    
    // Luma at current fragment
    float lumaCenter = rgb2luma(colorCenter);
    
    // Luma at neighbors
    float lumaDown  = rgb2luma(textureOffset(inputTexture, inUV, ivec2( 0, -1)).rgb);
    float lumaUp    = rgb2luma(textureOffset(inputTexture, inUV, ivec2( 0,  1)).rgb);
    float lumaLeft  = rgb2luma(textureOffset(inputTexture, inUV, ivec2(-1,  0)).rgb);
    float lumaRight = rgb2luma(textureOffset(inputTexture, inUV, ivec2( 1,  0)).rgb);
    
    // Find the maximum and minimum luma around the current fragment
    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    
    // Compute the delta
    float lumaRange = lumaMax - lumaMin;
    
    // If the luma variation is lower than a threshold, skip AA
    if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * params.qualityEdgeThreshold)) {
        outColor = vec4(colorCenter, 1.0);
        return;
    }
    
    // Query the 4 diagonal neighbors
    float lumaDownLeft  = rgb2luma(textureOffset(inputTexture, inUV, ivec2(-1, -1)).rgb);
    float lumaUpRight   = rgb2luma(textureOffset(inputTexture, inUV, ivec2( 1,  1)).rgb);
    float lumaUpLeft    = rgb2luma(textureOffset(inputTexture, inUV, ivec2(-1,  1)).rgb);
    float lumaDownRight = rgb2luma(textureOffset(inputTexture, inUV, ivec2( 1, -1)).rgb);
    
    // Combine the four edges lumas
    float lumaDownUp    = lumaDown + lumaUp;
    float lumaLeftRight = lumaLeft + lumaRight;
    
    // Corners
    float lumaLeftCorners  = lumaDownLeft + lumaUpLeft;
    float lumaDownCorners  = lumaDownLeft + lumaDownRight;
    float lumaRightCorners = lumaDownRight + lumaUpRight;
    float lumaUpCorners    = lumaUpRight + lumaUpLeft;
    
    // Compute horizontal and vertical gradients
    float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) + 
                          abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 + 
                          abs(-2.0 * lumaRight + lumaRightCorners);
    float edgeVertical   = abs(-2.0 * lumaUp + lumaUpCorners) + 
                          abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 + 
                          abs(-2.0 * lumaDown + lumaDownCorners);
    
    // Is the local edge horizontal or vertical?
    bool isHorizontal = (edgeHorizontal >= edgeVertical);
    
    // Select the two neighboring texels lumas in the opposite direction
    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp : lumaRight;
    
    // Compute gradients in this direction
    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;
    
    // Which direction is steeper?
    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));
    
    // Average luma in the correct direction
    float lumaLocalAverage;
    if (is1Steepest) {
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
    } else {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
    }
    
    // Step size and direction
    float stepLength = isHorizontal ? params.texelSize.y : params.texelSize.x;
    if (is1Steepest) {
        stepLength = -stepLength;
    }
    
    // Shift UV in the correct direction by half a pixel
    vec2 currentUV = inUV;
    if (isHorizontal) {
        currentUV.y += stepLength * 0.5;
    } else {
        currentUV.x += stepLength * 0.5;
    }
    
    // Compute offset (for each iteration step) in the right direction
    vec2 offset = isHorizontal ? vec2(params.texelSize.x, 0.0) : vec2(0.0, params.texelSize.y);
    
    // Compute UVs to explore on each side of the edge
    vec2 uv1 = currentUV - offset;
    vec2 uv2 = currentUV + offset;
    
    // Read lumas at both ends, compute deltas
    float lumaEnd1 = rgb2luma(texture(inputTexture, uv1).rgb) - lumaLocalAverage;
    float lumaEnd2 = rgb2luma(texture(inputTexture, uv2).rgb) - lumaLocalAverage;
    
    // If the luma deltas are smaller than the gradient, continue searching
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;
    
    if (!reached1) {
        uv1 -= offset;
    }
    if (!reached2) {
        uv2 += offset;
    }
    
    // Continue to search if not reached
    if (!reachedBoth) {
        for (int i = 2; i < FXAA_SEARCH_STEPS; i++) {
            if (!reached1) {
                lumaEnd1 = rgb2luma(texture(inputTexture, uv1).rgb) - lumaLocalAverage;
            }
            if (!reached2) {
                lumaEnd2 = rgb2luma(texture(inputTexture, uv2).rgb) - lumaLocalAverage;
            }
            
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;
            
            if (!reached1) {
                uv1 -= offset;
            }
            if (!reached2) {
                uv2 += offset;
            }
            
            if (reachedBoth) break;
        }
    }
    
    // Estimate distance to endpoints
    float distance1 = isHorizontal ? (inUV.x - uv1.x) : (inUV.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - inUV.x) : (uv2.y - inUV.y);
    
    // Determine which direction is closer
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = (distance1 + distance2);
    
    // Compute subpixel offset
    float pixelOffset = -distanceFinal / edgeThickness + 0.5;
    
    // Is the luma at center smaller than the local average?
    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    
    // If the luma at center is smaller than at its neighbor, the delta is positive
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    
    // If the luma variation is incorrect, don't offset
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    
    // Subpixel antialiasing
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * params.qualitySubpix;
    
    // Pick the bigger of the two offsets
    finalOffset = max(finalOffset, subPixelOffsetFinal);
    
    // Compute the final UV coordinates
    vec2 finalUV = inUV;
    if (isHorizontal) {
        finalUV.y += finalOffset * stepLength;
    } else {
        finalUV.x += finalOffset * stepLength;
    }
    
    // Final color
    vec3 finalColor = texture(inputTexture, finalUV).rgb;
    outColor = vec4(finalColor, 1.0);
}
