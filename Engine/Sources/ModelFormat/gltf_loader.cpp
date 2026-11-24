#include "ModelFormat/gltf_loader.h"

#define TINYGLTF_IMPLEMENTATION
#include "tinygltf-release/tiny_gltf.h"

#include "DirectXTex.h"


namespace
{
	enum class gltf_semantic_index
	{
		POSITION,
		NORMAL,
		TANGENT,
		TEXCOORD,
		COLOR,
		JOINTS,
		WEIGHTS,
	};
	static constexpr std::string_view gltf_semantic_names[] =
	{
		"POSITION",
		"NORMAL",
		"TANGENT",
		"TEXCOORD",
		"COLOR",
		"JOINTS",
		"WEIGHTS",
	};

	DirectX::XMFLOAT3 convert_gltf_vector3_to_xmfloat3(const std::vector<double>& vec)
	{
		return DirectX::XMFLOAT3(static_cast<float>(vec.at(0)), static_cast<float>(vec.at(1)), static_cast<float>(vec.at(2)));
	}

	DirectX::XMFLOAT4 convert_gltf_quaternion_to_xmfloat4(const std::vector<double>& quat)
	{
		return DirectX::XMFLOAT4(static_cast<float>(quat.at(0)), static_cast<float>(quat.at(1)), static_cast<float>(quat.at(2)), static_cast<float>(quat.at(3)));
	}

	DirectX::XMFLOAT4X4 convert_gltf_matrix_to_xmfloat4x4(const std::vector<double>& mat)
	{
		return DirectX::XMFLOAT4X4(static_cast<float>(mat.at(0)), static_cast<float>(mat.at(1)), static_cast<float>(mat.at(2)), static_cast<float>(mat.at(3)),
								   static_cast<float>(mat.at(4)), static_cast<float>(mat.at(5)), static_cast<float>(mat.at(6)), static_cast<float>(mat.at(7)),
								   static_cast<float>(mat.at(8)), static_cast<float>(mat.at(9)), static_cast<float>(mat.at(10)), static_cast<float>(mat.at(11)),
								   static_cast<float>(mat.at(12)), static_cast<float>(mat.at(13)), static_cast<float>(mat.at(14)), static_cast<float>(mat.at(15)));
	}

	void convert_vector_cordinate_axis_system(DirectX::XMFLOAT3& vec)
	{
		vec.x = -vec.x;
	}

	void convert_vector_cordinate_axis_system(DirectX::XMFLOAT4& vec)
	{
		vec.x = -vec.x;
	}

	void convert_quaternion_cordinate_axis_system(DirectX::XMFLOAT4& quat)
	{
		quat.x = -quat.x;
		quat.w = -quat.w;
	}

	void convert_matrix_cordinate_axis_system(DirectX::XMFLOAT4X4& mat)
	{
		mat._12 = -mat._12;
		mat._13 = -mat._13;
		mat._21 = -mat._21;
		mat._31 = -mat._31;
		mat._41 = -mat._41;
	}

	void convert_node_axis_system(gltf_loader::node_data& node)
	{
		convert_vector_cordinate_axis_system(node.translation);
		convert_quaternion_cordinate_axis_system(node.rotation);
	}

	void convert_vector3_cordinate_axis_system(gltf_loader::mesh_data& mesh, std::string_view semantic_name)
	{
		UINT semantic_index = 0;

		while (semantic_index < mesh.vertex_buffers.size())
		{
			auto it = mesh.vertex_buffers.find({ semantic_name, semantic_index });
			if (it != mesh.vertex_buffers.end())
			{
				DirectX::XMFLOAT3* ptr = reinterpret_cast<DirectX::XMFLOAT3*>(it->second.row_data.data());
				for (UINT index = 0; index < it->second.count(); ++index)
				{
					convert_vector_cordinate_axis_system(ptr[index]);
				}
			}
			++semantic_index;
		}
	}

	void convert_vector4_cordinate_axis_system(gltf_loader::mesh_data& mesh, std::string_view semantic_name)
	{
		UINT semantic_index = 0;

		while (semantic_index < mesh.vertex_buffers.size())
		{
			auto it = mesh.vertex_buffers.find({ semantic_name, semantic_index });
			if (it != mesh.vertex_buffers.end())
			{
				DirectX::XMFLOAT4* ptr = reinterpret_cast<DirectX::XMFLOAT4*>(it->second.row_data.data());
				for (UINT index = 0; index < it->second.count(); ++index)
				{
					convert_vector_cordinate_axis_system(ptr[index]);
				}
			}
			++semantic_index;
		}
	}
	template<typename type>
	void convert_index_buffer_cordinate_axis_system(gltf_loader::mesh_data& mesh)
	{
		type* ptr = reinterpret_cast<type*>(mesh.index_buffer.row_data.data());
		for (UINT index = 0; index < mesh.index_buffer.count(); index += 3)
		{
			type temp = ptr[index + 1];
			ptr[index + 1] = ptr[index + 2];
			ptr[index + 2] = temp;
		}
	}
	void convert_mesh_cordinate_axis_system(gltf_loader::mesh_data& mesh, const gltf_loader::convert_mesh_data& convert_data)
	{
		//	座標系変換フラグ
		if (convert_data.is_change_coordinate_system)
		{
			//	ベクトル変換
			convert_vector3_cordinate_axis_system(mesh, gltf_semantic_names[static_cast<int>(gltf_semantic_index::POSITION)]);
			convert_vector3_cordinate_axis_system(mesh, gltf_semantic_names[static_cast<int>(gltf_semantic_index::NORMAL)]);
			convert_vector4_cordinate_axis_system(mesh, gltf_semantic_names[static_cast<int>(gltf_semantic_index::TANGENT)]);

			//	ボーン変換
			for (gltf_loader::bone_data& bone : mesh.bones)
			{
				convert_matrix_cordinate_axis_system(bone.offset_matrix);
			}
		}

		//	インデックスバッファのフリップ処理
		if (convert_data.is_change_coordinate_system_indices)
		{
			switch(direct_x_helper::get_format_stride(mesh.index_buffer.format))
			{
			case 1: convert_index_buffer_cordinate_axis_system<UCHAR>(mesh); break;
			case 2: convert_index_buffer_cordinate_axis_system<USHORT>(mesh); break;
			case 4: convert_index_buffer_cordinate_axis_system<UINT>(mesh); break;
			default: assert(!L"convert_mesh_axis_system index_buffer not compatible format.\n"); break;
			}
		}
	}

	void convert_animation_axis_system(gltf_loader::animation_data& data)
	{
		for (gltf_loader::node_animation_data& node_animation : data.nodes)
		{
			for (gltf_loader::vector_key_frame& key_frame : node_animation.translations)
			{
				convert_vector_cordinate_axis_system(key_frame.value);
			}
			for (gltf_loader::quaternion_key_frame& key_frame : node_animation.rotations)
			{
				convert_quaternion_cordinate_axis_system(key_frame.value);
			}
		}
	}

	DXGI_FORMAT convert_from_tiny_image_to_direct_x_image(const tinygltf::Image& image)
	{
		int bit_index = -1;
		switch (image.bits)
		{
		case 8: bit_index = 0; break;
		case 16: bit_index = 1; break;
		case 32: bit_index = 2; break;
		default:
			assert(!L"Compatible bit type.");
			return DXGI_FORMAT_UNKNOWN;
		}

		int component_index = -1;
		switch (image.component)
		{
		case 1: component_index = 0; break;
		case 2: component_index = 1; break;
		case 3: component_index = 2; break;
		case 4: component_index = 3; break;
		default:
			assert(!L"Compatible component type.");
			return DXGI_FORMAT_UNKNOWN;
		}

		int pixel_index = -1;
		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		case TINYGLTF_COMPONENT_TYPE_INT:
			pixel_index = 0;
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			pixel_index = 1;
			break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			pixel_index = 2;
			break;
		default:
			assert(!L"Compatible pixel_type type.");
			return DXGI_FORMAT_UNKNOWN;
		}

		static constexpr DXGI_FORMAT format_list[3][4][3] =
		{
			{
				{ DXGI_FORMAT_R8_SINT, DXGI_FORMAT_R8_UINT, DXGI_FORMAT_UNKNOWN },
				{ DXGI_FORMAT_R8G8_SINT, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_UNKNOWN },
				{ DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN },
				{ DXGI_FORMAT_R8G8B8A8_SINT, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_UNKNOWN },
			},
			{
				{ DXGI_FORMAT_R16_SINT, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_FLOAT },
				{ DXGI_FORMAT_R16G16_SINT, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16_FLOAT },
				{ DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN },
				{ DXGI_FORMAT_R16G16B16A16_SINT, DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_FLOAT },
			},
			{
				{ DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_FLOAT },
				{ DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_FLOAT },
				{ DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_FLOAT },
				{ DXGI_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_FLOAT },
			},
		};

