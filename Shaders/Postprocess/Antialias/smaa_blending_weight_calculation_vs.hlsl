#include "subpixel_morphological_anti_aliasing.hlsli"

BLENDING_WEIGHT_OUT main(uint index : SV_VertexID)
{
    BLENDING_WEIGHT_OUT vout = (BLENDING_WEIGHT_OUT) 0;
    GetPositionUV(vout.position, vout.uv, index);
    SMAABlendingWeightCalculationVS(vout.uv, vout.texcoord, vout.offset);
    return vout;
}
