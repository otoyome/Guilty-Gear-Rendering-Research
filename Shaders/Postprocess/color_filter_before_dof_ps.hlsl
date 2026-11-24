#include "fullscreen_quad.hlsli"
#include "shading_functions.hlsli"
#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"
#include "common_gbuffer_loader.hlsli"
#include "common.hlsli"
#include "common_post_process.hlsli"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    //PSGBufferTextures gbuffer_textures = get_gbuffer_textures();
    //GBufferData data = get_gbuffer_data(gbuffer_textures, pin.texcoord);

    float4 color = scene_color_map.Sample(scene_map_sampler, pin.texcoord);
    return color;
}

