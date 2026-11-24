#ifndef __FORWARD_LIGHT_DATA_HLSLI__
#define __FORWARD_LIGHT_DATA_HLSLI__

#include "lights.hlsli"

//  フォワード用光源情報
static const uint directional_light_count = 4;
static const uint point_light_count = 8;
static const uint spot_light_count = 8;
cbuffer FORWARD_LIGHTS_CONSTANT_BUFFER : register(b3)
{
    uint4 forward_only_light_counts; //  x : 平行光源, y : 点光源, z : スポットライト, w : 空き
    directional_lights forward_only_directional_light_data[directional_light_count];
    point_lights forward_only_point_light_data[point_light_count];
    spot_lights forward_only_spot_light_data[spot_light_count];
    light_probes forward_only_light_probe_data;
};

//  フォワード用IBLテクスチャ
TextureCube forward_only_diffuse_iem : register(t33);
TextureCube forward_only_specular_pmrem : register(t34);
Texture2D forward_only_lut_ggx : register(t35);
SamplerState forward_only_ibl_sampler_state : register(s1);

#endif  //  __FORWARD_LIGHT_DATA_HLSLI__
