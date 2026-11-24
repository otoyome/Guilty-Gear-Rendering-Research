#ifndef __GLTF_RENDERING_HLSLI__
#define __GLTF_RENDERING_HLSLI__

#define HAS_TANGENT
#define VERTEX_COLOR_MAX    1
#define TEXCOORD_MAX        1
#define BONE_MAX            16
#include "model.hlsli"

#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseColorMap, 0)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, RoughnessMetallicMap, 1);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, NormalMap, 2);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, EmissiveMap, 3);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, OcclusionMap, 4);

//  UV座標
static const uint BaseTexcoord = 0;

//  サンプラーステート
SamplerState linear_sampler_state : register(s0);

//  マテリアル情報
cbuffer GLTF_MATERIAL_CONSTANT_BUFFER
{
	float4  base_color_factor;	//	ベースカラー
	float3  emissive_factor;	//	自己発光ファクター
	float normal_map_scale;		//	法線マップのスケーリング値

	float roughness_factor;		//	粗さ
	float metallic_factor;		//	金属質
	float occlusion_strength;	//	光の遮蔽強度
};

#endif	//	__GLTF_RENDERING_HLSLI__
