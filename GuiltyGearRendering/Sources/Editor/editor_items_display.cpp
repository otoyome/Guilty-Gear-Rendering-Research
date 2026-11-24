#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>

void editor_items::render_model_bones(const model_instance& model_instance, const std::vector<std::string_view>& selected_bones)
{
	std::vector<const char*> list;
	for (auto& selected_bone : selected_bones)
	{
		list.push_back(selected_bone.data());
	}
	editor_items::render_model_bones(model_instance, list.data(), list.size());
}

void editor_items::render_model_bones(const model_instance& model_instance, const std::vector<std::string>& selected_bones)
{
	std::vector<const char*> list;
	for (auto& selected_bone : selected_bones)
	{
		list.push_back(selected_bone.data());
	}
	editor_items::render_model_bones(model_instance, list.data(), list.size());
}

void editor_items::render_model_bones(const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size)
{
	auto& nodes = model_instance.get_nodes();
	auto& meshes = model_instance.get_meshes();

	auto is_select_bone = [&](std::string_view selected_bone_name)
	{
		for (UINT index = 0; index < selected_bone_size; ++index)
		{
			if (selected_bone_name == std::string_view(selected_bones[index]))
			{
				return true;
			}
		}
		return false;
	};

	DirectX::XMMATRIX WorldMatrix = DirectX::XMLoadFloat4x4(&model_instance.get_world_transform());
	for (UINT node_index = 0; node_index < nodes.size(); ++node_index)
	{
		auto& node = nodes[node_index];

		//	メッシュノードはいったん除外
		{
			bool find = false;
			for (UINT mesh_node_index = 0; mesh_node_index < meshes.size(); ++mesh_node_index)
			{
				if (node_index == meshes[mesh_node_index].mesh->node_index)
				{
					find = true;
					break;
				}
			}
			if (find)
				continue;
		}

		DirectX::XMMATRIX NodeWorldMatrix = DirectX::XMLoadFloat4x4(&node.global_transform) * WorldMatrix;
		DirectX::XMFLOAT3 start;
		DirectX::XMStoreFloat3(&start, NodeWorldMatrix.r[3]);

		std::vector<size_t> children;
		for (UINT child_node_index = 0; child_node_index < nodes.size(); ++child_node_index)
		{
			if (node_index == nodes[child_node_index].parent_index)
			{
				children.push_back(child_node_index);
			}
		}

		DirectX::XMFLOAT4 color = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		if (is_select_bone(node.name.data()))
		{
			color = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		}

		//	軸描画
		{
			DirectX::XMVECTOR Scale, Rotation, Translation;
			DirectX::XMMatrixDecompose(&Scale, &Rotation, &Translation, NodeWorldMatrix);
			DirectX::XMFLOAT4 rotation;
			DirectX::XMStoreFloat4(&rotation, Rotation);
			auto rotator = math::rotator(rotation);
			auto front = rotator.get_front();
			auto right = rotator.get_right();
			auto up = rotator.get_up();

			DirectX::XMFLOAT3 end[3];
			static constexpr float joint_length = 0.3f;
			end[0].x = start.x + right.x * joint_length;
			end[0].y = start.y + right.y * joint_length;
			end[0].z = start.z + right.z * joint_length;

			end[1].x = start.x + up.x * joint_length;
			end[1].y = start.y + up.y * joint_length;
			end[1].z = start.z + up.z * joint_length;

			end[2].x = start.x + front.x * joint_length;
			end[2].y = start.y + front.y * joint_length;
			end[2].z = start.z + front.z * joint_length;
			PUSH_DEBUG_PRIMITIVE_LINE(start, end[0], DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), false);
			PUSH_DEBUG_PRIMITIVE_LINE(start, end[1], DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), false);
			PUSH_DEBUG_PRIMITIVE_LINE(start, end[2], DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), false);
		}

		//	子供なしの時
		if (children.empty())
		{
			PUSH_DEBUG_PRIMITIVE_SPHERE(start, 0.1f, color, false);
			continue;
		}

		//	子供有りの時
		for (auto& child : children)
		{
			auto& child_node = nodes[child];
			DirectX::XMFLOAT3 end;
			DirectX::XMStoreFloat3(&end, (DirectX::XMLoadFloat4x4(&child_node.global_transform) * WorldMatrix).r[3]);

			DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
			DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
			DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(End, Start);
			float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
			if (length <= 0.000001f)
			{
				PUSH_DEBUG_PRIMITIVE_SPHERE(start, 0.1f, color, false);
				continue;
			}

			DirectX::XMVECTOR Front = DirectX::XMVector3Normalize(Vector);
			DirectX::XMVECTOR Up = DirectX::XMVectorSet(0, 1, 0, 0);
			if (fabsf(fabsf(DirectX::XMVectorGetZ(Front)) - 1) <= 0.000001f)
			{
				Up = DirectX::XMVectorSet(0, 0, -DirectX::XMVectorGetZ(Front), 0);
			}
			DirectX::XMVECTOR Right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Front, Up));
			Up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Right, Front));
			Right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Front, Up));
			DirectX::XMFLOAT3 front, right, up;
			DirectX::XMStoreFloat3(&right, Right);
			DirectX::XMStoreFloat3(&up, Up);
			DirectX::XMStoreFloat3(&front, Front);

			DirectX::XMFLOAT3 center, cpxpy, cpxmy, cmxpy, cmxmy;
			float bone_length = 0.1f * std::min(1.0f, length);
			center.x = start.x + front.x * length * bone_length;
			center.y = start.y + front.y * length * bone_length;
			center.z = start.z + front.z * length * bone_length;

			cpxpy.x = center.x + right.x * bone_length + up.x * bone_length;
			cpxpy.y = center.y + right.y * bone_length + up.y * bone_length;
			cpxpy.z = center.z + right.z * bone_length + up.z * bone_length;
			cpxmy.x = center.x + right.x * bone_length - up.x * bone_length;
			cpxmy.y = center.y + right.y * bone_length - up.y * bone_length;
			cpxmy.z = center.z + right.z * bone_length - up.z * bone_length;

			cmxmy.x = center.x - right.x * bone_length + up.x * bone_length;
			cmxmy.y = center.y - right.y * bone_length + up.y * bone_length;
			cmxmy.z = center.z - right.z * bone_length + up.z * bone_length;
			cmxpy.x = center.x - right.x * bone_length - up.x * bone_length;
			cmxpy.y = center.y - right.y * bone_length - up.y * bone_length;
			cmxpy.z = center.z - right.z * bone_length - up.z * bone_length;

			PUSH_DEBUG_PRIMITIVE_LINE(start, cpxpy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(start, cpxmy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(start, cmxpy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(start, cmxmy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cpxpy, cpxmy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cpxmy, cmxpy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cmxpy, cmxmy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cmxmy, cpxpy, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cpxpy, end, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cpxmy, end, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cmxpy, end, color, false);
			PUSH_DEBUG_PRIMITIVE_LINE(cmxmy, end, color, false);
		}
	}
}

#endif	//	defined(USE_IMGUI)


