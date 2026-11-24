#include "edit_model_color_rendering.hlsli"

#define VS_OUT VS_DEPTH_OUT

[maxvertexcount(3)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    int i, j;

    [unroll] for (i = 0; i < 3; i++)
    {
        VS_OUT element = (VS_OUT) 0;

        float3 v_normal = normalize(mul(float4(input[i].w_normal.xyz * line_factor, 0), view_transform).xyz);
        float3 v_position = mul(input[i].position, inverse_projection_transform).xyz;
        v_position += v_normal;	//	テキトーに

        element.position = mul(float4(v_position.xyz, 1), projection_transform);
        element.w_normal = input[i].w_normal;
        [unroll] for (j = 0; j < VertexColorMax; ++j)
        {
            element.color[j] = input[i].color[j] * float4(line_color.rgb, 1);
        }
        output.Append(element);
    }

    output.RestartStrip();
}
