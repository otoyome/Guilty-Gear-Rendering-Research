#include "fullscreen_quad.hlsli"
#include "shading_functions.hlsli"
#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"
#include "common_gbuffer_loader.hlsli"
#include "common.hlsli"
#include "common_post_process.hlsli"

//	色調補正情報
cbuffer COLOR_FILTER_CONSTANT_BUFFER
{
    float intensity; // 色調補正適応量
    float stage_depth_scale; // ステージ用の深度による適応にスケール
    float2 dummy;
    float3 hsv_weight; // HSV色空間での調整係数
    float dummy2;
    float3 color_multiplyer; // 乗算値
    float dummy3;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    //PSGBufferTextures gbuffer_textures = get_gbuffer_textures();
    //GBufferData data = get_gbuffer_data(gbuffer_textures, pin.texcoord);

    float4 color = scene_color_map.Sample(scene_map_sampler, pin.texcoord);

    //  色調補正
    if (intensity > 0.0f)
    {
        color.rgb = convert_rgb_to_hsv(color.rgb);
        color.rgb *= intensity * hsv_weight;
        color.rgb = convert_hsv_to_rgb(color.rgb);
        color.rgb *= color_multiplyer;
    }

    return color;
}

