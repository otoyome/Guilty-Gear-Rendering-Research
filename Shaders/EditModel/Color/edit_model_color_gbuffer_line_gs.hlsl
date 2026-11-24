#include "edit_model_color_rendering.hlsli"

#define VS_OUT VS_GBUFFER_OUT

[maxvertexcount(3)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    int i;
    uint j;

    [unroll] for (i = 0; i < 3; i++)
    {
        VS_OUT element = (VS_OUT) 0;

        float3 v_normal = normalize(mul(float4(input[i].w_normal.xyz * line_factor, 0), view_transform).xyz);
        float3 v_position = mul(input[i].position, inverse_projection_transform).xyz;
        v_position += v_normal;	//	テキトーに

        //  
        element.position = mul(float4(v_position.xyz, 1), projection_transform);
        element.w_position = input[i].w_position;
        element.w_normal.xyz = input[i].w_normal.xyz;
        element.w_normal.w = 1.0f;
#if defined(HAS_TANGENT)
        element.w_tangent = input[i].w_tangent;
#endif  //  defined(HAS_TANGENT)

#if VERTEX_COLOR_MAX > 0
        [unroll] for (j = 0; j < VertexColorMax; ++j)
        {
            element.color[j] = input[i].color[j] * float4(line_color.rgb, 1);
        }
#endif  //  VERTEX_COLOR_MAX > 0

#if TEXCOORD_MAX > 0
        [unroll] for (j = 0; j < TexcoordMax; ++j)
        {
            element.texcoord[j] = input[i].texcoord[j];
        }
#endif  //  TEXCOORD_MAX > 0
        element.current_clip_position = input[i].current_clip_position;
        element.previous_clip_position = input[i].previous_clip_position;
        output.Append(element);
    }

    output.RestartStrip();
}
