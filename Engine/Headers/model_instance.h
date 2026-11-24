#pragma once

#include "model_resource.h"
#include "collision/frustum_culling.h"

#include "collision/sphere.h"
#include "collision/capsule.h"
#include "collision/box.h"

//	モデルインスタンス
class model_instance
{
public:
	static constexpr size_t invalid_id = -1;

	enum class flags
	{
		active,						//	表示フラグ
		color_render,				//	色描画
		shadow_caster,				//	影生成
		culling,					//	カリング
		self_shadow_off,			//	セルフシャドウオフ
		use_capsule_shadow,			//	カプセルシャドウ使用

		debug_draw_culling_object,	//	カリングオブジェクトの表示フラグ
	};

	struct node
	{
		std::string name;
		bool dirty = true;
		size_t parent_index = invalid_id;

		DirectX::XMFLOAT4X4 local_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
		DirectX::XMFLOAT4X4 global_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

		DirectX::XMFLOAT4X4 previous_global_transform{1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
	};

	struct mesh
	{
		const model_resource::mesh* mesh;
		bool visible;
	};

	using node_animation = animation::node;

public:
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	int get_lod_index() const {return lod_index;}
	void set_lod_index(int lod_index)
	{
		this->lod_index = lod_index;
	}
	//	
	int get_lod_max() const
	{
		if (meshes.empty())
			return 0;

		return meshes.begin()->mesh ? static_cast<int>(meshes.begin()->mesh->lod_meshs.size()) : 0;
	}
	const DirectX::XMFLOAT2& get_lod_range_rate() const {return lod_range_rate;}
	void set_lod_range_rate(DirectX::XMFLOAT2 rate) {lod_range_rate = math::saturate(rate);}

	inline model_resource_ptr get_resource() const { return resource; }
	inline const std::vector<node>& get_nodes() const { return nodes; }
	inline std::vector<node>& get_nodes() { return nodes; }
	inline const std::vector<material>& get_materials() const { return materials; }
	inline std::vector<material>& get_materials() { return materials; }
	inline const std::vector<mesh>& get_meshes() const { return meshes; }
	inline std::vector<mesh>& get_meshes() { return meshes; }
	inline DirectX::XMFLOAT4X4& get_world_transform() { return world_transform; }
	inline const DirectX::XMFLOAT4X4& get_world_transform() const { return world_transform; }
	inline const DirectX::XMFLOAT4X4& get_previous_world_transform() const { return previous_world_transform; }
	inline void set_previous_world_transform(const DirectX::XMFLOAT4X4& transform){ previous_world_transform = transform; }

#if defined(USE_IMGUI)
	inline void set_materials_for_editor(const std::vector<material>& materials) { this->materials = materials; }
#endif	//	defined(USE_IMGUI)

	size_t get_node_index(std::string_view node_name) const;

public:
	model_instance();
	model_instance(model_resource_ptr resource);
	~model_instance();

	bool compute_animation_nodes(std::vector<node_animation>& nodes, size_t animation_index, float time) const;
	bool compute_animation_node(node_animation& node, size_t animation_index, size_t node_index, float time) const;
	void set_node_poses(const std::vector<node_animation>& nodes);
	void set_node_pose(size_t node_index, const node_animation& node);
	void update_transform(const DirectX::XMFLOAT4X4& world_transform);
	void update_transform(const DirectX::XMMATRIX& world_transform);

private:
	void build_model();

private:
	UINT system_flags;
	int lod_index;
	DirectX::XMFLOAT2 lod_range_rate = {0.0f, 1.0f};
	model_resource_ptr resource;
	DirectX::XMFLOAT4X4 world_transform{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
	DirectX::XMFLOAT4X4 previous_world_transform{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

	std::vector<material> materials;
	std::vector<node> nodes;
	std::vector<mesh> meshes;
};

using model_instance_ptr = std::shared_ptr<model_instance>;

//	モデルインスタンスマネージャー
class model_instance_maanger
{
	SINGLETON(model_instance_maanger)

	using	model_instances	= std::vector<std::weak_ptr<model_instance>>;

public:
	inline const model_instances& get_instances() const { return instances; }

#if defined(USE_IMGUI)
	bool get_draw_culling_mesh_aabb() const {return draw_culling_mesh_aabb;}
	void set_draw_culling_mesh_aabb(bool flag){draw_culling_mesh_aabb = flag;}
#endif	//	defined(USE_IMGUI)

private:
	model_instance_maanger();
	~model_instance_maanger();

	void generate_constant_bufferes();

public:
	void registered(std::weak_ptr<model_instance> instance);

	void get_container(mesh_container& container);

private:
	model_instances instances;
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constant_bufferes;

#if defined(USE_IMGUI)
	bool draw_culling_mesh_aabb = true;
#endif	//	defined(USE_IMGUI)
};
