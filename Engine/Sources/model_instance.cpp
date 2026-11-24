#include "model_instance.h"
#include "texture.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(USE_IMGUI)

namespace
{
}

model_instance::model_instance()
: system_flags(0)
, lod_index(-1)
, resource()
, materials()
, nodes()
{
	set_flag(flags::active, true);
	set_flag(flags::color_render, true);
	set_flag(flags::shadow_caster, true);
	set_flag(flags::culling, true);
	set_flag(flags::debug_draw_culling_object, false);
}

model_instance::model_instance(std::shared_ptr<model_resource> resource)
: model_instance()
{
	this->resource = resource;
	build_model();
}

model_instance::~model_instance()
{
}

bool model_instance::compute_animation_nodes(std::vector<node_animation>& nodes, size_t animation_index, float time) const
{
	if (!resource)
		return false;

	return resource->compute_animation_nodes(nodes, animation_index, time);
}

bool model_instance::compute_animation_node(node_animation& node, size_t animation_index, size_t node_index, float time) const
{
	if (!resource)
		return false;

	return resource->compute_animation_node(node, animation_index, node_index, time);
}

void model_instance::set_node_poses(const std::vector<node_animation>& node_animations)
{
	for (size_t index = 0; index < node_animations.size(); ++index)
	{
		auto& instance_node = *(nodes.data() + index);
		auto& animation_node = *(node_animations.data() + index);
		DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(animation_node.scale.x, animation_node.scale.y, animation_node.scale.z);
		DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&animation_node.rotation));
		DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(animation_node.translation.x, animation_node.translation.y, animation_node.translation.z);
		DirectX::XMStoreFloat4x4(&instance_node.local_transform, Scale * Rotation * Translation);
	}
}

void model_instance::set_node_pose(size_t node_index, const node_animation& node)
{
	if (node_index >= nodes.size())
	{
		assert(node_index < nodes.size());
		return;
	}

	auto& instance_node = nodes[node_index];
	DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z);
	DirectX::XMStoreFloat4x4(&instance_node.local_transform, Scale * Rotation * Translation);
}

void model_instance::update_transform(const DirectX::XMFLOAT4X4& world_transform)
{
	DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&world_transform);
	this->world_transform = world_transform;
	for (auto& node : nodes)
	{
		DirectX::XMMATRIX GlobalTransform = DirectX::XMLoadFloat4x4(&node.local_transform);
		if (node.parent_index != invalid_id)
		{
			auto& parent_node = nodes[node.parent_index];
			GlobalTransform *= DirectX::XMLoadFloat4x4(&parent_node.global_transform);
		}
		DirectX::XMStoreFloat4x4(&node.global_transform, GlobalTransform);
	}
}

void model_instance::update_transform(const DirectX::XMMATRIX& world_transform)
{
	DirectX::XMFLOAT4X4 matrix;
	DirectX::XMStoreFloat4x4(&matrix, world_transform);
	update_transform(matrix);
}

void model_instance::build_model()
{
	if(!resource)
		return;

	//	マテリアルをコピーしておく
	materials.reserve(resource->get_materials().size());
	for (auto& resource_material : resource->get_materials())
	{
		materials.emplace_back();
		resource_material.duplicate(materials.back());
	}

	//	ノード構築
	nodes.reserve(resource->get_nodes().size());
	for (auto& resource_node : resource->get_nodes())
	{
		node instance_node;
		instance_node.name = resource_node.name;
		instance_node.parent_index = resource_node.parent_index;
		DirectX::XMStoreFloat4x4(&instance_node.local_transform,
								DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&resource_node.scale)) *
								DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&resource_node.rotation)) *
								DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&resource_node.translation))
		);
		instance_node.global_transform = instance_node.local_transform;
		instance_node.previous_global_transform = instance_node.local_transform;
		nodes.emplace_back(instance_node);
	}

	//	メッシュ構築
	meshes.reserve(resource->get_meshes().size());
	for (UINT index = 0; index < static_cast<UINT>(resource->get_meshes().size()); ++index)
	{
		auto& mesh = meshes.emplace_back();
		mesh.mesh = &resource->get_meshes()[index];
		mesh.visible = true;
	}
}

