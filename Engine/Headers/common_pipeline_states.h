#pragma once

#include <string>
#include <string_view>
#include <d3d11.h>

namespace common_pipeline_states
{
	//	SRV数
	constexpr UINT shader_resource_view_max = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;

	//	UAV数
	constexpr UINT unordered_access_view_max = 8;//	FEATURELEVEL11.1 : D3D11_1_UAV_SLOT_COUNT

	//	共通CBV/SRV/Samplerインデックス
	constexpr UINT scene_cbv_index = 0;				//	シーン定数バッファ番号
	constexpr UINT primitive_cbv_index = 1;			//	プリミティブ定数バッファ番号
	constexpr UINT parameter_cbv_index = 2;			//	パラメーター定数バッファ番号
	constexpr UINT light_cbv_index = 3;				//	ライトの定数バッファ番号
	constexpr UINT shadowmap_cbv_index = 4;			//	シャドウマップの定数バッファ番号
	constexpr UINT shadowmap_srv_index = 10;		//	シャドウマップのSRV番号
	constexpr UINT shadowmap_sampler_index = 10;	//	シャドウマップのSampler番号

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	シーン情報
	constexpr std::string_view scene_constant_buffer_name = "SCENE_CONSTANT_BUFFER";
	constexpr system_utility::hash_key scene_constant_buffer_hash_key = system_utility::generate_hash(scene_constant_buffer_name);
	struct scene_constants
	{
		DirectX::XMFLOAT4 time_data;	//	x : 現在時間, y : 前時間, zw : 空き
		DirectX::XMFLOAT4 z_buffer_parameteres;
		DirectX::XMFLOAT4 camera_position; //  xyz : 座標, w : fovY
		DirectX::XMFLOAT4 camera_right; //  xyz : 方向, w : tan(fovY)
		DirectX::XMFLOAT4 camera_up; //  xyz : 方向, w : 空き
		DirectX::XMFLOAT4 camera_direction; //  xyz : 方向, w : 空き
		DirectX::XMFLOAT4 camera_clip_distance;
		DirectX::XMFLOAT4 viewport_rect;
		DirectX::XMFLOAT4X4 view_transform;
		DirectX::XMFLOAT4X4 projection_transform;
		DirectX::XMFLOAT4X4 view_projection_transform;
		DirectX::XMFLOAT4X4 inverse_view_transform;
		DirectX::XMFLOAT4X4 inverse_projection_transform;
		DirectX::XMFLOAT4X4 inverse_view_projection_transform;
		DirectX::XMFLOAT4X4 previous_view_projection_transform;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	ライト定数バッファ構造体
	constexpr std::string_view deferred_light_constant_buffer_name = "DEFERREDLIGHT_CONSTANT_BUFFER";
	constexpr system_utility::hash_key deferred_light_constant_buffer_hash_key = system_utility::generate_hash(deferred_light_constant_buffer_name);
	struct deferred_light_constants
	{
		light_data light_data;
		UINT light_attribute;
		DirectX::XMFLOAT3 light_data_dummy;
	};

	//	フォワード用ライト定数バッファ構造体
	constexpr UINT directional_light_count = 4;
	constexpr UINT point_light_count = 8;
	constexpr UINT spot_light_count = 8;
	constexpr std::string_view forward_light_constant_buffer_name = "FORWARD_LIGHTS_CONSTANT_BUFFER";
	constexpr system_utility::hash_key forward_light_constant_buffer_hash_key = system_utility::generate_hash(forward_light_constant_buffer_name);
	struct forward_light_constants
	{
		DirectX::XMUINT4 light_counts;
		directional_lights directional_light_data[directional_light_count];
		point_lights point_light_data[point_light_count];
		spot_lights spot_light_data[spot_light_count];
		light_probes light_probe_data;
	};