		return format_list[bit_index][component_index][pixel_index];
	}

	// 接線計算
	void compute_tangent(std::unordered_map<semantic, gltf_loader::buffer_data>& vertex_buffers,
						 const gltf_loader::buffer_data& index_buffer)
	{
		//	http://marupeke296.com/DXPS_No12_CalcTangentVectorSpace.html
		auto it_positions = vertex_buffers.find({gltf_semantic_names[static_cast<int>(gltf_semantic_index::POSITION)], 0});
		auto it_texcoord = vertex_buffers.find({gltf_semantic_names[static_cast<int>(gltf_semantic_index::TEXCOORD)], 0});
		auto it_normals = vertex_buffers.find({gltf_semantic_names[static_cast<int>(gltf_semantic_index::NORMAL)], 0});

		//	どれか一つでもなければ生成出来ない
		if (it_positions == vertex_buffers.end() ||
			it_texcoord == vertex_buffers.end() ||
			it_normals == vertex_buffers.end())
		{
			return;
		}

		auto data_positions = reinterpret_cast<const DirectX::XMFLOAT3*>(it_positions->second.row_data.data());
		auto data_texcoords = reinterpret_cast<const DirectX::XMFLOAT2*>(it_texcoord->second.row_data.data());
		auto data_normals = reinterpret_cast<const DirectX::XMFLOAT3*>(it_normals->second.row_data.data());

		auto calculate_tangent_and_binormal	= [](DirectX::XMFLOAT3* positions, DirectX::XMFLOAT2* texcorrds, DirectX::XMFLOAT3* normals,
												 DirectX::XMFLOAT3& tangent, DirectX::XMFLOAT3& binormal)
		{
			DirectX::XMVECTOR Parameters[3][3]	=
			{
				{
					DirectX::XMVectorSet(positions[0].x, texcorrds[0].x, texcorrds[0].y, 0),
					DirectX::XMVectorSet(positions[0].y, texcorrds[0].x, texcorrds[0].y, 0),
					DirectX::XMVectorSet(positions[0].z, texcorrds[0].x, texcorrds[0].y, 0)
				},
				{
					DirectX::XMVectorSet(positions[1].x, texcorrds[1].x, texcorrds[1].y, 0),
					DirectX::XMVectorSet(positions[1].y, texcorrds[1].x, texcorrds[1].y, 0),
					DirectX::XMVectorSet(positions[1].z, texcorrds[1].x, texcorrds[1].y, 0)
				},
				{
					DirectX::XMVectorSet(positions[2].x, texcorrds[2].x, texcorrds[2].y, 0),
					DirectX::XMVectorSet(positions[2].y, texcorrds[2].x, texcorrds[2].y, 0),
					DirectX::XMVectorSet(positions[2].z, texcorrds[2].x, texcorrds[2].y, 0)
				},
			};

			//	平面パラメーターからUV軸算出
			float u[3], v[3];
			for (UINT index = 0; index < 3; ++index)
			{
				DirectX::XMVECTOR V1 = DirectX::XMVectorSubtract(Parameters[1][index], Parameters[0][index]);
				DirectX::XMVECTOR V2 = DirectX::XMVectorSubtract(Parameters[2][index], Parameters[0][index]);
				DirectX::XMVECTOR ABC = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(V1, V2));
				if (DirectX::XMVectorGetX(ABC) == 0)
				{
					//	縮退ポリゴンの場合はうまく算出できない
					DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(normals);
					DirectX::XMVECTOR Binormal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Normal, DirectX::XMVectorSet(0, 1, 0, 0)));
					DirectX::XMVECTOR Tangent = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Normal, Binormal));
					Binormal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Normal, Tangent));
					DirectX::XMStoreFloat3(&tangent, Tangent);
					DirectX::XMStoreFloat3(&binormal, Binormal);
					return;
				}
				else
				{
					u[index] = -DirectX::XMVectorGetY(ABC) / DirectX::XMVectorGetX(ABC);
					v[index] = -DirectX::XMVectorGetZ(ABC) / DirectX::XMVectorGetX(ABC);
				}
			}
			DirectX::XMStoreFloat3(&tangent, DirectX::XMVector3Normalize(DirectX::XMVectorSet(u[0], u[1], u[2], 0)));
			DirectX::XMStoreFloat3(&binormal, DirectX::XMVector3Normalize(DirectX::XMVectorSet(v[0], v[1], v[2], 0)));
		};

		//	各頂点の接線と従法線の合計を算出
		std::vector<DirectX::XMFLOAT3> tangents, binormals;
		tangents.resize(it_positions->second.count());
		binormals.resize(it_positions->second.count());
		for (UINT index = 0; index < index_buffer.count(); index += 3)
		{
			//	頂点番号
			UINT indices[3];
			if (index_buffer.stride_in_bytes == 1)
			{
				const UCHAR* ptr = index_buffer.row_data.data();
				indices[0] = ptr[0];
				indices[1] = ptr[1];
				indices[2] = ptr[2];
			}
			else if (index_buffer.stride_in_bytes == 2)
			{
				const USHORT* ptr = reinterpret_cast<const USHORT*>(index_buffer.row_data.data());
				indices[0] = ptr[0];
				indices[1] = ptr[1];
				indices[2] = ptr[2];
			}
			else if (index_buffer.stride_in_bytes == 4)
			{
				const UINT* ptr = reinterpret_cast<const UINT*>(index_buffer.row_data.data());
				indices[0] = ptr[0];
				indices[1] = ptr[1];
				indices[2] = ptr[2];
			}
			else
			{
				assert(!"not compatible data.");
				continue;
			}

			//	接線算出
			DirectX::XMFLOAT3 positions[3] = {data_positions[indices[0]], data_positions[indices[1]], data_positions[indices[2]]};
			DirectX::XMFLOAT2 texcoords[3] ={data_texcoords[indices[0]], data_texcoords[indices[1]], data_texcoords[indices[2]]};
			DirectX::XMFLOAT3 normals[3] ={data_normals[indices[0]], data_normals[indices[1]], data_normals[indices[2]]};
			DirectX::XMFLOAT3 tangent, binormal;
			calculate_tangent_and_binormal(positions, texcoords, normals, tangent, binormal);

			//	合計から求める
			tangents[indices[0]].x += tangent.x;
			tangents[indices[0]].y += tangent.y;
			tangents[indices[0]].z += tangent.z;
			tangents[indices[1]].x += tangent.x;
			tangents[indices[1]].y += tangent.y;
			tangents[indices[1]].z += tangent.z;
			tangents[indices[2]].x += tangent.x;
			tangents[indices[2]].y += tangent.y;
			tangents[indices[2]].z += tangent.z;
			binormals[indices[0]].x += binormal.x;
			binormals[indices[0]].y += binormal.y;
			binormals[indices[0]].z += binormal.z;
			binormals[indices[1]].x += binormal.x;
			binormals[indices[1]].y += binormal.y;
			binormals[indices[1]].z += binormal.z;
			binormals[indices[2]].x += binormal.x;
			binormals[indices[2]].y += binormal.y;
			binormals[indices[2]].z += binormal.z;
		}

		//	頂点バッファに格納
		gltf_loader::buffer_data buffer_data;
		buffer_data.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		buffer_data.offset_in_bytes = 0;
		buffer_data.stride_in_bytes = graphics_utility::get_byte_offset_from_format(buffer_data.format);
		buffer_data.size_in_bytes = buffer_data.stride_in_bytes * it_positions->second.count();
		buffer_data.row_data.resize(buffer_data.size_in_bytes);
		auto data_tangents = reinterpret_cast<DirectX::XMFLOAT4*>(buffer_data.row_data.data());
		for (std::size_t index = 0; index < buffer_data.count(); ++index)
		{
			DirectX::XMVECTOR T = DirectX::XMLoadFloat3(tangents.data() + index);
			DirectX::XMVECTOR B = DirectX::XMLoadFloat3(binormals.data() + index);
			DirectX::XMVECTOR N = DirectX::XMLoadFloat3(data_normals + index);

			//	グラムシュミット直交化
			T = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(T, DirectX::XMVectorScale(N, DirectX::XMVectorGetX(DirectX::XMVector3Dot(N, T)))));

			//	符号算出
			float handedness = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(N, T), B)) < 0.0f) ? -1.0f : 1.0f;
			DirectX::XMStoreFloat4(data_tangents + index, DirectX::XMVectorSetW(T, handedness));
		}
		vertex_buffers.insert_or_assign(semantic(gltf_semantic_names[static_cast<int>(gltf_semantic_index::TANGENT)], 0), buffer_data);
	}

	bool load_image_function(tinygltf::Image*, const int, std::string*, std::string*, int, int, const unsigned char*, int, void*)
	{
		return true;
	}

	std::vector<std::string> split(const std::string& str, char delimiter)
	{
		std::vector<std::string> result;
		size_t start = 0;
		size_t end = str.find(delimiter);

		while (end != std::string::npos)
		{
			result.push_back(str.substr(start, end - start));
			start = end + 1;
			end = str.find(delimiter, start);
		}

		result.push_back(str.substr(start));
		return result;
	}

}

gltf_loader::node_animation_data::pose_data gltf_loader::node_animation_data::calculate(float second)
{
	pose_data pose;
	bool is_insert = false;
	for (UINT index = 0; index < scales.size() - 1; ++index)
	{
		auto& data1 = scales[index + 0];
		auto& data2 = scales[index + 1];
		if (data1.second <= second && second < data2.second)
		{
			is_insert = true;
			float rate = (second - data1.second) / (data2.second - data1.second);
			pose.scale.x = data1.value.x * (1.0f - rate) + data2.value.x * rate;
			pose.scale.y = data1.value.y * (1.0f - rate) + data2.value.y * rate;
			pose.scale.z = data1.value.z * (1.0f - rate) + data2.value.z * rate;
			break;
		}
	}
	if (!is_insert && second >= scales.back().second)
	{
		pose.scale = scales.back().value;
	}
	is_insert = false;
	for (UINT index = 0; index < rotations.size() - 1; ++index)
	{
		auto& data1 = rotations[index + 0];
		auto& data2 = rotations[index + 1];
		if (data1.second <= second && second < data2.second)
		{
			float rate = (second - data1.second) / (data2.second - data1.second);
			DirectX::XMVECTOR R1 = DirectX::XMLoadFloat4(&data1.value);
			DirectX::XMVECTOR R2 = DirectX::XMLoadFloat4(&data2.value);
			DirectX::XMStoreFloat4(&pose.rotation, DirectX::XMQuaternionSlerp(R1, R2, rate));
			break;
		}
	}
	if (!is_insert && second >= rotations.back().second)
	{
		pose.rotation = rotations.back().value;
	}
	is_insert = false;
	for (UINT index = 0; index < translations.size() - 1; ++index)
	{
		auto& data1 = translations[index + 0];
		auto& data2 = translations[index + 1];
		if (data1.second <= second && second < data2.second)
		{
			float rate = (second - data1.second) / (data2.second - data1.second);
			pose.translation.x = data1.value.x * (1.0f - rate) + data2.value.x * rate;
			pose.translation.y = data1.value.y * (1.0f - rate) + data2.value.y * rate;
			pose.translation.z = data1.value.z * (1.0f - rate) + data2.value.z * rate;
			break;
		}
	}
	if (!is_insert && second >= translations.back().second)
	{
		pose.translation = translations.back().value;
	}
	return pose;
};