size_t model_instance::get_node_index(std::string_view node_name) const
{
	for (size_t index = 0; index < nodes.size(); ++index)
	{
		auto& node = nodes[index];
		if (node.name == node_name)
			return index;
	}

	return model_resource::invalid_id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
model_instance_maanger::model_instance_maanger()
{
}

model_instance_maanger::~model_instance_maanger()
{
}

void model_instance_maanger::registered(std::weak_ptr<model_instance> instance)
{
	//	登録済みであれば登録しない
	auto it  = std::find_if(instances.begin(),
							instances.end(),
							[instance](std::weak_ptr<model_instance> value)
							{
								return system_utility::equals(value, instance);
							});
	if (it != instances.end())
		return;

	instances.emplace_back(instance);
}

void model_instance_maanger::get_container(mesh_container& container)
{
	container.clear();

	std::size_t primitive_buffer_count = 0;

	std::vector<DirectX::XMMATRIX> NodeWorldTransforms, PreviousNodeWorldTransforms;
	NodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);
	PreviousNodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);

	//	バッファ/マテリアルの配列生成
	std::size_t buffer_count = 0, material_count = 0;
	container.buffer_datas.resize(256);
	container.material_datas.resize(256);

	//	メッシュコンテナ生成
	auto it = instances.begin();
	for (; it != instances.end();)
	{
		if (it->expired())
		{
			it = instances.erase(it);
			continue;
		}

		auto instance = it->lock();
		++it;	//	次のインスタンスに移動させておく

		if (!instance->is_flag(model_instance::flags::active))
			continue;

		if (!instance->is_flag(model_instance::flags::color_render) &&
			!instance->is_flag(model_instance::flags::shadow_caster))
			continue;

		//	ワールド行列等生成
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
		DirectX::XMMATRIX PreviousWorldTransform = DirectX::XMLoadFloat4x4(&instance->get_previous_world_transform());

		//	ノードサイズ調整
		if (NodeWorldTransforms.size() <= instance->get_nodes().size())
		{
			NodeWorldTransforms.resize(instance->get_nodes().size());
			PreviousNodeWorldTransforms.resize(instance->get_nodes().size());
		}

		//	描画情報構築
		for (auto& instance_mesh : instance->get_meshes())
		{
			if (!instance_mesh.mesh || !instance_mesh.visible)
				continue;

			auto& mesh = *instance_mesh.mesh;
			if (mesh.material_index == model_resource::invalid_id)
				continue;

			//	必要な行列生成
			{
				bool inside = false;
				for (auto& bone : mesh.bones)
				{
					auto& node = instance->get_nodes()[bone.node_index];
					auto gm = DirectX::XMLoadFloat4x4(&node.global_transform);
					auto pgm = DirectX::XMLoadFloat4x4(&node.previous_global_transform);
					NodeWorldTransforms[bone.node_index] = gm * WorldTransform;
					PreviousNodeWorldTransforms[bone.node_index] = pgm * PreviousWorldTransform;
					if (mesh.node_index == bone.node_index)
						inside = true;
				}
				if (!inside)
				{
					auto& node = instance->get_nodes()[mesh.node_index];
					auto gm = DirectX::XMLoadFloat4x4(&node.global_transform);
					auto pgm = DirectX::XMLoadFloat4x4(&node.previous_global_transform);
					NodeWorldTransforms[mesh.node_index] = WorldTransform;
					PreviousNodeWorldTransforms[mesh.node_index] = PreviousWorldTransform;
				}
			}

			//	ワールド空間上のAABBを生成
			DirectX::XMFLOAT3 bounds_min, bounds_max;
			{
				bounds_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
				bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				DirectX::XMFLOAT3 scale((mesh.bounds_max.x - mesh.bounds_min.x) * 0.5f, (mesh.bounds_max.y - mesh.bounds_min.y) * 0.5f, (mesh.bounds_max.z - mesh.bounds_min.z) * 0.5f);
				DirectX::XMFLOAT3 center(mesh.bounds_min.x + scale.x, mesh.bounds_min.y + scale.y, mesh.bounds_min.z + scale.z);

				collision::bounding_box box = collision::bounding_box(scale, {0.0f, 0.0f, 0.0f, 1.0f}, center);

				DirectX::XMFLOAT3 vertices[8];
				box.calculate_vertex(vertices, ARRAYSIZE(vertices));
				for (auto& vertex : vertices)
				{
					DirectX::XMStoreFloat3(&vertex,
						DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertex),
											NodeWorldTransforms[mesh.node_index]));
					bounds_min.x = std::min(vertex.x, bounds_min.x);
					bounds_min.y = std::min(vertex.y, bounds_min.y);
					bounds_min.z = std::min(vertex.z, bounds_min.z);
					bounds_max.x = std::max(vertex.x, bounds_max.x);
					bounds_max.y = std::max(vertex.y, bounds_max.y);
					bounds_max.z = std::max(vertex.z, bounds_max.z);
				}
			}

