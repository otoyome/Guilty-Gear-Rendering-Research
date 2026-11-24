#ifndef __MATERIAL_PARAMETER_CONSTANT_BUFFER_HLSLI__
#define __MATERIAL_PARAMETER_CONSTANT_BUFFER_HLSLI__

#include "common.hlsli"

static const int shader_resource_view_max = 128;

static const uint srv_flag_srgb = 1 << 0;  //  sRGB色空間シェーダーリソースビュー

static const uint material_blend_mode_opaque = 0;		//	不透明描画
static const uint material_blend_mode_mask = 1;			//	不透明描画+透過マスク
static const uint material_blend_mode_translucent = 2;  //	半透明描画
static const uint material_blend_mode_addative = 3;     //  加算合成描画

cbuffer PARAMETER_CONSTANT_BUFFER : register(b2)
{
    int4 shader_resource_view_index[shader_resource_view_max / 4]; // SRV番号
    uint4 shader_resource_view_flags[shader_resource_view_max / 4]; // SRVフラグ類

    uint material_flags; // マテリアルフラグ類
    float material_alpha_clip_threshold; // 透明/不透明閾値
    uint material_shading_model; // シェーディングモデル
    uint material_blend_mode; // ブレンドモード
};

//  テクスチャ使用可否
bool has_resource(uint resource_id)
{
    return shader_resource_view_index[resource_id / 4][resource_id % 4] > -1;
}

bool is_convert_srgb(uint resource_id)
{
    return shader_resource_view_flags[resource_id / 4][resource_id % 4] & srv_flag_srgb;
}

float4 TextureSample(Texture1D srv, uint resource_id, SamplerState samp, float texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = degamma(color.rgb);

    return color;
}

float4 Texture2dSample(Texture2D srv, uint resource_id, SamplerState samp, float2 texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = degamma(color.rgb);

    return color;
}

float4 Texture2dSample(Texture3D srv, uint resource_id, SamplerState samp, float3 texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = degamma(color.rgb);

    return color;
}

float4 TextureCubeSample(TextureCube srv, uint resource_id, SamplerState samp, float3 texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = degamma(color.rgb);

    return color;
}

//  テクスチャサンプル
#define TEXTURE1D_SAMPLE(SRV, SAMPLER, TEXCOORD)  Texture1dSample(SRV, SRV##Index, SAMPLER, TEXCOORD)
#define TEXTURE2D_SAMPLE(SRV, SAMPLER, TEXCOORD)  Texture2dSample(SRV, SRV##Index, SAMPLER, TEXCOORD)
#define TEXTURE3D_SAMPLE(SRV, SAMPLER, TEXCOORD)  Texture3dSample(SRV, SRV##Index, SAMPLER, TEXCOORD)
#define TEXTURECUBE_SAMPLE(SRV, SAMPLER, TEXCOORD)  TextureCubeSample(SRV, SRV##Index, SAMPLER, TEXCOORD)

//  シェーダーリソースビューの名前構築
#define GET_SHADER_RESOURCE_VIEW_INDEX(NAME) NAME##Index

//  シェーダーリソース所持判定
#define HAS_RESOURCE(SRV) has_resource(GET_SHADER_RESOURCE_VIEW_INDEX(SRV))

//  シェーダーリソース登録用マクロ
#define REGISTER_SHADER_RESOURCE_VIEW(TYPE, NAME, INDEX) \
static const uint GET_SHADER_RESOURCE_VIEW_INDEX(NAME) = INDEX; \
TYPE NAME : register(t##INDEX);

#endif  //  __MATERIAL_PARAMETER_CONSTANT_BUFFER_HLSLI__
