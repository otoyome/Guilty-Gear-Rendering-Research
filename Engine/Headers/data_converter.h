#pragma	once

#include "model_resource.h"
#include "model_instance.h"
#include "animation_resource.h"

#if defined(USE_IMGUI)
namespace data_converter
{
	//	初期姿勢ポーズアニメーション名
	constexpr std::string_view DefaultAnimationName = "default";

	//	アニメーションサンプリングレート
	constexpr float animation_sampling_rate = system_units::convert_fps_to_sec(1.0f);

	//	GLTF形式の変換関数
	namespace gltf
	{
		struct convert_model_data
		{
			std::string_view file_path;							//	ファイルパス
			bool is_compute_tangent = true;						//	接線の無いメッシュの場合に接戦を算出する
			bool is_populate_vertex_color = true;				//	頂点色の無いメッシュに頂点色を含める
			bool is_combine_unskinned_meshes = true;			//	スキンメッシュ以外のメッシュを結合
			bool is_change_coordinate_system = true;			//	座標系変換
			bool is_change_coordinate_system_indices = false;	//	座標系変換時にインデックスを反転
			bool is_normalize_weight = true;					//	影響度の正規化
			bool is_generate_shadow_mesh = true;				//	影メッシュ生成
			int generate_lod_mesh_count = 8;					//	LODメッシュ生成数

			//	[エンジン改造:接線に再計算法線を含める対応]
			bool is_recomputed_normal = true;					//	法線を再計算するか
			bool is_recomputed_normal_to_copy_tangent = true;	//	再計算した法線を接線情報としてコピーするか
		};

		struct convert_animation_data
		{
			std::string_view file_path;				//	ファイルパス
			bool is_root_ignore = true;				//	ルートノードは無視

			model_resource_ptr retarget_resource;	//	リターゲッティングリソース
			retarget_profile retarget_profile;		//	リターゲッティング情報
		};

		bool convert(model_resource& resource, const convert_model_data& convert_data);
		bool convert(animation_resource& resource, const convert_animation_data& convert_data);
	}	//	namespace gltf

	//	MMD形式の変換関数
	namespace mmd
	{
		struct convert_model_data
		{
			std::string_view file_path;							//	ファイルパス
			bool is_compute_tangent = true;						//	接線の無いメッシュの場合に接戦を算出する
			bool is_populate_vertex_color = true;				//	頂点色の無いメッシュに頂点色を含める
			bool is_combine_unskinned_meshes = true;			//	スキンメッシュ以外のメッシュを結合
			bool is_change_coordinate_system = true;			//	座標系変換
			bool is_change_coordinate_system_indices = false;	//	座標系変換時にインデックスを反転
			bool is_normalize_weight = true;					//	影響度の正規化
			bool is_generate_shadow_mesh = true;				//	影メッシュ生成
			int generate_lod_mesh_count = 8;					//	LODメッシュ生成数

			//	[エンジン改造:接線に再計算法線を含める対応]
			bool is_recomputed_normal = true;					//	法線を再計算するか
			bool is_recomputed_normal_to_copy_tangent = true;	//	再計算した法線を接線情報としてコピーするか
		};

		struct convert_animation_data
		{
			std::string_view file_path;				//	ファイルパス
			bool is_root_ignore = true;				//	ルートノードは無視

			model_resource_ptr retarget_resource;	//	リターゲッティングリソース
			retarget_profile retarget_profile;		//	リターゲッティング情報
		};

		bool convert(model_resource& resource, const convert_model_data& convert_data);
		bool convert(animation_resource& resource, const convert_animation_data& convert_data);
	}	//	namespace mmd

}	//	namespace	data_converter
#endif	//	defined(USE_IMGUI)