gltf_loader::gltf_loader(std::string_view filepath)
: file_path(filepath)
{
	std::string extension = file_path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), tolower);	// 小文字化

	tinygltf::TinyGLTF gltf;
	gltf.SetImageLoader(load_image_function, this);

	// GLTFModel読み込み
	std::string path_str = system_utility::convert_shift_jis_to_utf8(file_path.string());
	std::string error, warning;
	bool result = false;
	if (extension == ".glb")
	{
		result = gltf.LoadBinaryFromFile(&gltf_model, &error, &warning, path_str);
	}
	else if (extension == ".gltf")
	{
		result = gltf.LoadASCIIFromFile(&gltf_model, &error, &warning, path_str);
	}
	if (!warning.empty())
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::model, warning.c_str());
	}
	if (!result)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, error.c_str());
		_ASSERT_EXPR_A(result, error.c_str());
	}
}

void gltf_loader::load_nodes(std::vector<node_data>& nodes)
{
	nodes.resize(gltf_model.nodes.size());
	for (size_t gltf_node_index = 0; gltf_node_index < nodes.size(); ++gltf_node_index)
	{
		const tinygltf::Node& gltf_node = gltf_model.nodes.at(gltf_node_index);

		node_data& node = nodes.at(gltf_node_index);

		node.name = gltf_node.name;

		//	子供の親を登録
		for (int gltf_child_node_index : gltf_node.children)
		{
			auto& node = nodes.at(gltf_child_node_index);
			node.parent_node_index = static_cast<int>(gltf_node_index);
		}

		if (!gltf_node.matrix.empty())
		{
			DirectX::XMFLOAT4X4 m = convert_gltf_matrix_to_xmfloat4x4(gltf_node.matrix);

			DirectX::XMVECTOR S, R, T;
			DirectX::XMMatrixDecompose(&S, &R, &T, DirectX::XMLoadFloat4x4(&m));

			DirectX::XMStoreFloat3(&node.scale, S);
			DirectX::XMStoreFloat4(&node.rotation, R);
			DirectX::XMStoreFloat3(&node.translation, T);
		}
		else
		{
			if (gltf_node.scale.size() > 0)
			{
				node.scale = convert_gltf_vector3_to_xmfloat3(gltf_node.scale);
			}
			if (gltf_node.rotation.size() > 0)
			{
				node.rotation = convert_gltf_quaternion_to_xmfloat4(gltf_node.rotation);
			}
			if (gltf_node.translation.size() > 0)
			{
				node.translation = convert_gltf_vector3_to_xmfloat3(gltf_node.translation);
			}
		}

		//	軸更新
		convert_node_axis_system(node);
	}
}

