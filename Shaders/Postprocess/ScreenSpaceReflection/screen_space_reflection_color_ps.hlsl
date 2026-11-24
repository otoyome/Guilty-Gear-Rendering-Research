
#include "screen_space_reflection.hlsli"

#include "common_gbuffer_loader.hlsli"

Texture2D<float4> screen_space_reflection_uv_map : register(t10);
SamplerState screen_space_reflection_uv_sampler : register(s10);
SamplerState linear_boarder_black_sampler : register(s11);

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData data = get_gbuffer_data(pin.texcoord);

    //  シェーディングモデルがunlitなら処理しない
    if (data.shading_model == shading_model_unlit)
        return (float4) 0;

    //  反射UVマップを取得
    float4 uv = screen_space_reflection_uv_map.Sample(screen_space_reflection_uv_sampler, pin.texcoord);

    //  透過している所は周辺情報を調べて色を補正
    if (uv.a <= 0.0001f && is_flag(ray_flags_consideration_uv_hole))
    {
        //  テクスチャサイズ取得
        float2 texture_size, g_buffer_texture_size;
        screen_space_reflection_uv_map.GetDimensions(texture_size.x, texture_size.y);
        gbuffer_base_color.GetDimensions(g_buffer_texture_size.x, g_buffer_texture_size.y);
        //  周辺情報を取得して確認
        int size = max(g_buffer_texture_size.x / texture_size.x, g_buffer_texture_size.y / texture_size.y) * 2;
        size = clamp(size, 0, 4);
        float separation = 1.0f;
        float4 accum_uv = (float4) 0;
        float count = 0.0f;
        [unroll] for (int i = -size; i <= size; ++i)
        {
            [unroll] for (int j = -size; j <= size; ++j)
            {
                float2 delta = (float2(i, j) * separation) / texture_size;
                accum_uv += screen_space_reflection_uv_map.Sample(screen_space_reflection_uv_sampler, pin.texcoord.xy + delta);
                count += 1.0f;
            }
        }
        uv = accum_uv / count;
    }

    float4 color = gbuffer_base_color.Sample(linear_boarder_black_sampler, uv.xy);
    color.a *= saturate(uv.w);

    return color;
}