	constexpr std::string_view forward_only_diffuse_iem_name = "forward_only_diffuse_iem";
	constexpr system_utility::hash_key forward_only_diffuse_iem_hash_key = system_utility::generate_hash(forward_only_diffuse_iem_name);
	constexpr std::string_view forward_only_specular_pmrem_name = "forward_only_specular_pmrem";
	constexpr system_utility::hash_key forward_only_specular_pmrem_hash_key = system_utility::generate_hash(forward_only_specular_pmrem_name);
	constexpr std::string_view forward_only_lut_ggx_name = "forward_only_lut_ggx";
	constexpr system_utility::hash_key forward_only_lut_ggx_hash_key = system_utility::generate_hash(forward_only_lut_ggx_name);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	影カテゴリー
	enum class shadow_category
	{
		default_id = 0,
		mask_1,
		mask_2,
		mask_3,
		mask_4,
		mask_5,
		mask_6,
		mask_7,
		max = 8,	// 8ビットまでなので
	};
	//	シャドウマップ情報
	constexpr UINT shadow_map_max = 6;
	constexpr std::string_view shadow_map_constant_buffer_name = "SHADOW_MAP_CONSTANT_BUFFER";
	constexpr system_utility::hash_key shadow_map_constant_buffer_hash_key = system_utility::generate_hash(shadow_map_constant_buffer_name);
	struct shadow_map_constants
	{
		UINT shadow_map_count = 0;
		DirectX::XMFLOAT3 shadow_attenuation;
		DirectX::XMFLOAT4 shadow_bias[shadow_map_max];
		DirectX::XMFLOAT4 shadow_split_distance[shadow_map_max];
		DirectX::XMFLOAT4X4 light_view_projection_transform[shadow_map_max];
		UINT shadow_mask;	//	8ビットまで
		DirectX::XMFLOAT3 shadow_dummy;
	};
	constexpr std::string_view shadow_maps_name = "shadow_maps";
	constexpr system_utility::hash_key shadow_maps_hash_key = system_utility::generate_hash(shadow_maps_name);
	constexpr std::string_view shadow_maps_sampler_state_name = "shadow_maps_sampler_state";
	constexpr system_utility::hash_key shadow_maps_sampler_state_hash_key = system_utility::generate_hash(shadow_maps_sampler_state_name);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	プリミティブ定数バッファ構造体
	constexpr UINT mesh_bone_max = 500;	//	1メッシュ500本あれば大丈夫かな？
	constexpr std::string_view primitive_constant_buffer_name = "PRIMITIVE_CONSTANT_BUFFER";
	constexpr system_utility::hash_key primitive_constant_buffer_hash_key = system_utility::generate_hash(primitive_constant_buffer_name);
	struct primitive_constants
	{
		bool has_tangent;	//	接線を所持しているかどうか
		DirectX::XMUINT3 primitive_dummy;

		DirectX::XMFLOAT4X4 bone_transforms[mesh_bone_max];
		DirectX::XMFLOAT4X4 previous_bone_transforms[mesh_bone_max];
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	パラメーター定数バッファ構造体
	constexpr std::string_view parameter_constant_buffer_name = "PARAMETER_CONSTANT_BUFFER";
	constexpr system_utility::hash_key parameter_constant_buffer_hash_key = system_utility::generate_hash(parameter_constant_buffer_name);
	struct parameter_constants
	{
		DirectX::XMINT4 shader_resource_view_index[shader_resource_view_max / 4]; // SRV使用フラグ
		DirectX::XMUINT4 shader_resource_view_flags[shader_resource_view_max / 4]; // SRVフラグ類

		UINT material_flags; // マテリアルフラグ類
		float material_alpha_clip_threshold; // 透明/不透明閾値
		UINT material_shading_model; // シェーディングモデル
		UINT material_blend_mode; // ブレンドモード

		parameter_constants()
		{
			memset(shader_resource_view_index, -1, sizeof(shader_resource_view_index));
			memset(shader_resource_view_flags, 0, sizeof(shader_resource_view_flags));
			material_flags = 0;
			material_shading_model = 0;
			material_alpha_clip_threshold = 0.1f;
			material_blend_mode = 0;
		}
	};
	//	シェーダーリソースフラグ
	constexpr UINT shader_resource_view_flags_srgb = 1 << 0;  //  sRGB色空間の情報フラグ
	constexpr UINT shader_resource_view_flags_cube_map = 1 << 1;  //  キューブマップフラグ

