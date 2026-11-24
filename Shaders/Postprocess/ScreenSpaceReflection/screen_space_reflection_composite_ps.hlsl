#include "screen_space_reflection.hlsli"
#include "common.hlsli"

#include "common_gbuffer_loader.hlsli"

Texture2D<float4> screen_space_reflection_map : register(t10);
Texture2D<float4> screen_space_reflection_bokeh_map : register(t11);
SamplerState screen_space_reflection_bokeh_sampler : register(s11);

float4 main(VS_OUT pin) : SV_TARGET
{
    GBufferData data = get_gbuffer_data(pin.texcoord.xy);

    if (data.shading_model == shading_model_unlit || data.metalness <= 0.01f)
    {
        return float4(data.base_color.rgb, 1);
    }

	//  反射映像取得
    float4 reflection_color = screen_space_reflection_map.Sample(sampler_states, pin.texcoord.xy);

    //  粗さを考慮
    if (data.roughness >= 0.001f)
    {
#if 0   //  暈し画像を使うならこっちで
        float4 bokeh_reflection_color = screen_space_reflection_bokeh_map.Sample(sampler_states, pin.texcoord.xy);
#else
        //  テクスチャサイズ取得
        float2 texture_size;
        screen_space_reflection_map.GetDimensions(texture_size.x, texture_size.y);

        //  周辺情報を取得して確認
        float random_value = random(pin.texcoord.xy);
        int size = 8;
        float separation = saturate(data.roughness) * (random_value * (size / 2.0f) + size);
        float4 accum_color = (float4) 0;
        float count = 0.0f;
        [unroll]
        for (int i = -size; i <= size; ++i)
        {
            [unroll]
            for (int j = -size; j <= size; ++j)
            {
                float2 delta = (float2(i, j) * separation) / texture_size;
                accum_color += screen_space_reflection_map.Sample(screen_space_reflection_bokeh_sampler, pin.texcoord.xy + delta);
                count += 1.0f;
            }
        }
        float4 bokeh_reflection_color = accum_color / count;
#endif
        reflection_color = lerp(reflection_color, bokeh_reflection_color, saturate(data.roughness));
    }

    //  反射映像を適応
    float3 color = lerp(data.base_color.rgb, reflection_color.rgb, data.metalness * reflection_color.a);
    return float4(color.rgb, 1);
}