void gltf_loader::load_meshes(std::vector<mesh_data>& meshes, const std::vector<node_data>& nodes, const convert_mesh_data& convert_data)
{
	auto make_buffer_data = [](const tinygltf::Accessor& accessor)
	{
		buffer_data data;
		switch (accessor.type)
		{
		case TINYGLTF_TYPE_SCALAR:
			switch (accessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				data.format = DXGI_FORMAT_R8_SINT;
				data.stride_in_bytes = sizeof(CHAR);
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				data.format = DXGI_FORMAT_R8_UINT;
				data.stride_in_bytes = sizeof(UCHAR);
				break;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
				data.format = DXGI_FORMAT_R16_SINT;
				data.stride_in_bytes = sizeof(SHORT);
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				data.format = DXGI_FORMAT_R16_UINT;
				data.stride_in_bytes = sizeof(USHORT);
				break;
			case TINYGLTF_COMPONENT_TYPE_INT:
				data.format = DXGI_FORMAT_R32_SINT;
				data.stride_in_bytes = sizeof(INT);
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				data.format = DXGI_FORMAT_R32_UINT;
				data.stride_in_bytes = sizeof(UINT);
				break;
			default:
				_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
				break;
			}
			break;
		case TINYGLTF_TYPE_VEC2:
			switch (accessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				data.format = DXGI_FORMAT_R8G8_SINT;
				data.stride_in_bytes = sizeof(CHAR) * 2;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				data.format = DXGI_FORMAT_R8G8_UINT;
				data.stride_in_bytes = sizeof(UCHAR) * 2;
				break;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
				data.format = DXGI_FORMAT_R16G16_SINT;
				data.stride_in_bytes = sizeof(SHORT) * 2;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				data.format = DXGI_FORMAT_R16G16_UINT;
				data.stride_in_bytes = sizeof(USHORT) * 2;
				break;
			case TINYGLTF_COMPONENT_TYPE_INT:
				data.format = DXGI_FORMAT_R32G32_SINT;
				data.stride_in_bytes = sizeof(INT) * 2;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				data.format = DXGI_FORMAT_R32G32_UINT;
				data.stride_in_bytes = sizeof(UINT) * 2;
				break;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				data.format = DXGI_FORMAT_R32G32_FLOAT;
				data.stride_in_bytes = sizeof(FLOAT) * 2;
				break;
			default:
				_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
				break;
			}
			break;
		case TINYGLTF_TYPE_VEC3:
			switch (accessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_INT:
				data.format = DXGI_FORMAT_R32G32B32_SINT;
				data.stride_in_bytes = sizeof(INT) * 3;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				data.format = DXGI_FORMAT_R32G32B32_UINT;
				data.stride_in_bytes = sizeof(UINT) * 3;
				break;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				data.format = DXGI_FORMAT_R32G32B32_FLOAT;
				data.stride_in_bytes = sizeof(FLOAT) * 3;
				break;
			default:
				_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
				break;
			}
			switch (accessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				break;
			default:
				_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
				break;
			}
			break;
		case TINYGLTF_TYPE_VEC4:
			switch (accessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				data.format = DXGI_FORMAT_R8G8B8A8_SINT;
				data.stride_in_bytes = sizeof(CHAR) * 4;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				data.format = DXGI_FORMAT_R8G8B8A8_UINT;
				data.stride_in_bytes = sizeof(UCHAR) * 4;
				break;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
				data.format = DXGI_FORMAT_R16G16B16A16_SINT;
				data.stride_in_bytes = sizeof(SHORT) * 4;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				data.format = DXGI_FORMAT_R16G16B16A16_UINT;
				data.stride_in_bytes = sizeof(USHORT) * 4;
				break;
			case TINYGLTF_COMPONENT_TYPE_INT:
				data.format = DXGI_FORMAT_R32G32B32A32_SINT;
				data.stride_in_bytes = sizeof(INT) * 4;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				data.format = DXGI_FORMAT_R32G32B32A32_UINT;
				data.stride_in_bytes = sizeof(UINT) * 4;
				break;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				data.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				data.stride_in_bytes = sizeof(FLOAT) * 4;
				break;
			default:
				_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
				break;
			}
			break;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor type is not supported.");
			break;
		}
		data.size_in_bytes = static_cast<UINT>(accessor.count * data.stride_in_bytes);
		return data;
	};

	auto calculate_transform_matrix = [&](const node_data& node)
	{
		DirectX::XMMATRIX S = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&node.scale));
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&node.translation));
		return S * R * T;
	};

	std::function<void(DirectX::XMMATRIX&, std::size_t)> calculate_global_matrix = [&](DirectX::XMMATRIX& m, std::size_t index)
	{
		if (index == invalid_id)
			return;

		m *= calculate_transform_matrix(nodes[index]);
		calculate_global_matrix(m, nodes[index].parent_node_index);
	};

	for (int gltf_node_index = 0; gltf_node_index < gltf_model.nodes.size(); ++gltf_node_index)
	{
		const tinygltf::Node& gltf_node = gltf_model.nodes.at(gltf_node_index);
		if (gltf_node.mesh < 0)
			continue;

		const tinygltf::Mesh& gltf_mesh = gltf_model.meshes.at(gltf_node.mesh);

		for (const tinygltf::Primitive& gltf_primitive : gltf_mesh.primitives)
		{
			mesh_data& mesh = meshes.emplace_back();
			mesh.node_index = gltf_node_index;
			mesh.material_index = gltf_primitive.material;

			// ボーン
			if (gltf_node.skin >= 0)
			{
				const tinygltf::Skin& gltf_skin = gltf_model.skins.at(gltf_node.skin);
				const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_skin.inverseBindMatrices);
				const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);
				const DirectX::XMFLOAT4X4* offset_matrices = reinterpret_cast<const DirectX::XMFLOAT4X4*>(gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset);
				for (int bone_index = 0; bone_index < gltf_accessor.count; ++bone_index)
				{
					bone_data& bone = mesh.bones.emplace_back();
					bone.offset_matrix = offset_matrices[bone_index];
					bone.node_index = gltf_skin.joints.at(bone_index);
				}
			}

			// インデックスバッファ
			{
				const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_primitive.indices);
				const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);
				const void* indices = gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;

				mesh.index_buffer = make_buffer_data(gltf_accessor);
				if (graphics_utility::get_byte_offset_from_format(mesh.index_buffer.format) == 1)
				{
					mesh.index_buffer.format = DXGI_FORMAT_R32_UINT;
					mesh.index_buffer.stride_in_bytes = sizeof(UINT);
					mesh.index_buffer.size_in_bytes = gltf_accessor.count * mesh.index_buffer.stride_in_bytes;
					mesh.index_buffer.row_data.resize(mesh.index_buffer.size_in_bytes);

					const UCHAR* data = reinterpret_cast<const UCHAR*>(indices);
					UINT* indices_buffer = reinterpret_cast<UINT*>(mesh.index_buffer.row_data.data());
					for (size_t index = 0; index < gltf_accessor.count; ++index)
					{
						indices_buffer[index] = data[index];
					}
				}
				else if (graphics_utility::get_byte_offset_from_format(mesh.index_buffer.format) == 2)
				{
					mesh.index_buffer.format = DXGI_FORMAT_R32_UINT;
					mesh.index_buffer.stride_in_bytes = sizeof(UINT);
					mesh.index_buffer.size_in_bytes = gltf_accessor.count * mesh.index_buffer.stride_in_bytes;
					mesh.index_buffer.row_data.resize(mesh.index_buffer.size_in_bytes);

					const USHORT* data = reinterpret_cast<const USHORT*>(indices);
					UINT* indices_buffer = reinterpret_cast<UINT*>(mesh.index_buffer.row_data.data());
					for (size_t index = 0; index < gltf_accessor.count; ++index)
					{
						indices_buffer[index] = data[index];
					}
				}
				else
				{
					mesh.index_buffer.row_data.resize(mesh.index_buffer.size_in_bytes);
					memcpy_s(mesh.index_buffer.row_data.data(), mesh.index_buffer.row_data.size(),
						indices, mesh.index_buffer.size_in_bytes);
				}
			}

			// 頂点バッファ
			for (std::map<std::string, int>::const_reference gltf_attribute : gltf_primitive.attributes)
			{
				const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_attribute.second);
				const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);

				auto vertex_buffer = make_buffer_data(gltf_accessor);
				auto vertices = gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;

				//	識別子摘出
				auto split_texts = split(gltf_attribute.first, '_');
				std::string semantic_name = gltf_attribute.first;
				int semantic_index = 0;
				if (split_texts.size() == 0)
				{
					semantic_name = gltf_attribute.first;
					semantic_index = 0;
				}
				else if (split_texts.size() == 1)
				{
					semantic_name = split_texts[0];
				}
				else if (split_texts.size() == 2)
				{
					semantic_name = split_texts[0];
					semantic_index = std::atoi(split_texts[1].data());
				}
				else
				{
					assert(!L"not comaptible gltf attribute format");
				}

				//	対応していない識別子があるかチェック
				{
					bool find_semantic_name = false;
					for (auto& gltf_semantic_name : gltf_semantic_names)
					{
						if (semantic_name == std::string(gltf_semantic_name))
						{
							find_semantic_name = true;
							break;
						}
					}
					assert(find_semantic_name);
				}

				//	バッファ格納
				vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);
				UINT count = static_cast<UINT>(vertex_buffer.count());
				if (semantic_name == gltf_semantic_names[static_cast<int>(gltf_semantic_index::TEXCOORD)])
				{
					//	FLOAT型に直しておく
					switch (gltf_accessor.componentType)
					{
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						{
							vertex_buffer.format = DXGI_FORMAT_R32G32_FLOAT;
							vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
							vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
							vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

							const UCHAR* vertex = reinterpret_cast<const UCHAR*>(vertices);
							float* texcoord = reinterpret_cast<float*>(vertex_buffer.row_data.data());
							for (size_t i = 0; i < gltf_accessor.count; ++i)
							{
								texcoord[i * 2 + 0] = vertex[0] / static_cast<float>(UCHAR_MAX);
								texcoord[i * 2 + 1] = vertex[1] / static_cast<float>(UCHAR_MAX);
								vertex += 2;
							}
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						{
							vertex_buffer.format = DXGI_FORMAT_R32G32_FLOAT;
							vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
							vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
							vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

							const USHORT* vertex = reinterpret_cast<const USHORT*>(vertices);
							float* texcoord = reinterpret_cast<float*>(vertex_buffer.row_data.data());
							for (size_t i = 0; i < gltf_accessor.count; ++i)
							{
								texcoord[i * 2 + 0] = vertex[0] / static_cast<float>(USHORT_MAX);
								texcoord[i * 2 + 1] = vertex[1] / static_cast<float>(USHORT_MAX);
								vertex += 2;
							}
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_FLOAT:
						{
							memcpy_s(vertex_buffer.row_data.data(), vertex_buffer.row_data.size(),
								vertices, vertex_buffer.row_data.size());
							break;
						}
						default:
						{
							_ASSERT_EXPR(0, L"");
							continue;
						}
					}
				}
				else if (semantic_name == gltf_semantic_names[static_cast<int>(gltf_semantic_index::COLOR)])
				{
					//	FLOAT型に直しておく
					if (gltf_accessor.type == TINYGLTF_TYPE_VEC3)
					{
						switch (gltf_accessor.componentType)
						{
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
							{
								vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
								vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
								vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
								vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

								const UCHAR* vertex = reinterpret_cast<const UCHAR*>(vertices);
								float* colors = reinterpret_cast<float*>(vertex_buffer.row_data.data());
								for (size_t i = 0; i < gltf_accessor.count; ++i)
								{
									colors[i * 4 + 0] = vertex[0] / static_cast<float>(UCHAR_MAX);
									colors[i * 4 + 1] = vertex[1] / static_cast<float>(UCHAR_MAX);
									colors[i * 4 + 2] = vertex[2] / static_cast<float>(UCHAR_MAX);
									colors[i * 4 + 3] = 1.0f;
									vertex += 3;
								}
								break;
							}
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							{
								vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
								vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
								vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
								vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

								const USHORT* vertex = reinterpret_cast<const USHORT*>(vertices);
								float* colors = reinterpret_cast<float*>(vertex_buffer.row_data.data());
								for (size_t i = 0; i < gltf_accessor.count; ++i)
								{
									colors[i * 4 + 0] = vertex[0] / static_cast<float>(USHORT_MAX);
									colors[i * 4 + 1] = vertex[1] / static_cast<float>(USHORT_MAX);
									colors[i * 4 + 2] = vertex[2] / static_cast<float>(USHORT_MAX);
									colors[i * 4 + 3] = 1.0f;
									vertex += 3;
								}
								break;
							}
							case TINYGLTF_COMPONENT_TYPE_FLOAT:
							{
								vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
								vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
								vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
								vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

								const float* vertex = reinterpret_cast<const float*>(vertices);
								float* colors = reinterpret_cast<float*>(vertex_buffer.row_data.data());
								for (size_t i = 0; i < gltf_accessor.count; ++i)
								{
									colors[i * 4 + 0] = vertex[0];
									colors[i * 4 + 1] = vertex[1];
									colors[i * 4 + 2] = vertex[2];
									colors[i * 4 + 3] = 1.0f;
									vertex += 3;
								}
								break;
							}
							default:
							{
								_ASSERT_EXPR(0, L"");
								continue;
							}
						}
					}
					else if (gltf_accessor.type == TINYGLTF_TYPE_VEC4)
					{
						switch (gltf_accessor.componentType)
						{
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
							{
								vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
								vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
								vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
								vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

								const UCHAR* vertex = reinterpret_cast<const UCHAR*>(vertices);
								float* colors = reinterpret_cast<float*>(vertex_buffer.row_data.data());
								for (size_t i = 0; i < gltf_accessor.count; ++i)
								{
									colors[i * 4 + 0] = vertex[0] / static_cast<float>(UCHAR_MAX);
									colors[i * 4 + 1] = vertex[1] / static_cast<float>(UCHAR_MAX);
									colors[i * 4 + 2] = vertex[2] / static_cast<float>(UCHAR_MAX);
									colors[i * 4 + 3] = vertex[3] / static_cast<float>(UCHAR_MAX);
									vertex += 4;
								}
								break;
							}
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							{
								vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
								vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
								vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
								vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

								const USHORT* vertex = reinterpret_cast<const USHORT*>(vertices);
								float* colors = reinterpret_cast<float*>(vertex_buffer.row_data.data());
								for (size_t i = 0; i < gltf_accessor.count; ++i)
								{
									colors[i * 4 + 0] = vertex[0] / static_cast<float>(USHORT_MAX);
									colors[i * 4 + 1] = vertex[1] / static_cast<float>(USHORT_MAX);
									colors[i * 4 + 2] = vertex[2] / static_cast<float>(USHORT_MAX);
									colors[i * 4 + 3] = vertex[3] / static_cast<float>(USHORT_MAX);
									vertex += 4;
								}
								break;
							}
							case TINYGLTF_COMPONENT_TYPE_FLOAT:
							{
								memcpy_s(vertex_buffer.row_data.data(), vertex_buffer.row_data.size(),
									vertices, vertex_buffer.row_data.size());
								break;
							}
							default:
							{
								_ASSERT_EXPR(0, L"");
								continue;
							}
						}
					}
				}
				else if (semantic_name == gltf_semantic_names[static_cast<int>(gltf_semantic_index::JOINTS)])
				{
					//	UINT型に直しておく
					switch (gltf_accessor.componentType)
					{
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						{
							vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_UINT;
							vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
							vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
							vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

							const UCHAR* vertex = reinterpret_cast<const UCHAR*>(vertices);
							UINT* joints = reinterpret_cast<UINT*>(vertex_buffer.row_data.data());
							for (size_t index = 0; index < gltf_accessor.count; ++index)
							{
								joints[index * 4 + 0] = vertex[index * 4 + 0];
								joints[index * 4 + 1] = vertex[index * 4 + 1];
								joints[index * 4 + 2] = vertex[index * 4 + 2];
								joints[index * 4 + 3] = vertex[index * 4 + 3];
							}
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						{
							vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_UINT;
							vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
							vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
							vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

							const USHORT* vertex = reinterpret_cast<const USHORT*>(vertices);
							UINT* joints = reinterpret_cast<UINT*>(vertex_buffer.row_data.data());
							for (size_t index = 0; index < gltf_accessor.count; ++index)
							{
								joints[index * 4 + 0] = vertex[index * 4 + 0];
								joints[index * 4 + 1] = vertex[index * 4 + 1];
								joints[index * 4 + 2] = vertex[index * 4 + 2];
								joints[index * 4 + 3] = vertex[index * 4 + 3];
							}
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						{
							memcpy_s(vertex_buffer.row_data.data(), vertex_buffer.row_data.size(),
								vertices, vertex_buffer.row_data.size());
							break;
						}
						default:
						{
							_ASSERT_EXPR(0, L"");
							continue;
						}
					}
				}
				else if (semantic_name == gltf_semantic_names[static_cast<int>(gltf_semantic_index::WEIGHTS)])
				{
					//	FLOAT型に直しておく
					switch (gltf_accessor.componentType)
					{
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						{
							vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
							vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
							vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
							vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

							const UCHAR* vertex = reinterpret_cast<const UCHAR*>(vertices);
							float* weight = reinterpret_cast<float*>(vertex_buffer.row_data.data());
							for (size_t index = 0; index < gltf_accessor.count; ++index)
							{
								weight[index * 4 + 0] = vertex[0] / static_cast<float>(UCHAR_MAX);
								weight[index * 4 + 1] = vertex[1] / static_cast<float>(UCHAR_MAX);
								weight[index * 4 + 2] = vertex[2] / static_cast<float>(UCHAR_MAX);
								weight[index * 4 + 3] = vertex[3] / static_cast<float>(UCHAR_MAX);
								vertex += 4;
							}
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						{
							vertex_buffer.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
							vertex_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(vertex_buffer.format);
							vertex_buffer.size_in_bytes = vertex_buffer.stride_in_bytes * count;
							vertex_buffer.row_data.resize(vertex_buffer.size_in_bytes);

							const USHORT* vertex = reinterpret_cast<const USHORT*>(vertices);
							float* weight = reinterpret_cast<float*>(vertex_buffer.row_data.data());
							for (size_t index = 0; index < gltf_accessor.count; ++index)
							{
								weight[index * 4 + 0] = vertex[0] / static_cast<float>(USHORT_MAX);
								weight[index * 4 + 1] = vertex[1] / static_cast<float>(USHORT_MAX);
								weight[index * 4 + 2] = vertex[2] / static_cast<float>(USHORT_MAX);
								weight[index * 4 + 3] = vertex[3] / static_cast<float>(USHORT_MAX);
								vertex += 4;
							}
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_FLOAT:
						{
							memcpy_s(vertex_buffer.row_data.data(), vertex_buffer.row_data.size(),
								vertices, vertex_buffer.row_data.size());
							break;
						}
						default:
						{
							_ASSERT_EXPR(0, L"");
							continue;
						}
					}
				}
				else
				{
					memcpy_s(vertex_buffer.row_data.data(), vertex_buffer.size_in_bytes,
						vertices, vertex_buffer.size_in_bytes);
				}

				mesh.vertex_buffers.insert_or_assign(semantic(semantic_name, semantic_index), vertex_buffer);
			}

			// タンジェントがなかった場合は自力で計算
			if (convert_data.is_compute_tangent &&
				gltf_primitive.attributes.find(std::string(gltf_semantic_names[static_cast<int>(gltf_semantic_index::TANGENT)])) == gltf_primitive.attributes.end() &&
				gltf_primitive.attributes.find(std::string(gltf_semantic_names[static_cast<int>(gltf_semantic_index::POSITION)])) != gltf_primitive.attributes.end())
			{
				//	本来は法線マップとして使用するテクスチャ番号を判定して利用する必要があるが、
				//	今の所そんな機能存在しないっぽいので0番固定で処理する。
				std::string code = std::string(gltf_semantic_names[static_cast<int>(gltf_semantic_index::TEXCOORD)]);
				code += "_0";
				if (gltf_primitive.attributes.find(code) != gltf_primitive.attributes.end())
					compute_tangent(mesh.vertex_buffers, mesh.index_buffer);
			}

			// 座標系変換
			convert_mesh_cordinate_axis_system(mesh, convert_data);

			// ウェイトの正規化処理
			if (convert_data.is_normalize_weight)
			{
				std::vector<std::pair<bool, float>> total_weights;
				for (UINT semantic_index = 0; semantic_index < 8; ++semantic_index)
				{
					auto it = mesh.vertex_buffers.find(semantic(std::string(gltf_semantic_names[static_cast<int>(gltf_semantic_index::WEIGHTS)]), semantic_index));
					if (it == mesh.vertex_buffers.end())
						continue;
					if (total_weights.empty())
						total_weights.resize(it->second.count());

					DirectX::XMFLOAT4* weights = it->second.data<DirectX::XMFLOAT4>();
					for (UINT vertex_index = 0; vertex_index < it->second.count(); ++vertex_index)
					{
						total_weights[vertex_index].second += weights[vertex_index].x;
						total_weights[vertex_index].second += weights[vertex_index].y;
						total_weights[vertex_index].second += weights[vertex_index].z;
						total_weights[vertex_index].second += weights[vertex_index].w;
					}
				}
				//	正規化係数を算出
				for (auto& it : total_weights)
				{
					it.first = it.second <= FLT_EPSILON;	//	合計数が0以下の場合は1.0fを入れるためのフラグ
					if (!it.first)
					{
						it.second = 1.0f / it.second;
					}
				}
				for (UINT semantic_index = 0; semantic_index < 8; ++semantic_index)
				{
					auto it = mesh.vertex_buffers.find(semantic(std::string(gltf_semantic_names[static_cast<int>(gltf_semantic_index::WEIGHTS)]), semantic_index));
					if (it == mesh.vertex_buffers.end())
						continue;

					DirectX::XMFLOAT4* weights = it->second.data<DirectX::XMFLOAT4>();
					for (UINT vertex_index = 0; vertex_index < it->second.count(); ++vertex_index)
					{
						if (total_weights[vertex_index].first)
						{
							weights[vertex_index].x = semantic_index == 0;
							weights[vertex_index].y = 0;
							weights[vertex_index].z = 0;
							weights[vertex_index].w = 0;
						}
						else
						{
							float normalized_value = total_weights[vertex_index].second;
							weights[vertex_index].x *= normalized_value;
							weights[vertex_index].y *= normalized_value;
							weights[vertex_index].z *= normalized_value;
							weights[vertex_index].w *= normalized_value;
						}
					}
				}
				compute_tangent(mesh.vertex_buffers, mesh.index_buffer);
			}

			// バウンディングボックス計算
			{
				mesh.bounds_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
				mesh.bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

				auto positions = mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::POSITION)], 0});
				if (positions != mesh.vertex_buffers.end())
				{
					DirectX::XMFLOAT3* ptr = reinterpret_cast<DirectX::XMFLOAT3*>(positions->second.row_data.data());

					std::vector<DirectX::XMUINT4*> joints;
					std::vector<DirectX::XMFLOAT4*> weights;
					while (mesh.vertex_buffers.end() != mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::JOINTS)], static_cast<UINT>(joints.size()) }))
					{
						auto joint_data = mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::JOINTS)], static_cast<UINT>(joints.size())});
						auto weight_data = mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::WEIGHTS)], static_cast<UINT>(joints.size())});
						joints.emplace_back(reinterpret_cast<DirectX::XMUINT4*>(joint_data->second.row_data.data()));
						weights.emplace_back(reinterpret_cast<DirectX::XMFLOAT4*>(weight_data->second.row_data.data()));
					}

					std::vector<DirectX::XMMATRIX> transforms(mesh.bones.size());
					for (UINT index = 0; index < mesh.bones.size(); ++index)
					{
						auto& node = mesh.bones[index];
						DirectX::XMMATRIX wm = calculate_transform_matrix(nodes[node.node_index]);
						calculate_global_matrix(wm, nodes[node.node_index].parent_node_index);
						transforms[index] = DirectX::XMLoadFloat4x4(&node.offset_matrix) * wm;
					}

					for (UINT index = 0; index < positions->second.count(); ++index)
					{
						DirectX::XMFLOAT3 pos = ptr[index];
						if (!joints.empty())
						{
							DirectX::XMMATRIX m = DirectX::XMMatrixSet(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
							for (UINT joint_index = 0; joint_index < joints.size(); ++joint_index)
							{
								m += transforms[joints[joint_index][index].x] * weights[joint_index][index].x;
								m += transforms[joints[joint_index][index].y] * weights[joint_index][index].y;
								m += transforms[joints[joint_index][index].z] * weights[joint_index][index].z;
								m += transforms[joints[joint_index][index].w] * weights[joint_index][index].w;
							}
							DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&pos);
							DirectX::XMStoreFloat3(&pos, DirectX::XMVector3TransformCoord(p, m));
						}
						mesh.bounds_min.x = std::min(pos.x, mesh.bounds_min.x);
						mesh.bounds_min.y = std::min(pos.y, mesh.bounds_min.y);
						mesh.bounds_min.z = std::min(pos.z, mesh.bounds_min.z);
						mesh.bounds_max.x = std::max(pos.x, mesh.bounds_max.x);
						mesh.bounds_max.y = std::max(pos.y, mesh.bounds_max.y);
						mesh.bounds_max.z = std::max(pos.z, mesh.bounds_max.z);
					}
				}
			}
		}
	}
}

