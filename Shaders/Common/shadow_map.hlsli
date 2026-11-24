#ifndef __SHADOW_MAP_HLSLI__
#define __SHADOW_MAP_HLSLI__

static const uint shadow_map_max = 6;
static const uint shadow_map_constant_bias = 0.0001f;

//  シャドウマップ情報
cbuffer SHADOW_MAP_CONSTANT_BUFFER : register(b4)
{
    uint shadow_map_count;
    float3 shadow_attenuation;
    float4 shadow_bias[shadow_map_max];
    float4 shadow_split_distance[shadow_map_max];
    row_major float4x4 light_view_projection_transform[shadow_map_max];
    uint shadow_mask;
    float3 shadow_dummy;
};

//  シャドウマップ
Texture2DArray<float> shadow_maps : register(t10);
SamplerState shadow_maps_sampler_state : register(s10);

//	シャドウサンプル関数
float3 shadowmap_fetch(int index, float3 texcoord)
{
    float3 texcoordUVW = float3(texcoord.xy, index);
    float depth = shadow_maps.Sample(shadow_maps_sampler_state, texcoordUVW).r;
    float factor = step(texcoord.z, depth + shadow_map_constant_bias);
    return lerp(shadow_attenuation, float3(1.0f, 1.0f, 1.0f), factor);
}

//  PCFフィルタリングシャドウサンプル関数
float3 shadowmap_fetch_pcf(int index, float3 texcoord, float radius = 3.0f)
{
    float3 tex_size;
    shadow_maps.GetDimensions(tex_size.x, tex_size.y, tex_size.z);

    //  PCFノイズ
    static const float2 noise[16] =
    {
        { 0.772417068, -0.423055708 },
        { -0.714291692, 0.509235382 },
        { -0.780199528, -0.183640003 },
        { 0.472295880, 0.162610054 },
        { -0.442540765, -0.263137937 },
        { -0.960392714, -0.472289205 },
        { -0.347602427, 0.830676079 },
        { -0.573735476, -0.472531378 },
        { -0.629811406, -0.642059684 },
        { -0.352298439, -0.320050240 },
        { 0.599728346, 0.426071405 },
        { -0.173549354, 0.653944254 },
        { -0.287484705, -0.0727353096 },
        { 0.324079156, 0.534158945 },
        { 0.968484163, -0.229418755 },
        { -0.377048135, 0.338529825 },
    };
    static const uint noise_size = 16;

    float3 attenuation = (float3) 0.0f;
    [unroll(noise_size)] for (uint pcf_index = 0; pcf_index < noise_size; ++pcf_index)
    {
        float3 texcoordUVW = float3(texcoord.xy + radius.xx * (noise[pcf_index] / tex_size.xy), index);
        float depth = shadow_maps.Sample(shadow_maps_sampler_state, texcoordUVW).r;
        float value = step(texcoord.z, depth + shadow_map_constant_bias);
        attenuation += value.xxx;
    }
    return lerp(shadow_attenuation, float3(1.0f, 1.0f, 1.0f), attenuation / noise_size);
}

//	シャドウサンプル
float4 shadowing_process(float3 world_position, float4x4 view_matrix)
{
    float4 view_position = mul(float4(world_position, 1.0f), view_matrix);
    float view_depth = view_position.z / view_position.w;

    float3 light_attenuation = float3(1.0f, 1.0f, 1.0f);
    uint sm_index = shadow_map_max;
    for (uint index = 0; index < shadow_map_max; ++index)
    {
        if (index >= shadow_map_count)
            break;

        //	深度クリップ
        if (!(shadow_split_distance[index].x <= view_depth && view_depth <= shadow_split_distance[index].w))
            continue;

        //	シャドウ座標
        float4 shadow_position = mul(float4(world_position, 1.0f), light_view_projection_transform[index]);
        shadow_position /= shadow_position.w;

        //	境界クリップ
        float3 texcoord = float3(shadow_position.xy * float2(0.5f, -0.5f) + (float2)0.5f, shadow_position.z);
        if (!(all(texcoord >= (float3)0.0f) * all(texcoord <= (float3)1.0f)))
            continue;

        //light_attenuation = shadowmap_fetch(index, texcoord);
        light_attenuation = shadowmap_fetch_pcf(index, texcoord, shadow_bias[index].w);
        sm_index = index;
        break;
    }

    return float4(light_attenuation, sm_index);
}

#endif  //  __SHADOW_MAP_HLSLI__