	//  マテリアルアトリビュート
	constexpr UINT material_attribute_indirect_lighting_off = 1 << 0;  //  間接光の処理を適応させない
	constexpr UINT material_attribute_shadow_receiver = 1 << 1;	//	影適応

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	シーン関係
	constexpr std::string_view scene_color_map_name = "scene_color_map";
	constexpr std::string_view scene_depth_map_name = "scene_depth_map";
	constexpr system_utility::hash_key scene_color_map_hash_key = system_utility::generate_hash(scene_color_map_name);
	constexpr system_utility::hash_key scene_depth_map_hash_key = system_utility::generate_hash(scene_depth_map_name);
	//	G-Buffer関連
	constexpr std::string_view gbuffer_base_color = "gbuffer_base_color";
	constexpr std::string_view gbuffer_normal_depth = "gbuffer_normal_depth";
	constexpr std::string_view gbuffer_parameter = "gbuffer_parameter";
	constexpr std::string_view gbuffer_velocity = "gbuffer_velocity";
	constexpr std::string_view gbuffer_unique_data = "gbuffer_unique_data";
	constexpr std::string_view gbuffer_shade_color = "gbuffer_shade_color";	//	[エンジン改造:G-Buffer]
	constexpr std::string_view gbuffer_ilm_color = "gbuffer_ilm_color";	//	[エンジン改造:G-Buffer]
	constexpr std::string_view gbuffer_names[] =
	{
		gbuffer_base_color,
		gbuffer_normal_depth,
		gbuffer_parameter,
		gbuffer_velocity,
		gbuffer_unique_data,

		gbuffer_shade_color,	//	[エンジン改造:G-Buffer]
		gbuffer_ilm_color,	//	[エンジン改造:G-Buffer]
	};
	constexpr system_utility::hash_key gbuffer_hash_keys[] =
	{
		system_utility::generate_hash(gbuffer_base_color),
		system_utility::generate_hash(gbuffer_normal_depth),
		system_utility::generate_hash(gbuffer_parameter),
		system_utility::generate_hash(gbuffer_velocity),
		system_utility::generate_hash(gbuffer_unique_data),

		system_utility::generate_hash(gbuffer_shade_color),	//	[エンジン改造:G-Buffer]
		system_utility::generate_hash(gbuffer_ilm_color),	//	[エンジン改造:G-Buffer]
	};
	//	G-Buffer識別子
	enum class gbuffer_id
	{
		base_color = 0,
		normal_depth,
		parameters,
		velocity,
		unique_data,
		shade_color,	//	[エンジン改造:G-Buffer]
		ilm_color,		//	[エンジン改造:G-Buffer]

		max,
		start = base_color,
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	スクリーン空間反射用情報
	//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
	struct screen_space_reflection_constants
	{
		float ray_max_distance;				//  反射最大距離
		float ray_resolution;				//  レイのステップ解像度
		DirectX::XMFLOAT2 ray_tickness;		//  深度判定を行う際の厚みの範囲

		int ray_substep_iteration_max;		//	ステップ深度チェック細分化回数
		int ray_flags;						//  処理の設定フラグ
		DirectX::XMFLOAT2 dummy;
		DirectX::XMFLOAT4 viewport_size;	//  シーンのビューポートサイズ
	};
	constexpr UINT screen_space_reflection_flags_reflection_dir_adjust = 0;	//	角度による厚み調整
	constexpr UINT screen_space_reflection_flags_consideration_uv_hole = 1;	//	UV穴埋め
	constexpr UINT screen_space_reflection_flags_substep_check = 2;	//	水平反射時の深度チェック
	constexpr UINT screen_space_reflection_flags_step_random = 3;	//	ランダムノイズを入れる

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	システム側で利用する定数バッファ名前リスト
	//	マテリアルやパイプラインステートで自由に設定されると困る系列
	constexpr std::string_view system_constant_buffer_names[] =
	{
		scene_constant_buffer_name,
		primitive_constant_buffer_name,
		shadow_map_constant_buffer_name,
		deferred_light_constant_buffer_name,
		forward_light_constant_buffer_name,
		parameter_constant_buffer_name,
	};

	//	システム側で利用するシェーダーリソース名前リスト
	//	マテリアルやパイプラインステートで自由に設定されると困る系列
	constexpr std::string_view system_shader_resource_names[] =
	{
		forward_only_diffuse_iem_name,
		forward_only_specular_pmrem_name,
		forward_only_lut_ggx_name,

		shadow_maps_name,

		scene_color_map_name,
		scene_depth_map_name,

		gbuffer_base_color,
		gbuffer_normal_depth,
		gbuffer_parameter,
		gbuffer_velocity,
		gbuffer_unique_data,
		gbuffer_shade_color,	//	[エンジン改造:G-Buffer]
		gbuffer_ilm_color,	//	[エンジン改造:G-Buffer]
	};
}	//	namespace common_pipeline_states