void gltf_loader::load_materials(std::vector<material_data>& materials)
{
	std::filesystem::path directry_path(file_path.parent_path());

	auto load_texture = [&](int gltf_texture_index, std::string_view texture_type, std::string_view material_name)
	{
		if (gltf_texture_index < 0)
			return std::string("");

		const tinygltf::Texture& gltf_texture = gltf_model.textures.at(gltf_texture_index);
		const tinygltf::Image& gltf_image = gltf_model.images.at(gltf_texture.source);

		//	元テクスチャ生成
		std::filesystem::path texture_filepath(gltf_image.uri);
		if (gltf_image.bufferView >= 0 || !gltf_image.image.empty())
		{
			std::vector<uint8_t> image_buffer;
			if (gltf_image.bufferView >= 0)
			{
				const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_image.bufferView);
				const tinygltf::Buffer& gltf_buffer = gltf_model.buffers.at(gltf_buffer_view.buffer);
				const byte* data = gltf_buffer.data.data() + gltf_buffer_view.byteOffset;
				image_buffer.resize(gltf_buffer_view.byteLength);
				::memcpy_s(image_buffer.data(), image_buffer.size(), data, gltf_buffer_view.byteLength);
			}
			else
			{
				image_buffer.resize(gltf_image.image.size());
				::memcpy_s(image_buffer.data(), image_buffer.size(), gltf_image.image.data(), gltf_image.image.size());
			}

			// ファイルパス作成
			if (texture_filepath == "")
			{
				std::string extension(std::filesystem::path(gltf_image.mimeType).filename().string());
				std::string path;
				path += material_name;
				path += "_";
				path += texture_type;
				path += ".";
				path += extension;
				texture_filepath = path;
			}
			texture_filepath = "textures" / texture_filepath.filename();

			//	ディレクトリチェック
			std::filesystem::path output_directry_path(directry_path / texture_filepath.parent_path());
			if (!std::filesystem::exists(output_directry_path))
			{
				std::filesystem::create_directories(output_directry_path);
			}

			// 出力ディレクトリに画像ファイルを保存
			std::filesystem::path output_filepath(directry_path / texture_filepath);
			if (!std::filesystem::exists(output_filepath))
			{
				if (gltf_image.bufferView >= 0)
				{
					//	こちらは直接保存
					const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_image.bufferView);
					const tinygltf::Buffer& gltf_buffer = gltf_model.buffers.at(gltf_buffer_view.buffer);
					const byte* data = gltf_buffer.data.data() + gltf_buffer_view.byteOffset;

					std::ofstream os(output_filepath.string().c_str(), std::ios::binary);
					os.write(reinterpret_cast<const char*>(data), gltf_buffer_view.byteLength);
				}
				else
				{
					//	テクスチャ生成
					std::string path = texture_filepath.string();
					texture_manager::save_texture_desc desc;
					desc.file_path = path;
					desc.format = convert_from_tiny_image_to_direct_x_image(gltf_image);
					desc.data_format = texture_manager::save_texture_desc::data_formats::dds;
					auto& image = desc.images.emplace_back();
					image.width = gltf_image.width;
					image.height = gltf_image.height;
					image.pixels = gltf_image.image.data();

					texture_manager::instance()->save_texture(desc);
				}
			}
		}
		return texture_filepath.string();
	};

	for (const tinygltf::Material& gltf_material : gltf_model.materials)
	{
		material_data& material = materials.emplace_back();
		if (gltf_material.name.empty())
		{
			material.name = std::string("material_") + std::to_string(materials.size() - 1);
		}
		else
		{
			material.name = system_utility::convert_utf8_to_shift_jis(gltf_material.name);
		}
		material.base_color.x = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(0));
		material.base_color.y = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(1));
		material.base_color.z = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(2));
		material.base_color.w = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(3));
		material.emissive_color.x = static_cast<float>(gltf_material.emissiveFactor.at(0));
		material.emissive_color.y = static_cast<float>(gltf_material.emissiveFactor.at(1));
		material.emissive_color.z = static_cast<float>(gltf_material.emissiveFactor.at(2));
		material.occlusion_strength = static_cast<float>(gltf_material.occlusionTexture.strength);
		material.roughness = static_cast<float>(gltf_material.pbrMetallicRoughness.roughnessFactor);
		material.metalness = static_cast<float>(gltf_material.pbrMetallicRoughness.metallicFactor);
		material.alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
		material.normal_scale = static_cast<float>(gltf_material.normalTexture.scale);
		material.double_sided = gltf_material.doubleSided;
		if (gltf_material.alphaMode == "BLEND")
		{
			material.alpha_mode = alpha_modes::translucent;
		}
		else if (gltf_material.alphaMode == "MASK")
		{
			material.alpha_mode = alpha_modes::mask;
		}
		else
		{
			material.alpha_mode = alpha_modes::opaque;
		}
		material.base_texture_path = load_texture(gltf_material.pbrMetallicRoughness.baseColorTexture.index, "base_color", material.name.c_str());
		material.emissive_texture_path = load_texture(gltf_material.emissiveTexture.index, "emissive", material.name.c_str());
		material.normal_texture_path = load_texture(gltf_material.normalTexture.index, "normal", material.name.c_str());
		material.occlusion_texture_path = load_texture(gltf_material.occlusionTexture.index, "occlusion", material.name.c_str());
		material.roughness_metalness_texture_path = load_texture(gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index, "roughness_metallic", material.name.c_str());
	}
}

