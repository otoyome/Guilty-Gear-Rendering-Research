#include "subpixel_morphological_anti_aliasing.hlsli"

NEIGHBORHOOD_BLENDING_OUT main(uint index : SV_VertexID)
{
    NEIGHBORHOOD_BLENDING_OUT vout = (NEIGHBORHOOD_BLENDING_OUT) 0;
    GetPositionUV(vout.position, vout.uv, index);
    SMAANeighborhoodBlendingVS(vout.uv, vout.offset);
    return vout;
}
