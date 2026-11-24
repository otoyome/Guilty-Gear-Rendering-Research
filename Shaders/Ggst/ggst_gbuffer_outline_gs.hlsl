#include "ggst_rendering.hlsli"

#define VS_OUT VS_GBUFFER_OUT

[maxvertexcount(6)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    int i;
    uint j;

	//	表面登録
    [unroll] for (i = 0; i < 3; i++)
    {
        input[i].w_normal.w = 0.0f;	//	アウトライン判定用に数値を変更
        output.Append(input[i]);
    }
    output.RestartStrip();

    //  輪郭線処理
    [unroll] for (i = 2; i >= 0; i--)
    {
        VS_OUT element = input[i];
        float3 w_position = calculate_outline_position(input[i].w_position.xyz, input[i].w_tangent.xyz, input[i].color[0]);
        element.position = mul(float4(w_position.xyz, 1), view_projection_transform);
        element.w_position.xyz = w_position;
        element.w_position.w = input[i].w_position.w;
        element.w_normal.w = 1.0f;	//	アウトライン判定用に数値を変更
        output.Append(element);
    }

    output.RestartStrip();
}