void gltf_loader::populate_vertex_data(std::vector<mesh_data>& meshes, const semantic& semantic, const register_vertex_data& vertex_data)
{
	for (UINT mesh_index = 0; mesh_index < meshes.size(); ++mesh_index)
	{
		auto& mesh = meshes[mesh_index];
		if (mesh.vertex_buffers.find(semantic) != mesh.vertex_buffers.end())
			continue;

		std::size_t vertex_count = 0;
		auto it_begin = mesh.vertex_buffers.begin();
		if (it_begin == mesh.vertex_buffers.end())
			return;

		vertex_count = it_begin->second.count();

		buffer_data& buffer = mesh.vertex_buffers[semantic];
		buffer.format = vertex_data.format;
		buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(buffer.format);
		buffer.size_in_bytes = buffer.stride_in_bytes * vertex_count;
		buffer.row_data.resize(buffer.size_in_bytes);
		for (std::size_t offset = 0; offset < buffer.size_in_bytes; offset += buffer.stride_in_bytes)
		{
			memcpy_s(buffer.row_data.data() + offset,
				buffer.row_data.size() - offset,
				vertex_data.set_data.data(),
				vertex_data.set_data.size());
		}
	}
}

void gltf_loader::combine_unskinned_meshes(std::vector<gltf_loader::mesh_data>& meshes, std::vector<gltf_loader::node_data>& nodes)
{
	std::vector<gltf_loader::mesh_data> out_meshes, bones_mesh;

	//	同じマテリアルID・頂点バッファの種類のリストを作成
	std::unordered_map<std::size_t, std::vector<std::size_t>> same_mesh_index_map;
	std::vector<std::size_t> single_mesh_index_map;

	//	同じマテリアルID・頂点バッファの種類を統合する
	for (UINT mesh_index = 0; mesh_index < meshes.size(); ++mesh_index)
	{
		auto& mesh = meshes[mesh_index];

		//	ボーンがある場合は結合しない
		//	結合対象が無ければしない
		if (!mesh.bones.empty() || mesh.index_buffer.count() == 0)
		{
			single_mesh_index_map.push_back(mesh_index);
			continue;
		}

		//	同じマテリアルIDでかつ頂点バッファであるかチェック
		bool register_same_mesh = false;
		auto it_same_mesh_list = same_mesh_index_map.find(mesh_index);
		for (auto& same_mesh_data : same_mesh_index_map)
		{
			auto& same_mesh = meshes[same_mesh_data.first];

			//	マテリアルIDが違えば除外
			if (mesh.material_index != same_mesh.material_index)
				continue;

			//	結合対象が無ければしない
			//	頂点バッファ数が同じであれば対応
			if (same_mesh.index_buffer.count() == 0 ||
				same_mesh.vertex_buffers.size() != mesh.vertex_buffers.size())
				continue;

			//	頂点情報が同じか判定
			bool is_same = true;
			for (auto& vertex_buffer : mesh.vertex_buffers)
			{
				//	存在しないバッファがあれば結合対象とはみなさない
				auto it_same_mesh_vertex_buffer = same_mesh.vertex_buffers.find(vertex_buffer.first);
				if (it_same_mesh_vertex_buffer == same_mesh.vertex_buffers.end())
				{
					is_same = false;
					break;
				}
				if (vertex_buffer.second.format != it_same_mesh_vertex_buffer->second.format)
				{
					is_same = false;
					break;
				}
			}
			if (!is_same)
				continue;

			same_mesh_data.second.push_back(mesh_index);
			register_same_mesh = true;
			break;
		}

		if (register_same_mesh)
			continue;

		//	新規メッシュとして登録
		std::vector<std::size_t> index_buffer;
		index_buffer.emplace_back(mesh_index);
		same_mesh_index_map.insert_or_assign(mesh_index, index_buffer);
	}

	//	結合メッシュを登録していく
	UINT combine_mesh_counter = 0;
	for (auto it_same_mesh : same_mesh_index_map)
	{
		if (it_same_mesh.second.size() == 1)
		{
			//	結合なしなのでそのまま登録
			out_meshes.push_back(meshes[it_same_mesh.first]);
			continue;
		}

		//	合成用ノード生成
		{
			nodes.emplace_back();
			auto& insert_node = nodes.back();
			insert_node.name = "combined_mesh_";
			insert_node.name += std::to_string(combine_mesh_counter++);
			insert_node.parent_node_index = invalid_id;
		}
		//	基準メッシュ生成
		out_meshes.emplace_back();
		auto& basis_mesh = out_meshes.back();
		{
			basis_mesh.node_index = nodes.size() - 1;
			basis_mesh.material_index = meshes[it_same_mesh.first].material_index;
			basis_mesh.bones.reserve(it_same_mesh.second.size());
			for (auto& mesh_index : it_same_mesh.second)
			{
				basis_mesh.bones.emplace_back();
				auto& bone = basis_mesh.bones.back();
				bone.node_index = meshes[mesh_index].node_index;
			}
			//	最初からUINTで登録しておく
			basis_mesh.index_buffer.format = DXGI_FORMAT_R32_UINT;
			basis_mesh.index_buffer.offset_in_bytes = 0;
			basis_mesh.index_buffer.size_in_bytes = 0;
			basis_mesh.index_buffer.stride_in_bytes = graphics_utility::get_byte_offset_from_format(basis_mesh.index_buffer.format);
			//	JOINTSとWEIGHTSを含めて頂点バッファ登録
			{
				gltf_loader::buffer_data buffer_data;
				buffer_data.format = DXGI_FORMAT_R32G32B32A32_UINT;
				buffer_data.offset_in_bytes = 0;
				buffer_data.stride_in_bytes = sizeof(DirectX::XMUINT4);
				buffer_data.size_in_bytes = 0;
				basis_mesh.vertex_buffers.insert(std::make_pair(semantic(gltf_semantic_names[static_cast<int>(gltf_semantic_index::JOINTS)], 0), buffer_data));
			}
			{
				gltf_loader::buffer_data buffer_data;
				buffer_data.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				buffer_data.offset_in_bytes = 0;
				buffer_data.stride_in_bytes = sizeof(DirectX::XMFLOAT4);
				buffer_data.size_in_bytes = 0;
				basis_mesh.vertex_buffers.insert(std::make_pair(semantic(gltf_semantic_names[static_cast<int>(gltf_semantic_index::WEIGHTS)], 0), buffer_data));
			}
			for (auto& it_mesh_vertex_buffer : meshes[it_same_mesh.first].vertex_buffers)
			{
				gltf_loader::buffer_data buffer_data;
				buffer_data.format = it_mesh_vertex_buffer.second.format;
				buffer_data.offset_in_bytes = 0;
				buffer_data.stride_in_bytes = it_mesh_vertex_buffer.second.stride_in_bytes;
				buffer_data.size_in_bytes = 0;
				basis_mesh.vertex_buffers.insert(std::make_pair(it_mesh_vertex_buffer.first, buffer_data));
			}

		}
		//	基準メッシュに対して各種情報を登録
		UINT basis_vertex_count = 0;
		for (UINT bone_index = 0; bone_index < it_same_mesh.second.size(); ++bone_index)
		{
			auto& same_mesh = meshes[it_same_mesh.second[bone_index]];

			//	インデックスバッファ登録
			{
				std::size_t original_size = basis_mesh.index_buffer.size_in_bytes;

				//	データ登録
				UINT mesh_index_count = static_cast<UINT>(same_mesh.index_buffer.count());

				basis_mesh.index_buffer.size_in_bytes += mesh_index_count * graphics_utility::get_byte_offset_from_format(basis_mesh.index_buffer.format);
				basis_mesh.index_buffer.row_data.resize(basis_mesh.index_buffer.size_in_bytes);
				UINT* insert_indices = reinterpret_cast<UINT*>(basis_mesh.index_buffer.row_data.data() + original_size);
				switch (same_mesh.index_buffer.stride_in_bytes)
				{
					case 2:
					{
						USHORT* mesh_indices = reinterpret_cast<USHORT*>(same_mesh.index_buffer.row_data.data());
						for (UINT index = 0; index < mesh_index_count; ++index)
						{
							insert_indices[index] = basis_vertex_count + mesh_indices[index];
						}
						break;
					}
					case 4:
					{
						UINT* mesh_indices = reinterpret_cast<UINT*>(same_mesh.index_buffer.row_data.data());
						for (UINT index = 0; index < mesh_index_count; ++index)
						{
							insert_indices[index] = basis_vertex_count + mesh_indices[index];
						}
						break;
					}
				}
			}

			//	JOINTS/WEIGHTSを登録
			{
				std::size_t same_mesh_vertex_count = same_mesh.vertex_buffers.begin()->second.count();

				//	ボーン番号登録
				auto it_joints = basis_mesh.vertex_buffers.find(semantic(gltf_semantic_names[static_cast<int>(gltf_semantic_index::JOINTS)], 0));
				it_joints->second.size_in_bytes += same_mesh_vertex_count * it_joints->second.stride_in_bytes;
				it_joints->second.row_data.resize(it_joints->second.size_in_bytes);
				DirectX::XMUINT4* value = reinterpret_cast<DirectX::XMUINT4*>(it_joints->second.row_data.data());
				for (std::size_t index = basis_vertex_count; index < basis_vertex_count + same_mesh_vertex_count; ++index)
				{
					value[index].x = static_cast<UINT>(bone_index);
					value[index].y = 0;
					value[index].z = 0;
					value[index].w = 0;
				}

				//	影響度登録
				auto it_weights = basis_mesh.vertex_buffers.find(semantic(gltf_semantic_names[static_cast<int>(gltf_semantic_index::WEIGHTS)], 0));
				it_weights->second.size_in_bytes += same_mesh_vertex_count * it_weights->second.stride_in_bytes;
				it_weights->second.row_data.resize(it_weights->second.size_in_bytes);
				DirectX::XMFLOAT4* value2 = reinterpret_cast<DirectX::XMFLOAT4*>(it_weights->second.row_data.data());
				for (std::size_t index = basis_vertex_count; index < basis_vertex_count + same_mesh_vertex_count; ++index)
				{
					value2[index].x = 1.0f;
					value2[index].y = 0.0f;
					value2[index].z = 0.0f;
					value2[index].w = 0.0f;
				}
			}

			//	全頂点バッファを登録
			for (auto& it_mesh_vertex_buffer : same_mesh.vertex_buffers)
			{
				auto it_basis_mesh_vertex_buffer = basis_mesh.vertex_buffers.find(it_mesh_vertex_buffer.first);

				//	サイズ調整
				std::size_t original_size = it_basis_mesh_vertex_buffer->second.size_in_bytes;
				it_basis_mesh_vertex_buffer->second.size_in_bytes += it_mesh_vertex_buffer.second.size_in_bytes;
				it_basis_mesh_vertex_buffer->second.row_data.resize(it_basis_mesh_vertex_buffer->second.size_in_bytes);
				memcpy_s(it_basis_mesh_vertex_buffer->second.row_data.data() + original_size,
						it_mesh_vertex_buffer.second.size_in_bytes,
						it_mesh_vertex_buffer.second.row_data.data(),
						it_mesh_vertex_buffer.second.size_in_bytes);
			}

			//	頂点数加算
			basis_vertex_count += static_cast<UINT>(same_mesh.vertex_buffers.begin()->second.count());
		}
	}

	//	非結合メッシュを登録していく
	for (auto it_mesh : single_mesh_index_map)
	{
		bones_mesh.push_back(meshes[it_mesh]);
	}

	//	AABB再計算
	{
		auto calculate_transform_matrix = [&](const node_data& node)
		{
			DirectX::XMMATRIX S = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&node.scale));
			DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
			DirectX::XMMATRIX T = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&node.translation));
			return S * R * T;
		};

		std::function<void(DirectX::XMMATRIX&, std::size_t)> calculate_global_matrix = [&](DirectX::XMMATRIX& m, std::size_t index)
		{
			if (index == invalid_id)
				return;

			m *= calculate_transform_matrix(nodes[index]);
			calculate_global_matrix(m, nodes[index].parent_node_index);
		};

		for (mesh_data& mesh : out_meshes)
		{
			mesh.bounds_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
			mesh.bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

			auto positions = mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::POSITION)], 0});
			if (positions != mesh.vertex_buffers.end())
			{
				DirectX::XMFLOAT3* ptr = reinterpret_cast<DirectX::XMFLOAT3*>(positions->second.row_data.data());

				std::vector<DirectX::XMUINT4*> joints;
				std::vector<DirectX::XMFLOAT4*> weights;
				while (mesh.vertex_buffers.end() != mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::JOINTS)], static_cast<UINT>(joints.size()) }))
				{
					auto joint_data = mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::JOINTS)], static_cast<UINT>(joints.size())});
					auto weight_data = mesh.vertex_buffers.find({ gltf_semantic_names[static_cast<int>(gltf_semantic_index::WEIGHTS)], static_cast<UINT>(joints.size())});
					joints.emplace_back(reinterpret_cast<DirectX::XMUINT4*>(joint_data->second.row_data.data()));
					weights.emplace_back(reinterpret_cast<DirectX::XMFLOAT4*>(weight_data->second.row_data.data()));
				}

				std::vector<DirectX::XMMATRIX> transforms(mesh.bones.size());
				for (UINT index = 0; index < mesh.bones.size(); ++index)
				{
					auto& node = mesh.bones[index];
					DirectX::XMMATRIX wm = calculate_transform_matrix(nodes[node.node_index]);
					calculate_global_matrix(wm, nodes[node.node_index].parent_node_index);
					transforms[index] = DirectX::XMLoadFloat4x4(&node.offset_matrix) * wm;
				}

				for (UINT index = 0; index < positions->second.count(); ++index)
				{
					DirectX::XMFLOAT3 pos = ptr[index];
					if (!joints.empty())
					{
						DirectX::XMMATRIX m = DirectX::XMMatrixSet(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
						for (UINT joint_index = 0; joint_index < joints.size(); ++joint_index)
						{
							m += transforms[joints[joint_index][index].x] * weights[joint_index][index].x;
							m += transforms[joints[joint_index][index].y] * weights[joint_index][index].y;
							m += transforms[joints[joint_index][index].z] * weights[joint_index][index].z;
							m += transforms[joints[joint_index][index].w] * weights[joint_index][index].w;
						}
						DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&pos);
						DirectX::XMStoreFloat3(&pos, DirectX::XMVector3TransformCoord(p, m));
					}
					mesh.bounds_min.x = std::min(pos.x, mesh.bounds_min.x);
					mesh.bounds_min.y = std::min(pos.y, mesh.bounds_min.y);
					mesh.bounds_min.z = std::min(pos.z, mesh.bounds_min.z);
					mesh.bounds_max.x = std::max(pos.x, mesh.bounds_max.x);
					mesh.bounds_max.y = std::max(pos.y, mesh.bounds_max.y);
					mesh.bounds_max.z = std::max(pos.z, mesh.bounds_max.z);
				}
			}
		}
	}

	//	更新したメッシュを登録
	meshes = out_meshes;
	for (auto& mesh : bones_mesh)
	{
		meshes.emplace_back(mesh);
	}
}

