#include "data_converter.h"

#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "animation_data_version.h"

#if defined(USE_IMGUI)

namespace
{
	//	頂点フォーマット
	struct format_data
	{
		semantic s;
		DXGI_FORMAT f;
	};
	enum format_id
	{
		position,
		normal,
		tangent,
		color_0,
		color_1,
		color_2,
		color_3,
		color_4,
		color_5,
		color_6,
		color_7,
		texcoord_0,
		texcoord_1,
		texcoord_2,
		texcoord_3,
		texcoord_4,
		texcoord_5,
		texcoord_6,
		texcoord_7,
		joint_0,
		joint_1,
		weight_0,
		weight_1,
	};
	const format_data format_datas[] =
	{
		{{"POSITION", 0}, DXGI_FORMAT_R32G32B32_FLOAT},
		{{"NORMAL", 0}, DXGI_FORMAT_R32G32B32_FLOAT},
		{{"TANGENT", 0}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 0}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 1}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 2}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 3}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 4}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 5}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 6}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"COLOR", 7}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"TEXCOORD", 0}, DXGI_FORMAT_R32G32_FLOAT},
		{{"TEXCOORD", 1}, DXGI_FORMAT_R32G32_FLOAT},
		{{"TEXCOORD", 2}, DXGI_FORMAT_R32G32_FLOAT},
		{{"TEXCOORD", 3}, DXGI_FORMAT_R32G32_FLOAT},
		{{"TEXCOORD", 4}, DXGI_FORMAT_R32G32_FLOAT},
		{{"TEXCOORD", 5}, DXGI_FORMAT_R32G32_FLOAT},
		{{"TEXCOORD", 6}, DXGI_FORMAT_R32G32_FLOAT},
		{{"TEXCOORD", 7}, DXGI_FORMAT_R32G32_FLOAT},
		{{"JOINTS", 0}, DXGI_FORMAT_R32G32B32A32_UINT},
		{{"JOINTS", 1}, DXGI_FORMAT_R32G32B32A32_UINT},
		{{"WEIGHTS", 0}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"WEIGHTS", 1}, DXGI_FORMAT_R32G32B32A32_FLOAT},
	};

	//	MMDは1.0 == 8cm(目測)、EMOは 1.0 == 10cm なのでスケールしておく
	constexpr float ScaleFactorForMMDtoEMO = 10.0f / 8.0f;

	//	回転補正
	constexpr DirectX::XMFLOAT3 RotationFactorForMMDtoEMO = {0.0f, DirectX::XM_PI, 0.0f};

	//	正規化フレームVMD
	constexpr float NormalizedFrameForVMD = 30.0f;

	//	アニメーションサンプリングレート
	constexpr float AnimationSamplingRate = system_units::convert_fps_to_sec(1.0f);

	//	PMX変換
	bool convert(model_resource& resource, pmx_model& loader, const data_converter::mmd::convert_model_data& convert_data)
	{
		HRESULT	hr;
		std::filesystem::path path(convert_data.file_path);

		auto& nodes = resource.get_nodes_for_editor();
		auto& meshes = resource.get_meshes_for_editor();
		auto& materials = resource.get_materials_for_editor();
		auto& animations = resource.get_animations_for_editor();

		//	ノード構築
		{
			//	ルートノード追加
			{
				nodes.emplace_back();
				auto& node = nodes.back();
				node.name = std::string("root node");
				node.parent_index = model_resource::invalid_id;
				//	MMDは1.0 == 80cm(目測)なので、スケールしておく
				node.scale.x = ScaleFactorForMMDtoEMO;
				node.scale.y = ScaleFactorForMMDtoEMO;
				node.scale.z = ScaleFactorForMMDtoEMO;
				DirectX::XMStoreFloat4(&node.rotation, DirectX::XMQuaternionRotationRollPitchYaw(RotationFactorForMMDtoEMO.x, RotationFactorForMMDtoEMO.y, RotationFactorForMMDtoEMO.z));
			}
			for (auto& pmx_node : loader.get_nodes())
			{
				nodes.emplace_back();
				auto& node = nodes.back();
				node.name = pmx_node.name;
				node.parent_index = (pmx_node.parent_node_index >= loader.get_nodes().size()) ? 0 : pmx_node.parent_node_index + 1;

				node.scale = pmx_node.calculate_data.init_scale;
				node.rotation = pmx_node.calculate_data.init_rotation;
				node.translation = pmx_node.calculate_data.init_translation;
			}
		}

		//	メッシュ構築
		//	ノードはとりあえず末尾に追加していく
		for (auto& pmx_subset : loader.get_subsets())
		{
			if (pmx_subset.count == 0)
				continue;

			//	基本設定
			meshes.emplace_back();
			auto& mesh = meshes.back();
			mesh.material_index = pmx_subset.material_index;
			mesh.node_index = nodes.size();

			//	メッシュノード追加
			{
				nodes.emplace_back();
				auto& node = nodes.back();
				node.name = std::string("mesh node ") + std::to_string(meshes.size());
				node.parent_index = 0;	//	root nodeは0番固定なので
			}

			//	頂点バッファ・インデックスバッファを振り分ける
			std::vector<pmx_model::vertex> pmx_vertices;
			std::vector<UINT> pmx_indices;
			{
				std::unordered_map<size_t, size_t> bones_index_map;
				//	未設定ボーンを0番に置く
				{
					auto mesh_bone_index = mesh.bones.size();
					bones_index_map.insert(std::make_pair(pmx_invalid_target_id, mesh_bone_index));

					//	ボーン情報設定
					mesh.bones.emplace_back();
					auto& bone = mesh.bones.back();
					bone.node_index = mesh.node_index;
					bone.offset_matrix = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
				}

				//	頂点毎の情報を割り振り
				std::unordered_map<size_t, size_t> verices_index_map;
				//for (size_t index = 0; index < pmx_subset.count; ++index)
				for (INT64 index = pmx_subset.count - 1; index >= 0; --index)
				{
					size_t vertex_index = loader.get_indices()[pmx_subset.start + index];
					auto it_vertex = verices_index_map.find(vertex_index);
					if (it_vertex != verices_index_map.end())
					{
						vertex_index = it_vertex->second;
					}
					else
					{
						verices_index_map.insert(std::make_pair(vertex_index, pmx_vertices.size()));
						pmx_vertices.emplace_back(loader.get_vertices()[vertex_index]);
						vertex_index = pmx_vertices.size() - 1;

						//	ボーンはここで処理する
						auto& pmx_vertex = pmx_vertices.back();
						//	ボーン差し替え処理
						UINT* pmx_vertex_bones = &pmx_vertex.bone_indices.x;
						for (UINT index = 0; index < 4; ++index)
						{
							auto& pmx_bone_index = pmx_vertex_bones[index];
							auto it_bone = bones_index_map.find(pmx_bone_index);
							if (it_bone != bones_index_map.end())
							{
								pmx_bone_index = static_cast<UINT>(it_bone->second);
							}
							else
							{
								auto mesh_bone_index = mesh.bones.size();
								bones_index_map.insert(std::make_pair(pmx_bone_index, mesh_bone_index));

								//	ボーン情報設定
								mesh.bones.emplace_back();
								auto& bone = mesh.bones.back();
								bone.node_index = pmx_bone_index + 1;	//	ルートノード考慮
								bone.offset_matrix = loader.get_nodes()[pmx_bone_index].offset_matrix;
								pmx_bone_index = static_cast<UINT>(mesh_bone_index);
							}
						}
					}
					pmx_indices.emplace_back(static_cast<UINT>(vertex_index));
				}
			}

			//	頂点バッファビュー生成
			{
				std::vector<buffer_view> views(ARRAYSIZE(format_datas));
				for (size_t index = 0; index < views.size(); ++index)
				{
					auto& view = views[index];
					view.format = format_datas[index].f;
					view.stride_in_bytes = graphics_utility::get_byte_offset_from_format(view.format);
					view.size_in_bytes = view.stride_in_bytes * pmx_vertices.size();
					view.row_data.resize(view.size_in_bytes);
				}
				mesh.bounds_min = {+FLT_MAX, +FLT_MAX, +FLT_MAX};
				mesh.bounds_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
				for (size_t index = 0; index < pmx_vertices.size(); ++index)
				{
					auto& vertex = pmx_vertices[index];
					reinterpret_cast<DirectX::XMFLOAT3*>(views[format_id::position].row_data.data())[index] = vertex.position;
					mesh.bounds_min.x = std::min(vertex.position.x, mesh.bounds_min.x);
					mesh.bounds_min.y = std::min(vertex.position.y, mesh.bounds_min.y);
					mesh.bounds_min.z = std::min(vertex.position.z, mesh.bounds_min.z);
					mesh.bounds_max.x = std::max(vertex.position.x, mesh.bounds_max.x);
					mesh.bounds_max.y = std::max(vertex.position.y, mesh.bounds_max.y);
					mesh.bounds_max.z = std::max(vertex.position.z, mesh.bounds_max.z);
					reinterpret_cast<DirectX::XMFLOAT3*>(views[format_id::normal].row_data.data())[index] = vertex.normal;
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_0].row_data.data())[index] = vertex.texcoord;
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_1].row_data.data())[index] = { vertex.additional_uv[0].x, vertex.additional_uv[0].y };
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_2].row_data.data())[index] = { vertex.additional_uv[0].z, vertex.additional_uv[0].w };
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_3].row_data.data())[index] = { vertex.additional_uv[1].x, vertex.additional_uv[1].y };
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_4].row_data.data())[index] = { vertex.additional_uv[1].z, vertex.additional_uv[1].w };
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_5].row_data.data())[index] = { vertex.additional_uv[2].x, vertex.additional_uv[2].y };
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_6].row_data.data())[index] = { vertex.additional_uv[2].z, vertex.additional_uv[2].w };
					reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_7].row_data.data())[index] = { vertex.additional_uv[3].x, vertex.additional_uv[3].y };
				//	reinterpret_cast<DirectX::XMFLOAT2*>(views[format_id::texcoord_8].row_data.data())[index] = { vertex.additional_uv[3].z, vertex.additional_uv[3].w };

					reinterpret_cast<DirectX::XMUINT4*>(views[format_id::joint_0].row_data.data())[index] = vertex.bone_indices;
					reinterpret_cast<DirectX::XMUINT4*>(views[format_id::joint_1].row_data.data())[index] = DirectX::XMUINT4(0, 0, 0, 0);
					reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::weight_0].row_data.data())[index] = vertex.bone_weights;
					reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::weight_1].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);

					reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_0].row_data.data())[index] = DirectX::XMFLOAT4(1, 1, 1, 1);
					reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_1].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);
				//	reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_2].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);
				//	reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_3].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);
				//	reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_4].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);
				//	reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_5].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);
				//	reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_6].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);
				//	reinterpret_cast<DirectX::XMFLOAT4*>(views[format_id::color_7].row_data.data())[index] = DirectX::XMFLOAT4(0, 0, 0, 0);

					//	接線は未対応
					reinterpret_cast<DirectX::XMFLOAT3*>(views[format_id::tangent].row_data.data())[index] = DirectX::XMFLOAT3(0, 0, 0);
				}
				for (size_t index = 0; index < views.size(); ++index)
				{
					auto& view = views[index];

					D3D11_BUFFER_DESC buffer_desc{};
					buffer_desc.ByteWidth = static_cast<UINT>(view.size_in_bytes);
					buffer_desc.Usage = D3D11_USAGE_DEFAULT;
					buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

					D3D11_SUBRESOURCE_DATA subresource_data{};
					subresource_data.pSysMem = view.row_data.data();
					hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &subresource_data, view.buffer.ReleaseAndGetAddressOf());

					_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
					mesh.vertex_buffer_views.insert(std::make_pair(format_datas[index].s, views[index]));
				}
			}

			//	インデックスバッファビュー生成
			{
				auto& view = mesh.index_buffer_view;
				view.format = DXGI_FORMAT_R32_UINT;
				view.stride_in_bytes = graphics_utility::get_byte_offset_from_format(view.format);
				view.size_in_bytes = view.stride_in_bytes * pmx_indices.size();
				view.row_data.resize(view.size_in_bytes);
				memcpy_s(view.row_data.data(), view.row_data.size() * view.stride_in_bytes,
						 pmx_indices.data(), pmx_indices.size() * view.stride_in_bytes);

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

		//	マテリアル生成
		for(auto& pmx_material : loader.get_materials())
		{
			auto& material = materials.emplace_back();
			material.set_name(pmx_material.name);
			material.set_domain(material::domains::surface);
			material.set_blend_mode(material::blend_modes::mask);
			material.set_alpha_clip_threshold(0.5f);
			material.set_shading_model(material::shading_models::default_lit);
			{
				material::set_surface_pipeline_states_desc descs[] =
				{
					{material::surface_paths::depth_write_pass, "pmx_depth"},
					{material::surface_paths::g_buffer_pass, "pmx_gbuffer"},
					{material::surface_paths::shadow_write_pass, "pmx_shadowmap"},
					{material::surface_paths::forward_pass, "pmx_forward"},
				};
				material.set_pipeline_state(descs, ARRAYSIZE(descs));
			}
			material.set_flag(material::flags::two_sided, system_utility::is_bitflag(pmx_material.draw_flag, pmx_material_draw_bitflag::cull_both));
			material.set_flag(material::flags::shadow_caster, system_utility::is_bitflag(pmx_material.draw_flag, pmx_material_draw_bitflag::use_shadow));
			material.set_flag(material::flags::shadow_receiver, system_utility::is_bitflag(pmx_material.draw_flag, pmx_material_draw_bitflag::use_shadow));

			if (system_utility::is_bitflag(pmx_material.draw_flag, pmx_material_draw_bitflag::use_edge_line))
			{
				material.set_float("outline_size", pmx_material.def.edge_size * ScaleFactorForMMDtoEMO);
			}
			else
			{
				material.set_float("outline_size", 0.0f);
			}

			material.set_float4("base_color_factor", pmx_material.def.diffuse_color);
			material.set_float4("specular_color", pmx_material.def.specular_color);
			material.set_float4("edge_color", pmx_material.def.edge_color);
			material.set_float("roughness", 0.8f);
			material.set_float("metalness", 0.1f);
			material.set_float("metalness", 0.1f);
			if(pmx_material.def.diffuse_texture_index < loader.get_textures().size())
			{
				material.set_texture("BaseMap", loader.get_textures()[pmx_material.def.diffuse_texture_index]);
			}
			if(pmx_material.def.sphere_texture_index < loader.get_textures().size())
			{
				material.set_texture("SphereMap", loader.get_textures()[pmx_material.def.sphere_texture_index]);
			}
			if(pmx_material.def.ramp_texture_index < loader.get_textures().size())
			{
				material.set_texture("RampMap", loader.get_textures()[pmx_material.def.ramp_texture_index]);
			}
		}

		//	アニメーション生成
		{
			//	初期姿勢用アニメーション
			{
				animation::data animation;
				animation.name = "default";
				animation.end_time = 1.0f;

				//	ルートノード登録
				animation.nodes.reserve(nodes.size());
				{
					animation.nodes.emplace_back();
					auto& na = animation.nodes.back();
					na.nodes.emplace_back();
					na.nodes.back().time = 0.0f;
					na.nodes.back().scale.x = ScaleFactorForMMDtoEMO;
					na.nodes.back().scale.y = ScaleFactorForMMDtoEMO;
					na.nodes.back().scale.z = ScaleFactorForMMDtoEMO;
					DirectX::XMStoreFloat4(&na.nodes.back().rotation, DirectX::XMQuaternionRotationRollPitchYaw(RotationFactorForMMDtoEMO.x, RotationFactorForMMDtoEMO.y, RotationFactorForMMDtoEMO.z));
				}
				//	ボーン分のノードを登録
				for (auto& node : loader.get_nodes())
				{
					animation.nodes.emplace_back();
					auto& na = animation.nodes.back();
					na.nodes.emplace_back();
					na.nodes.back().time = 0.0f;
					na.nodes.back().scale = node.calculate_data.init_scale;
					na.nodes.back().rotation = node.calculate_data.init_rotation;
					na.nodes.back().translation = node.calculate_data.init_translation;
				}
				//	メッシュノード分登録
				for(auto& node : meshes)
				{
					animation.nodes.emplace_back();
					auto& na = animation.nodes.back();
					na.nodes.emplace_back();
				}
				animations.emplace_back(animation);
			}
			//	登録されているアニメーションを設定
			for(auto& pmx_animation : loader.get_animations())
			{
				animation::data animation;
				animation.name = pmx_animation.second.name;
				animation.end_time = pmx_animation.second.end_time;

				//	サイズ調整
				animation.nodes.reserve(nodes.size());

				//	ルートノード登録
				{
					animation.nodes.emplace_back();
					auto& aa = animation.nodes.back();
					aa.nodes.emplace_back();
					auto& sa = aa.nodes.back();
					sa.scale.x = ScaleFactorForMMDtoEMO;
					sa.scale.y = ScaleFactorForMMDtoEMO;
					sa.scale.z = ScaleFactorForMMDtoEMO;
					DirectX::XMStoreFloat4(&sa.rotation, DirectX::XMQuaternionRotationRollPitchYaw(RotationFactorForMMDtoEMO.x, RotationFactorForMMDtoEMO.y, RotationFactorForMMDtoEMO.z));
					aa.nodes.emplace_back();
					auto& ea = aa.nodes.back();
					ea.time = animation.end_time;
					ea.scale.x = ScaleFactorForMMDtoEMO;
					ea.scale.y = ScaleFactorForMMDtoEMO;
					ea.scale.z = ScaleFactorForMMDtoEMO;
					DirectX::XMStoreFloat4(&ea.rotation, DirectX::XMQuaternionRotationRollPitchYaw(RotationFactorForMMDtoEMO.x, RotationFactorForMMDtoEMO.y, RotationFactorForMMDtoEMO.z));
				}

				//	ボーン分のノードを登録
				for (auto& nodes : pmx_animation.second.nodes)
				{
					animation.nodes.emplace_back();
				}
				float sampling_rate = system_units::convert_fps_to_sec(1.0f);
				float sampling_time = 0;
				for(sampling_time = 0; sampling_time <= animation.end_time; sampling_time += sampling_rate)
				{
					std::vector<pmx_model::node_animation_data> node_datas;
					loader.compute_animation(animation.name, sampling_time, node_datas);

					UINT node_index = 1;
					for (auto& node : node_datas)
					{
						auto& animation_nodes = animation.nodes[node_index++];
						animation_nodes.nodes.emplace_back();
						auto& na = animation_nodes.nodes.back();
						na.time = sampling_time;
						na.scale = node.scale;
						na.rotation = node.rotation;
						na.translation = node.translation;
					}
				}
				if (sampling_time > animation.end_time)
				{
					std::vector<pmx_model::node_animation_data> node_datas;
					loader.compute_animation(animation.name, animation.end_time, node_datas);

					UINT node_index = 1;
					for (auto& node : node_datas)
					{
						auto& animation_nodes = animation.nodes[node_index++];
						animation_nodes.nodes.emplace_back();
						auto& na = animation_nodes.nodes.back();
						na.time = animation.end_time;
						na.scale = node.scale;
						na.rotation = node.rotation;
						na.translation = node.translation;
					}
				}

				//	メッシュノード分登録
				for (auto& node : meshes)
				{
					animation.nodes.emplace_back();
					auto& aa = animation.nodes.back();
					aa.nodes.emplace_back();
					auto& sa = aa.nodes.back();
					aa.nodes.emplace_back();
					auto& ea = aa.nodes.back();
					ea.time = animation.end_time;
				}
				animations.emplace_back(animation);
			}
		}

		//	EMOに変換したのでファイルパスを改めて設定
		path.replace_extension("emo");
		resource.set_file_path(path.string());
		return true;
	}

}

