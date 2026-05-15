#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform ubuf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float screenCurvature;
    float frameSize;
    float screenRadius;
    vec2 viewportSize;
    float curvedGlass;
    float curvedGlassHighlightBurn;
};

vec2 distortCoordinates(vec2 coords) {
    vec2 paddedCoords = coords * (vec2(1.0) + frameSize * 2.0) - frameSize;
    vec2 cc = paddedCoords - vec2(0.5);
    float dist = dot(cc, cc) * screenCurvature;
    return paddedCoords + cc * (1.0 + dist) * dist;
}

float roundedRectSdfPixels(vec2 p, vec2 topLeft, vec2 bottomRight, float radiusPixels) {
    vec2 safeViewportSize = max(viewportSize, vec2(1.0));
    vec2 sizePixels = (bottomRight - topLeft) * safeViewportSize;
    vec2 centerPixels = (topLeft + bottomRight) * 0.5 * safeViewportSize;
    vec2 localPixels = p * safeViewportSize - centerPixels;
    vec2 halfSize = sizePixels * 0.5 - vec2(radiusPixels);
    vec2 d = abs(localPixels) - halfSize;
    return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0) - radiusPixels;
}

float saturate(float value) {
    return clamp(value, 0.0, 1.0);
}

vec3 domeNormal(vec2 coords) {
    vec2 p = coords * 2.0 - vec2(1.0);
    float domeStrength = 0.64;
    return normalize(vec3(p.x * 0.58 * domeStrength, p.y * 0.96 * domeStrength, 1.0));
}

float cornerZone(float dxPixels, float dyPixels, float widthPixels, float heightPixels) {
    float u = saturate(dxPixels / max(widthPixels, 1.0));
    float v = saturate(dyPixels / max(heightPixels, 1.0));
    return (1.0 - smoothstep(0.62, 1.0, u)) * (1.0 - smoothstep(0.62, 1.0, v));
}

void main() {
    const float HIGHLIGHT_GROW = 0.0;
    const float HIGHLIGHT_PUSH = 0.82;
    const float HIGHLIGHT_SHARPNESS = 0.75;
    const float HIGHLIGHT_CORNER_CURVE = 0.0;
    const float HIGHLIGHT_CORNER_FOCUS = 0.0;

    vec2 coords = distortCoordinates(qt_TexCoord0);
    float distPixels = roundedRectSdfPixels(coords, vec2(0.0), vec2(1.0), screenRadius);
    float insidePixels = -distPixels;
    float screenMask = smoothstep(0.0, 1.0, insidePixels);

    vec2 safeViewportSize = max(viewportSize, vec2(1.0));
    vec2 centered = coords * 2.0 - vec2(1.0);
    float radial = saturate(length(centered) * 0.78);
    vec3 normal = domeNormal(coords);

    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    vec3 keyLight = normalize(vec3(-0.42, -0.58, 0.70));
    vec3 halfKey = normalize(keyLight + viewDir);

    float keyFacing = saturate(dot(normal, keyLight) * 0.72 + 0.28);
    float broadSpec = pow(saturate(dot(normal, halfKey)), 20.0);
    float tightSpec = pow(saturate(dot(normal, halfKey)), 72.0);

    float minViewportSide = min(safeViewportSize.x, safeViewportSize.y);
    float leftPixels = coords.x * safeViewportSize.x;
    float rightPixels = (1.0 - coords.x) * safeViewportSize.x;
    float topPixels = coords.y * safeViewportSize.y;
    float bottomPixels = (1.0 - coords.y) * safeViewportSize.y;

    float highlightInsidePixels = insidePixels + HIGHLIGHT_GROW * 10.0;
    float highlightMask = smoothstep(0.0, 1.0, highlightInsidePixels);
    float highlightEdgeWidth = mix(230.0, 44.0, HIGHLIGHT_PUSH);
    float highlightEdgeExponent = mix(0.75, 6.5, HIGHLIGHT_SHARPNESS);
    float highlightEdge = pow(1.0 - smoothstep(0.0, highlightEdgeWidth, highlightInsidePixels), highlightEdgeExponent);
    float highlightInner = pow(1.0 - smoothstep(0.0, highlightEdgeWidth * 1.85, highlightInsidePixels), highlightEdgeExponent * 0.58);
    float highlightOuter = pow(1.0 - smoothstep(0.0, mix(36.0, 7.0, HIGHLIGHT_PUSH), highlightInsidePixels), mix(0.95, 2.6, HIGHLIGHT_SHARPNESS));

    float highlightCornerWidth = mix(0.34, 0.12, HIGHLIGHT_PUSH) * minViewportSide;
    float highlightCornerHeight = mix(0.42, 0.15, HIGHLIGHT_PUSH) * minViewportSide;
    float highlightTopLeftScale = mix(1.20, 0.58, HIGHLIGHT_CORNER_FOCUS);
    float hTlZone = cornerZone(leftPixels, topPixels, highlightCornerWidth * highlightTopLeftScale, highlightCornerHeight * highlightTopLeftScale);
    float hTrZone = cornerZone(rightPixels, topPixels, highlightCornerWidth, highlightCornerHeight);
    float hBlZone = cornerZone(leftPixels, bottomPixels, highlightCornerWidth, highlightCornerHeight);
    float hBrZone = cornerZone(rightPixels, bottomPixels, highlightCornerWidth, highlightCornerHeight);
    float highlightCornerArea = max(max(hTlZone, hTrZone), max(hBlZone, hBrZone));
    float highlightCornerSuppression = mix(0.42, 0.70, HIGHLIGHT_CORNER_CURVE);
    float softenedHighlightEdge = highlightEdge * (1.0 - highlightCornerArea * highlightCornerSuppression);

    float fresnel = pow(saturate((1.0 - normal.z) * 7.5), 1.55);
    float rimLight = softenedHighlightEdge * (0.45 + 0.55 * keyFacing) * (0.30 + 0.70 * fresnel);
    float edgeFlash = highlightOuter * keyFacing;
    float domeSheen = broadSpec * (0.45 + 0.55 * (1.0 - radial));
    float hotSpot = tightSpec * smoothstep(0.05, 0.95, keyFacing);
    float edgeShadow = softenedHighlightEdge * (0.55 + 0.45 * (1.0 - keyFacing));

    float glass = saturate(curvedGlass);
    float highlightSignal = highlightMask * (
        rimLight * 0.45 +
        edgeFlash * 0.22 +
        domeSheen * 0.18 +
        hotSpot * 0.24 +
        highlightInner * keyFacing * 0.12
    );

    float highlightBurnAmount = glass * highlightSignal * mix(0.0, 0.42, saturate(curvedGlassHighlightBurn)) * mix(0.80, 1.75, HIGHLIGHT_SHARPNESS);
    float darkAlpha = glass * screenMask * edgeShadow * mix(0.025, 0.095, HIGHLIGHT_PUSH);

    highlightBurnAmount = clamp(highlightBurnAmount, 0.0, 0.48);
    darkAlpha = clamp(darkAlpha, 0.0, 0.13);

    float alpha = clamp(highlightBurnAmount + darkAlpha, 0.0, 0.54);
    vec3 premultipliedColor = vec3(highlightBurnAmount);

    fragColor = vec4(premultipliedColor, alpha) * qt_Opacity;
}
