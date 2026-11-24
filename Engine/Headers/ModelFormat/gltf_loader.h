#pragma once

#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tinygltf-release/tiny_gltf.h"

class gltf_loader
{
public:
	static constexpr size_t invalid_id = -1;

	enum class alpha_modes
	{
		opaque,
		mask,
		translucent,
	};

	struct node_data
	{
		std::string name;
		size_t parent_node_index = invalid_id;
		DirectX::XMFLOAT3 scale = { 1, 1, 1 };
		DirectX::XMFLOAT4 rotation = { 0, 0, 0, 1 };
		DirectX::XMFLOAT3 translation = { 0, 0, 0 };
	};

	struct bone_data
	{
		size_t node_index;
		DirectX::XMFLOAT4X4 offset_matrix = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
	};

	struct buffer_data
	{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		std::vector<BYTE> row_data;
		size_t stride_in_bytes = 0;
		size_t size_in_bytes = 0;
		size_t offset_in_bytes = 0;
		size_t count() const
		{
			return size_in_bytes / stride_in_bytes;
		}
		size_t offset() const
		{
			return offset_in_bytes / stride_in_bytes;
		}
		template<typename Type> Type* data() { return reinterpret_cast<Type*>(row_data.data()); }
		template<typename Type> const Type* data() const { return reinterpret_cast<Type*>(row_data.data()); }
		buffer_data() = default;
		~buffer_data() {}
	};

	struct mesh_data
	{
		std::vector<bone_data> bones;
		size_t material_index = invalid_id;
		size_t node_index = invalid_id;

		std::unordered_map<semantic, buffer_data> vertex_buffers;
		buffer_data index_buffer;

		DirectX::XMFLOAT3 bounds_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
		DirectX::XMFLOAT3 bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	};

	struct material_data
	{
		std::string name;
		alpha_modes alpha_mode = alpha_modes::opaque;
		DirectX::XMFLOAT4 base_color = { 1, 1, 1, 1 };
		DirectX::XMFLOAT3 emissive_color = { 1, 1, 1 };
		float emissive_intensity = 1;

		float occlusion_strength = 0.0f;
		float roughness = 0.0f;
		float metalness = 0.0f;
		float alpha_cutoff = 0.5f;
		float normal_scale = 1.0f;
		bool double_sided = false;

		std::string base_texture_path;
		std::string emissive_texture_path;
		std::string normal_texture_path;
		std::string occlusion_texture_path;
		std::string roughness_metalness_texture_path;
	};

	struct vector_key_frame
	{
		float second;
		DirectX::XMFLOAT3 value;
	};

	struct quaternion_key_frame
	{
		float second;
		DirectX::XMFLOAT4 value;
	};

	struct node_animation_data
	{
		std::string node_name;
		std::vector<vector_key_frame> translations;
		std::vector<quaternion_key_frame> rotations;
		std::vector<vector_key_frame> scales;

		struct pose_data
		{
			DirectX::XMFLOAT3 scale = {1.0f, 1.0f, 1.0f};
			DirectX::XMFLOAT4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
			DirectX::XMFLOAT3 translation = {0.0f, 0.0f, 0.0f};
		};
		pose_data calculate(float second);
	};

	struct animation_data
	{
		std::string name;
		float end_time;

		std::vector<node_animation_data> nodes;
	};

	struct register_vertex_data
	{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		std::vector<BYTE> set_data;	//	フォーマットごとに一つ分のデータで良い
	};

public:
	gltf_loader(std::string_view filepath);

	//	ノード読み込み
	void load_nodes(std::vector<node_data>& nodes);

	//	メッシュ読み込み
	struct convert_mesh_data
	{
		//	接線自動計算フラグ
		bool is_compute_tangent = false;

		//	影響度の正規化計算フラグ
		bool is_normalize_weight = false;

		//	右手座標系>左手座標系に変換
		//	基本的にはYUp前提
		bool is_change_coordinate_system = false;
		//	↑インデックスバッファのフリップ
		bool is_change_coordinate_system_indices = false;
	};
	void load_meshes(std::vector<mesh_data>& meshes, const std::vector<node_data>& nodes, const convert_mesh_data& convert_data);

	//	マテリアル読み込み
	void load_materials(std::vector<material_data>& materials);

	//	頂点バッファに存在しなければ情報登録
	void populate_vertex_data(std::vector<mesh_data>& meshes, const semantic& semantic, const register_vertex_data& vertex_data);

	//	テキトーにメッシュ結合
	void combine_unskinned_meshes(std::vector<mesh_data>& meshes, std::vector<node_data>& nodes);

	//	アニメーション読み込み
	void load_animations(std::vector<animation_data>& animations, const std::vector<node_data>& nodes);

public:	//	個別でほしかったので
	//	ボーン読み込み
	void load_node_bones(std::vector<bone_data>& bones, const convert_mesh_data& convert_data);

private:
	std::filesystem::path file_path;
	tinygltf::Model gltf_model;
};
