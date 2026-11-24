#include "subpixel_morphological_anti_aliasing.hlsli"

Texture2D color_map : register(t0);

float2 main(EDGE_DETECTION_OUT pin) : SV_TARGET0
{
    return SMAAColorEdgeDetectionPS(pin.uv.xy, pin.offset, color_map);
}
