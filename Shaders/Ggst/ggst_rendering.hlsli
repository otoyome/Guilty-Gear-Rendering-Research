#ifndef __GGST_RENDERING_HLSLI__
#define __GGST_RENDERING_HLSLI__

#define HAS_TANGENT
#define VERTEX_COLOR_MAX    1
#define TEXCOORD_MAX        2
#define BONE_MAX            8
#include "model.hlsli"


#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseMap, 0)

//  UV座標
static const uint BaseTexcoord = 0;

//  サンプラーステート
SamplerState linear_sampler_state : register(s0);

//  マテリアル情報
cbuffer TOON_MATERIAL_CONSTANT_BUFFER
{
    float4  base_color_factor;              //  ベースカラー
    float  roughness;
    float  metalness;

};

//	基底の範囲で0～1を算出する関数
#define linear_step(a, b, d) saturate((d - a) / (b - a))

//  輪郭線算出関数
float3 calculate_outline_position(float3 w_position, float3 w_normal, float4 vertex_color)
{
    // r : 陰影判定の閾値に対するオフセット 1が標準で0だと必ず影になる
    // g : カメラからの輪郭線の距離に応じてどの程度膨張させるかの係数
    // b : 輪郭線のZオフセット値
    // a : 輪郭線の太さ係数。 0.5:標準、0で無し

    return w_position;
}

#endif  //  __GGST_RENDERING_HLSLI__