#if defined(USE_IMGUI)
			//	カリングオブジェクトの表示
			if (draw_culling_mesh_aabb && instance->is_flag(model_instance::flags::debug_draw_culling_object))
			{
				DirectX::XMFLOAT3 scale((bounds_max.x - bounds_min.x) * 0.5f, (bounds_max.y - bounds_min.y) * 0.5f, (bounds_max.z - bounds_min.z) * 0.5f);
				DirectX::XMFLOAT4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
				DirectX::XMFLOAT3 center(bounds_min.x + scale.x, bounds_min.y + scale.y, bounds_min.z + scale.z);
				DirectX::XMFLOAT4 color = {1.0f, 0.0f, 0.0f, 1.0f};
				PUSH_DEBUG_PRIMITIVE_BOX(scale, rotation, center, color, true);
			}
#endif	//	defined(USE_IMGUI)

			//	バッファ情報構築
			if (buffer_count >= container.buffer_datas.size())
			{
				container.buffer_datas.resize(container.buffer_datas.size() * 2);
			}
			auto& buffer_data = container.buffer_datas[buffer_count];
			buffer_data.lod_range_rate = instance->get_lod_range_rate();
			buffer_data.bounds_min = bounds_min;
			buffer_data.bounds_max = bounds_max;
			buffer_data.system_flags = 0;
			system_utility::set_bitflag(buffer_data.system_flags, buffer_datas::flags::culling, instance->is_flag(model_instance::flags::culling));

			//	頂点バッファ系列登録
			{
				auto& main_mesh = buffer_data.color_meshs.emplace_back();
				main_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
				main_mesh.index_buffer_view = &mesh.index_buffer_view;
				main_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			}

			if (instance->get_lod_index() == -1)
			{
				//	LODメッシュがあれば登録
				for (auto& lod_mesh : mesh.lod_meshs)
				{
					auto& sub_mesh = buffer_data.color_meshs.emplace_back();
					sub_mesh.vertex_buffer_views = &lod_mesh.vertex_buffer_views;
					if (lod_mesh.vertex_buffer_views.empty())
					{
						sub_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
					}
					sub_mesh.index_buffer_view = &lod_mesh.index_buffer_view;
					if (lod_mesh.index_buffer_view.count() == 0)
					{
						sub_mesh.index_buffer_view = &mesh.index_buffer_view;
					}
					sub_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				}
			}
			else if (!mesh.lod_meshs.empty() && instance->get_lod_index() > 0)
			{
				//	LODメッシュを強制適用
				int index = std::min(instance->get_lod_index() - 1, static_cast<int>(mesh.lod_meshs.size() - 1));
				auto& sub_mesh = buffer_data.color_meshs.back();
				if (!mesh.lod_meshs[index].vertex_buffer_views.empty())
				{
					sub_mesh.vertex_buffer_views = &mesh.lod_meshs[index].vertex_buffer_views;
				}
				if (!mesh.lod_meshs[index].index_buffer_view.count() == 0)
				{
					sub_mesh.index_buffer_view = &mesh.lod_meshs[index].index_buffer_view;
				}
			}

			//	変換行列登録
			{
				auto& current_transform_matrices = buffer_data.transform_matrices[static_cast<int>(buffer_datas::transform_matrix_index::current)];
				auto& previous_transform_matrices = buffer_data.transform_matrices[static_cast<int>(buffer_datas::transform_matrix_index::previous)];
				current_transform_matrices.reserve(mesh.bones.size());
				previous_transform_matrices.reserve(mesh.bones.size());
				for (auto& bone : mesh.bones)
				{
					DirectX::XMMATRIX NodeOffsetTransform = DirectX::XMLoadFloat4x4(&bone.offset_matrix);
					auto& node = instance->get_nodes()[bone.node_index];
					current_transform_matrices.emplace_back();
					previous_transform_matrices.emplace_back();

					DirectX::XMMATRIX NodeWorldTransform = NodeWorldTransforms[bone.node_index];
					DirectX::XMStoreFloat4x4(&current_transform_matrices.back(), NodeOffsetTransform * NodeWorldTransform);

					NodeWorldTransform = PreviousNodeWorldTransforms[bone.node_index];
					DirectX::XMStoreFloat4x4(&previous_transform_matrices.back(), NodeOffsetTransform * NodeWorldTransform);
				}

				//	プリミティブ用定数バッファ登録
				if (primitive_buffer_count + 1 >= constant_bufferes.size())
				{
					generate_constant_bufferes();
				}
				buffer_data.primitive_buffer = constant_bufferes[primitive_buffer_count++];
			}

			//	マテリアル情報構築
			if (material_count >= container.material_datas.size())
			{
				container.material_datas.resize(container.material_datas.size() * 2);
			}
			auto& material_data = container.material_datas[material_count];
			material_data.material = instance->get_materials().data() + mesh.material_index;

			//	描画情報構築
			container.draw_datas.emplace_back();
			auto& draw_data = container.draw_datas.back();
			draw_data.buffer_index = static_cast<UINT>(buffer_count);
			draw_data.material_index = static_cast<UINT>(material_count);
			++buffer_count;
			++material_count;
			if (instance->is_flag(model_instance::flags::color_render))
			{
				container.color_datas.emplace_back(container.draw_datas.size() - 1);
			}

			//	影描画するなら登録
			if (instance->is_flag(model_instance::flags::shadow_caster) &&
				material_data.material->is_flag(material::flags::shadow_caster))
			{
				//	影メッシュがあれば登録
				buffer_data.shadow_mesh.vertex_buffer_views = &mesh.shadow_mesh.vertex_buffer_views;
				if (mesh.shadow_mesh.vertex_buffer_views.empty())
				{
					buffer_data.shadow_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
				}
				buffer_data.shadow_mesh.index_buffer_view = &mesh.shadow_mesh.index_buffer_view;
				if (mesh.shadow_mesh.index_buffer_view.size_in_bytes == 0)
				{
					buffer_data.shadow_mesh.index_buffer_view = &mesh.index_buffer_view;
				}
				buffer_data.shadow_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

				container.shadow_datas.emplace_back(container.draw_datas.size() - 1);

				//if (instance->get_resource()->get_capsule_shadows().empty())
				//{
				//	//	カプセルシャドウオブジェクトがなければ通常シャドウ
				//	container.shadow_datas.emplace_back(container.shadow_datas.size() - 1);
				//}
				//else if (!in_sight)	//	カプセルシャドウは、メッシュが見えている場合のみ登録
				//{
				//	for (auto& capsule : instance->get_resource()->get_capsule_shadows())
				//	{
				//		DirectX::XMFLOAT4X4 node_world_transform = instance->get_nodes()[capsule.node_index].world_transform;
				//		capsule.capsule.
				//		container.capsule_shadow_datas.emplace_back();
				//	}
				//}
			}
		}

		//	過去行列を保存
		{
			DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
			for (auto& node : instance->get_nodes())
			{
				node.previous_global_transform = node.global_transform;
			}
			instance->set_previous_world_transform(instance->get_world_transform());
		}
	}

	//	バッファリサイズ
	container.buffer_datas.resize(buffer_count);
	container.material_datas.resize(material_count);
}

void model_instance_maanger::generate_constant_bufferes()
{
	auto render_data = render_data::instance();
	auto device = render_data->get_device();

	HRESULT hr;
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	constant_bufferes.emplace_back();
	auto& buffer = constant_bufferes.back();
	buffer_desc.ByteWidth = sizeof(common_pipeline_states::primitive_constants);
	hr = device->CreateBuffer(&buffer_desc, nullptr, buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