void gltf_loader::load_animations(std::vector<animation_data>& animations, const std::vector<node_data>& nodes)
{
	DirectX::XMVECTOR epsilon = DirectX::XMVectorReplicate(0.00001f);

	for (const tinygltf::Animation& gltf_animation : gltf_model.animations)
	{
		animation_data& animation = animations.emplace_back();
		animation.name = system_utility::convert_utf8_to_shift_jis(gltf_animation.name);
		animation.nodes.resize(nodes.size());
		animation.end_time = 0;

		float min_time = FLT_MAX;
		float max_time = 0;
		for (const tinygltf::AnimationChannel& gltf_animation_channel : gltf_animation.channels)
		{
			node_animation_data& node_animation = animation.nodes.at(gltf_animation_channel.target_node);
			if (!nodes.empty())
			{
				node_animation.node_name = nodes[gltf_animation_channel.target_node].name;
			}
			const tinygltf::AnimationSampler& gltf_animation_sampler = gltf_animation.samplers.at(gltf_animation_channel.sampler);
			const tinygltf::Accessor& gltf_input_accessor = gltf_model.accessors.at(gltf_animation_sampler.input);
			const tinygltf::Accessor& gltf_output_accessor = gltf_model.accessors.at(gltf_animation_sampler.output);
			const tinygltf::BufferView& gltf_input_buffer_view{ gltf_model.bufferViews.at(gltf_input_accessor.bufferView) };
			const tinygltf::BufferView& gltf_output_buffer_view = gltf_model.bufferViews.at(gltf_output_accessor.bufferView);

			const float* gltf_key_frame_times = reinterpret_cast<const float*>(gltf_model.buffers.at(gltf_input_buffer_view.buffer).data.data() + gltf_input_buffer_view.byteOffset + gltf_input_accessor.byteOffset);
			min_time = std::min(min_time, gltf_key_frame_times[0]);
			max_time = std::max(animation.end_time, gltf_key_frame_times[gltf_input_accessor.count - 1]);

			if (gltf_animation_channel.target_path == "scale")
			{
				const DirectX::XMFLOAT3* gltf_key_frame_values = reinterpret_cast<const DirectX::XMFLOAT3*>(gltf_model.buffers.at(gltf_output_buffer_view.buffer).data.data() + gltf_output_buffer_view.byteOffset + gltf_output_accessor.byteOffset);
				for (int index = 0; index < gltf_input_accessor.count; ++index)
				{
					vector_key_frame& key_frame = node_animation.scales.emplace_back();
					key_frame.second = gltf_key_frame_times[index];
					key_frame.value = gltf_key_frame_values[index];
				}
			}
			else if (gltf_animation_channel.target_path == "rotation")
			{
				// キーフレームデータ取得
				const DirectX::XMFLOAT4* gltf_key_frame_values = reinterpret_cast<const DirectX::XMFLOAT4*>(gltf_model.buffers.at(gltf_output_buffer_view.buffer).data.data() + gltf_output_buffer_view.byteOffset + gltf_output_accessor.byteOffset);
				for (int index = 0; index < gltf_input_accessor.count; ++index)
				{
					quaternion_key_frame& key_frame = node_animation.rotations.emplace_back();
					key_frame.second = gltf_key_frame_times[index];
					key_frame.value = gltf_key_frame_values[index];
				}
			}
			else if (gltf_animation_channel.target_path == "translation")
			{
				// キーフレームデータ取得
				const DirectX::XMFLOAT3* gltf_key_frame_values = reinterpret_cast<const DirectX::XMFLOAT3*>(gltf_model.buffers.at(gltf_output_buffer_view.buffer).data.data() + gltf_output_buffer_view.byteOffset + gltf_output_accessor.byteOffset);
				for (int index = 0; index < gltf_input_accessor.count; ++index)
				{
					vector_key_frame& key_frame = node_animation.translations.emplace_back();
					key_frame.second = gltf_key_frame_times[index];
					key_frame.value = gltf_key_frame_values[index];
				}
			}
		}

		// 先頭キーフレームの時間が0じゃない場合があるので調整する
		for (node_animation_data& node_animation : animation.nodes)
		{
			for (vector_key_frame& key_frame : node_animation.translations)
				key_frame.second -= min_time;
			for (quaternion_key_frame& key_frame : node_animation.rotations)
				key_frame.second -= min_time;
			for (vector_key_frame& key_frame : node_animation.scales)
				key_frame.second -= min_time;
		}

		// アニメーション再生時間計算
		animation.end_time = max_time - min_time;

		// 座標系変換
		convert_animation_axis_system(animation);

		// アニメーションがなかったノードに対して初期姿勢のキーフレームを追加する
		for (size_t node_index = 0; node_index < animation.nodes.size(); ++node_index)
		{
			const node_data& node = nodes.at(node_index);
			node_animation_data& node_animation = animation.nodes.at(node_index);
			// 移動
			if (node_animation.translations.size() == 0)
			{
				vector_key_frame& key_frame = node_animation.translations.emplace_back();
				key_frame.second = 0.0f;
				key_frame.value = node.translation;
			}
			if (node_animation.translations.size() == 1)
			{
				vector_key_frame& key_frame = node_animation.translations.emplace_back();
				key_frame.second = animation.end_time;
				key_frame.value = node_animation.translations.at(0).value;
			}
			// 回転
			if (node_animation.rotations.size() == 0)
			{
				quaternion_key_frame& key_frame = node_animation.rotations.emplace_back();
				key_frame.second = 0.0f;
				key_frame.value = node.rotation;
			}
			if (node_animation.rotations.size() == 1)
			{
				quaternion_key_frame& key_frame = node_animation.rotations.emplace_back();
				key_frame.second = animation.end_time;
				key_frame.value = node_animation.rotations.at(0).value;
			}
			// スケール
			if (node_animation.scales.size() == 0)
			{
				vector_key_frame& key_frame = node_animation.scales.emplace_back();
				key_frame.second = 0.0f;
				key_frame.value = node.scale;
			}
			if (node_animation.scales.size() == 1)
			{
				vector_key_frame& key_frame = node_animation.scales.emplace_back();
				key_frame.second = animation.end_time;
				key_frame.value = node_animation.scales.at(0).value;
			}
		}
	}
}