bool data_converter::mmd::convert(model_resource& resource, const convert_model_data& convert_data)
{
	{
		std::filesystem::path path(convert_data.file_path);
		path.replace_extension("pmx");
		//	ファイル読み込み
		pmx_loader loader;
		{
			std::string directory = path.parent_path().string();

			std::ifstream	read_file;
			read_file.open(path, std::ios::in | std::ios::binary);
			if (!read_file)
				return false;

			std::string	file_buffer;
			file_buffer = std::string(std::istreambuf_iterator<char>(read_file), std::istreambuf_iterator<char>());
			bool succeeded = loader.load(file_buffer.data(), file_buffer.size(), directory);
			if (!succeeded)
				return false;

			pmx_model model = pmx_model(render_data::instance()->get_device().Get(), loader);
			if (!::convert(resource, model, convert_data))
				return false;
		}
	}

	return true;
}

bool data_converter::mmd::convert(animation_resource& resource, const convert_animation_data& convert_data)
{
	if (!convert_data.retarget_resource)
	{
		DEBUG_LOG_CATEGORY(logger::categorys::animation, "PMX Load NotFound Model");
		return false;
	}

	//	MMDモデルの場合、IK情報をアニメーション情報に変換するので、
	//	一旦元データのPMXを読み込む。無ければ作らない。今は。
	std::unique_ptr<pmx_model> model;
	{
		std::filesystem::path path(convert_data.retarget_resource->get_file_path());
		path.replace_extension("pmx");
		//	ファイル読み込み
		pmx_loader loader;
		{
			std::string directory = path.parent_path().string();

			std::ifstream	read_file;
			read_file.open(path, std::ios::in | std::ios::binary);
			if (!read_file)
				return false;

			std::string	file_buffer;
			file_buffer = std::string(std::istreambuf_iterator<char>(read_file), std::istreambuf_iterator<char>());
			bool succeeded = loader.load(file_buffer.data(), file_buffer.size(), directory);
			if (!succeeded)
				return false;

			model = std::make_unique<pmx_model>(render_data::instance()->get_device().Get(), loader);
		}

		//	モーション登録
		{
			std::filesystem::path path(convert_data.file_path);
			auto motion_name = path.filename();
			motion_name.replace_extension("");
			if (!model->add_vmd_animation(motion_name.string().c_str(), convert_data.file_path.data()))
				return false;
		}
	}

	auto& nodes = convert_data.retarget_resource->get_nodes_for_editor();
	auto& meshes = convert_data.retarget_resource->get_meshes_for_editor();
	auto& materials = convert_data.retarget_resource->get_materials_for_editor();

	//	登録されているアニメーションを設定
	for(auto& pmx_animation : model->get_animations())
	{
		animation::data animation;
		animation.name = pmx_animation.second.name;
		animation.end_time = pmx_animation.second.end_time;

		//	サイズ調整
		animation.nodes.reserve(nodes.size());

		//	ルートノード登録
		{
			animation.nodes.emplace_back();
			auto& aa = animation.nodes.back();
			aa.nodes.emplace_back();
			auto& sa = aa.nodes.back();
			sa.scale.x = ScaleFactorForMMDtoEMO;
			sa.scale.y = ScaleFactorForMMDtoEMO;
			sa.scale.z = ScaleFactorForMMDtoEMO;
			DirectX::XMStoreFloat4(&sa.rotation, DirectX::XMQuaternionRotationRollPitchYaw(RotationFactorForMMDtoEMO.x, RotationFactorForMMDtoEMO.y, RotationFactorForMMDtoEMO.z));
			aa.nodes.emplace_back();
			auto& ea = aa.nodes.back();
			ea.time = animation.end_time;
			ea.scale.x = ScaleFactorForMMDtoEMO;
			ea.scale.y = ScaleFactorForMMDtoEMO;
			ea.scale.z = ScaleFactorForMMDtoEMO;
			DirectX::XMStoreFloat4(&ea.rotation, DirectX::XMQuaternionRotationRollPitchYaw(RotationFactorForMMDtoEMO.x, RotationFactorForMMDtoEMO.y, RotationFactorForMMDtoEMO.z));
		}

		//	ボーン分のノードを登録
		for (auto& nodes : pmx_animation.second.nodes)
		{
			animation.nodes.emplace_back();
		}
		float sampling_time = 0;
		for(sampling_time = 0; sampling_time <= animation.end_time; sampling_time += AnimationSamplingRate)
		{
			std::vector<pmx_model::node_animation_data> node_datas;
			model->compute_animation(animation.name, sampling_time, node_datas);

			UINT node_index = 1;
			for (auto& node : node_datas)
			{
				auto& animation_nodes = animation.nodes[node_index++];
				animation_nodes.nodes.emplace_back();
				auto& na = animation_nodes.nodes.back();
				na.time = sampling_time;
				na.scale = node.scale;
				na.rotation = node.rotation;
				na.translation = node.translation;
			}
		}
		if (sampling_time > animation.end_time)
		{
			std::vector<pmx_model::node_animation_data> node_datas;
			model->compute_animation(animation.name, animation.end_time, node_datas);

			UINT node_index = 1;
			for (auto& node : node_datas)
			{
				auto& animation_nodes = animation.nodes[node_index++];
				animation_nodes.nodes.emplace_back();
				auto& na = animation_nodes.nodes.back();
				na.time = animation.end_time;
				na.scale = node.scale;
				na.rotation = node.rotation;
				na.translation = node.translation;
			}
		}

		//	メッシュノード分登録
		for (auto& node : meshes)
		{
			animation.nodes.emplace_back();
			auto& aa = animation.nodes.back();
			aa.nodes.emplace_back();
			auto& sa = aa.nodes.back();
			aa.nodes.emplace_back();
			auto& ea = aa.nodes.back();
			ea.time = animation.end_time;
		}

		//	アニメーション登録
		resource.add_animation_data(animation);
		return true;
	}
	return false;
}

#endif	//	defined(USE_IMGUI)
