#include "ggst_rendering.hlsli"

#define VS_OUT VS_DEPTH_OUT

[maxvertexcount(6)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    int i;
    uint j;

	//	•\–Ê“o˜^
    [unroll] for (i = 0; i < 3; i++)
    {
        output.Append(input[i]);
    }

    output.RestartStrip();

    //  —ÖŠsüˆ—
    for (i = 2; i >= 0; i--)
    {
        VS_OUT element = input[i];
        float3 w_position = calculate_outline_position(input[i].w_position.xyz, input[i].w_tangent.xyz, input[i].color[0]);
        element.position = mul(float4(w_position.xyz, 1), view_projection_transform);
        element.w_position.xyz = w_position;
        element.w_position.w = input[i].w_position.w;
        output.Append(element);
    }

    output.RestartStrip();
}
