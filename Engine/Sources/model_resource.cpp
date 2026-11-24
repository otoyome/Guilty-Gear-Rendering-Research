#include <cmath>

#include "model_resource.h"
#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"

#include "animation_data_version.h"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(model_resource::node, 1)
CEREAL_CLASS_VERSION(model_resource::bone, 1)
CEREAL_CLASS_VERSION(model_resource::mesh, 2)
CEREAL_CLASS_VERSION(model_resource::capsule_shadow, 1)
CEREAL_CLASS_VERSION(model_resource, 1)

template<class Archive>
void model_resource::node::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(parent_index),
			CEREAL_NVP(scale),
			CEREAL_NVP(rotation),
			CEREAL_NVP(translation));
}

template<class Archive>
void model_resource::bone::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(node_index),
			CEREAL_NVP(offset_matrix));
}

template<class Archive>
void model_resource::mesh::sub_mesh::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(vertex_buffer_views),
			CEREAL_NVP(index_buffer_view));
}

template<class Archive>
void model_resource::mesh::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 2:
		{
			archive(CEREAL_NVP(bones),
					CEREAL_NVP(material_index),
					CEREAL_NVP(node_index),
					CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max),
					CEREAL_NVP(lod_meshs));
			break;
		}
		default:
		{
			archive(CEREAL_NVP(bones),
					CEREAL_NVP(material_index),
					CEREAL_NVP(node_index),
					CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max));
			break;
		}
	}
}

template<class Archive>
void model_resource::capsule_shadow::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(node_index),
			CEREAL_NVP(rotation),
			CEREAL_NVP(center),
			CEREAL_NVP(radius),
			CEREAL_NVP(length));
}

model_resource::model_resource()
: file_path()
, materials()
, meshes()
{
}

model_resource::model_resource(std::string_view file_path)
: file_path(file_path)
, materials()
, meshes()
{
}

model_resource::~model_resource()
{
}

bool model_resource::compute_animation_nodes(std::vector<animation::node>& nodes, size_t animation_index, float time) const
{
	nodes.clear();

	size_t node_index = 0;
	if (animation_index >= animations.size())
	{
		nodes.resize(animations[0].nodes.size());
		for (auto& node : animations[0].nodes)
		{
			nodes[node_index++] = node.nodes[0];
		}
		return false;
	}

	auto& animation = animations[animation_index];
	float calculate_time = math::clamp(time, 0.0f, animation.end_time);
	nodes.resize(animation.nodes.size());
	return animation.compute_animation(nodes, calculate_time);
}

bool model_resource::compute_animation_node(animation::node& node, size_t animation_index, size_t node_index, float time) const
{
	if (animation_index >= animations.size())
	{
		return false;
	}
	auto& animation = animations[animation_index];
	float calculate_time = math::clamp(time, 0.0f, animation.end_time);
	return animation.compute_animation(node, node_index, calculate_time);
}

size_t model_resource::get_animation_index_from_name(std::string_view name) const
{
	size_t index = 0;
	for (auto& animation : animations)
	{
		if (animation.name == name)
		{
			return index;
		}
		++index;
	}
	return invalid_id;
}

size_t model_resource::get_node_index_from_name(std::string_view name) const
{
	size_t index = 0;
	for (auto& node : nodes)
	{
		if (node.name == name)
		{
			return index;
		}
		++index;
	}
	return invalid_id;
}

bool model_resource::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if(ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(materials),
					CEREAL_NVP(nodes),
					CEREAL_NVP(meshes),
					CEREAL_NVP(animations),
					CEREAL_NVP(capsule_shadows));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::model, "model serialize failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG_CATEGORY(logger::categorys::model, "model serialize successed[%s]", file_path.data());
		return true;
	}

	DEBUG_LOG_CATEGORY(logger::categorys::model, "model open failed[%s]", file_path.data());
	return false;
}

bool model_resource::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if(istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(materials),
					CEREAL_NVP(nodes),
					CEREAL_NVP(meshes),
					CEREAL_NVP(animations),
					CEREAL_NVP(capsule_shadows));
		}
		catch (...)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::model, "model deserialize failed[%s]", file_path.data());
			return false;
		}

		//	モデル構築
		this->file_path = file_path;
		build_model();
		DEBUG_LOG_CATEGORY(logger::categorys::model, "model deserialize successed[%s]", file_path.data());
		return true;
	}

	DEBUG_ERROR_CATEGORY(logger::categorys::model, "model open failed[%s]", file_path.data());
	return false;
}

void model_resource::paste_materials(const std::vector<material>& materials)
{
	for (size_t index = 0; index < materials.size(); ++index)
	{
		materials[index].duplicate(this->materials[index]);
	}
}

void model_resource::build_model()
{
	HRESULT hr;

	//	マテリアル
	for (auto& material : materials)
	{
		material.build();
	}

	//	頂点・インデックスバッファ
	for (auto& mesh : meshes)
	{
		//	頂点バッファビュー生成
		for (auto& view : mesh.vertex_buffer_views)
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.ByteWidth = static_cast<UINT>(view.second.size_in_bytes);
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

			D3D11_SUBRESOURCE_DATA subresource_data{};
			subresource_data.pSysMem = view.second.row_data.data();
			hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &subresource_data, view.second.buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	インデックスバッファビュー生成
		{
			auto& view = mesh.index_buffer_view;

			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.ByteWidth = static_cast<UINT>(view.size_in_bytes);
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

			D3D11_SUBRESOURCE_DATA subresource_data{};
			subresource_data.pSysMem = view.row_data.data();
			hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &subresource_data, view.buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
model_resource_manageer::model_resource_manageer()
{
}

model_resource_manageer::~model_resource_manageer()
{
}

bool model_resource_manageer::load_model(model_resource_ptr& resource, std::string_view file_path)
{
	resource = std::make_shared<model_resource>();
	return resource->deserialize(file_path);
}
