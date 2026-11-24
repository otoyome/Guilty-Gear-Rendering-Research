#include "subpixel_morphological_anti_aliasing.hlsli"

EDGE_DETECTION_OUT main(uint index : SV_VertexID)
{
    EDGE_DETECTION_OUT vout = (EDGE_DETECTION_OUT) 0;
    GetPositionUV(vout.position, vout.uv, index);
    SMAAEdgeDetectionVS(vout.uv, vout.offset);
    return vout;
}