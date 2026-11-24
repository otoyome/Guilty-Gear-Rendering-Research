#ifndef __PMX_RENDERING_HLSLI__
#define __PMX_RENDERING_HLSLI__

//#define HAS_TANGENT
//#define VERTEX_COLOR_MAX  0
#define TEXCOORD_MAX        1
#define BONE_MAX            4
#include "model.hlsli"


#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseMap, 0)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, SphereMap, 1)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, RampMap, 2)

//  UV座標
static const uint BaseTexcoord = 0;

//  サンプラーステート
SamplerState linear_sampler_state : register(s0);

//  マテリアル情報
cbuffer TOON_MATERIAL_CONSTANT_BUFFER
{
    float4  base_color_factor;  //  ベースカラー
    float4  specular_color;     //  スペキュラカラー
    float4  edge_color;         //  輪郭線色
    
    float   outline_size;       //  輪郭線サイズ
    float   roughness;          //  粗さ(未使用)
    float   metalness;          //  金属質(未使用)
    float   threshold_offset;   //  閾値

};

#endif  //  __PMX_RENDERING_HLSLI__