void gltf_loader::load_node_bones(std::vector<bone_data>& bones, const convert_mesh_data& convert_data)
{
	bones.resize(gltf_model.nodes.size());
	for (auto& bone : bones)
	{
		bone.node_index = invalid_id;
	}

	for (size_t gltf_node_index = 0; gltf_node_index < bones.size(); ++gltf_node_index)
	{
		const tinygltf::Node& gltf_node = gltf_model.nodes.at(gltf_node_index);
		if (gltf_node.mesh < 0)
			continue;

		const tinygltf::Mesh& gltf_mesh = gltf_model.meshes.at(gltf_node.mesh);

		for (const tinygltf::Primitive& gltf_primitive : gltf_mesh.primitives)
		{
			// ボーン
			if (gltf_node.skin >= 0)
			{
				const tinygltf::Skin& gltf_skin = gltf_model.skins.at(gltf_node.skin);
				const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_skin.inverseBindMatrices);
				const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);
				const DirectX::XMFLOAT4X4* offset_matrices = reinterpret_cast<const DirectX::XMFLOAT4X4*>(gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset);
				for (int bone_index = 0; bone_index < gltf_accessor.count; ++bone_index)
				{
					auto node_index = gltf_skin.joints.at(bone_index);

					bone_data& bone = bones[node_index];
					bone.node_index = node_index;
					bone.offset_matrix = offset_matrices[bone_index];
				}
			}
		}
	}

	//	変換
	if (convert_data.is_change_coordinate_system)
	{
		for (auto& bone : bones)
		{
			if (bone.node_index == invalid_id)
				continue;
			convert_matrix_cordinate_axis_system(bone.offset_matrix);
		}
	}
}
