#include "scenes/editscene/edit_model_scene.h"

#include "model_instance.h"
#include "camera.h"

#include "object/light_object.h"

#include "object/model_object.h"

#include "custom_pipeline_state.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"

namespace
{
	//	モデル
	constexpr TCHAR* ModelFileDialogFilter =
		L"モデルファイル (*.emo, *.pmx, *.gltf, *.glb)\0*.emo;*.pmx;*.gltf;*.glb;\0\
		すべてのファイル (*.*)\0*.*\0";
	//	アニメーション
	constexpr TCHAR* AnimationFileDialogFilter =
		L"モデルファイル (*.vmd, *.gltf, *.glb)\0*.vmd;*.gltf;*.glb;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	リターゲティング用ファイルパス
	constexpr std::string_view RetargetFilePath = "Assets/Editor/RetargetFile.rfo";
}

bool edit_model_scene::initialize_edit_core()
{
	//	エディターカメラ生成
	{
		auto camera = std::make_shared<editor_camera>(main_camera);
		camera->set_position(system_units::convert_m_to_unit({ 0, 1, 8 }));
		camera->set_rotation({ 0, DirectX::XM_PI, 0 });
		camera->set_rotation_speed(camera->get_rotation_speed() * 0.25f);
		camera->set_move_speed(camera->get_move_speed() * 0.25f);
		register_object(camera);
	}

	//	エディターアイテム用コンテキスト生成
	{
		editor_context = editor_items::generate_context();
	}

	//	ライトのプリセット情報を設定
	{
		preset_lights preset_light;
		{
			//	preset_light.direction.direction = { 1.0f, -1.0f, -1.0f, 0.0f };
			preset_light.direction.color = { 255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f };
			preset_light.ambient_color = { 127.5f / 255.0f, 127.5f / 255.0f, 127.5f / 255.0f, 1.0f };
			preset_light_list.emplace_back(preset_light);
		}
		{
			//	preset_light.direction.direction = { -1.0f, -1.0f, 1.0f, 0.0f };
			preset_light.direction.color = { 128.0f / 255.0f, 128.0f / 255.0f, 255.0f / 255.0f, 1.0f };
			preset_light.ambient_color = { 64.0f / 255.0f, 64.0f / 255.0f, 128.0f / 255.0f, 1.0f };
			preset_light_list.emplace_back(preset_light);
		}
		{
			//	preset_light.direction.direction = { -0.5f, -1.0f, 0.0f, 0.0f };
			preset_light.direction.color = { 255.0f / 255.0f, 176.0f / 255.0f, 172.0f / 255.0f, 1.0f };
			preset_light.ambient_color = { 106.0f / 255.0f, 69.0f / 255.0f, 90.0f / 255.0f, 1.0f };
			preset_light_list.emplace_back(preset_light);
		}
	}

	if (!initialize_vertex_color_edit())
		return false;

	if (!initialize_animation())
		return false;

	//	リターゲティング用情報読込
	retarget_data.deserialize(RetargetFilePath);

	return true;
}

bool edit_model_scene::uninitialize_edit_core()
{
	if (!uninitialize_vertex_color_edit())
		return false;

	if (!uninitialize_animation())
		return false;

	//	リターゲティング用情報読込
	retarget_data.serialize(RetargetFilePath);

	return true;
}

void edit_model_scene::update_edit_core(float elapsed_time)
{
	update_vertex_color_edit();

	update_animation();
}

void edit_model_scene::imgui_render_edit_core()
{
	if (!edit_model)
		return;

	//	メッシュごとに表示を切り替えたいとき用ウィンドウ
	imgui_render_mesh_list();

	//	マテリアル編集系ウィンドウ
	imgui_render_material_window();

	//	頂点カラー編集系ウィンドウ
	imgui_render_vertex_color_edit();

	//	アニメーション編集系ウィンドウ
	imgui_render_animation();

	//	編集モデル表示
	if (display_bone && edit_model)
	{
		editor_items::render_model_bones(*edit_model->get_model_instance(), selected_bones);
	}
}

void edit_model_scene::imgui_render_preset_light()
{
	if (ImGui::Begin("Preset Ligt Window"))
	{
		bool edit_preset_light = ImGui::SliderInt("preset_light", &preset_light_index, 0, static_cast<INT>(preset_light_list.size() - 1));
		preset_light_index = std::clamp(preset_light_index, 0, static_cast<int>(preset_light_list.size() - 1));
		{
			auto& data = preset_light_list[preset_light_index];
			ImGui::BeginDisabled();
		//	ImGui::SliderFloat3("directional_light_direction", &data.direction.direction.x, -1.0f, +1.0f);
			ImGui::ColorEdit3("directional_light_color", &data.direction.color.x);
			ImGui::DragFloat("directional_light_intensity", &data.direction.color.w, 0.0f, 1000.0f);
			ImGui::ColorEdit3("ambient_light_color", &data.ambient_color.x);
			ImGui::Separator();
			ImGui::EndDisabled();

			if (edit_model && edit_preset_light)
			{
				auto light = object_manager::instance()->search_object<light_object>("平行光源");
				if (light)
				{
					auto dlo = std::static_pointer_cast<directional_light_object>(light);
					dlo->set_color(data.direction.color);
				}
				for (auto& material : edit_model->get_model_instance()->get_materials())
				{
					material.set_float4("fake_ambient_color", data.ambient_color);
				}
			}
		}
	}
	ImGui::End();
}

void edit_model_scene::imgui_render_mesh_list()
{
	if (ImGui::Begin("Mesh List Window"))
	{
		for (auto& mesh : edit_model_meshes)
		{
			auto& node = edit_model->get_model_instance()->get_nodes()[mesh.node_index];
			ImGui::PushID(node.name.data());
			if (ImGui::Checkbox("", &mesh.visible))
			{
				for (auto& md : mesh.meshes)
				{
					md->visible = mesh.visible;
				}
			}
			ImGui::SameLine();
			ImGui::Text(node.name.data());
			ImGui::PopID();
		}
	}
	ImGui::End();
}

void edit_model_scene::imgui_render_material_window()
{
	if (ImGui::Begin("Material Window"))
	{
		for (auto& material : edit_model->get_model_instance()->get_materials())
		{
			if (ImGui::TreeNode(system_utility::convert_shift_jis_to_utf8(material.get_name()).data()))
			{
				editor_items::edit_material(editor_context, material);
				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void edit_model_scene::imgui_model_loader_window()
{
#pragma region 読込みウィンドウ
	if (!load_model_path.empty())
	{
		std::filesystem::path path(load_model_path);
		std::filesystem::path extension = path.extension();

		if (extension == std::filesystem::path(".pmx"))
		{
			//	PMX > EMOに変換
			std::shared_ptr<model_resource> resource = std::make_shared<model_resource>();
			data_converter::mmd::convert_model_data data;
			data.file_path = load_model_path;
			if (data_converter::mmd::convert(*resource, data))
			{
				next_edit_model = std::make_shared<model_object>(load_model_path, resource);
			}
			load_model_path.clear();
		}
		else if (extension == std::filesystem::path(".gltf") || extension == std::filesystem::path(".glb"))
		{
			bool show_flag = !load_model_path.empty();
			if (ImGui::Begin("Convert Gltf Data", &show_flag))
			{
				static data_converter::gltf::convert_model_data data;
				data.file_path = load_model_path;

				ImGui::Checkbox("compute_tangent", &data.is_compute_tangent);
				ImGui::Checkbox("populate_vertex_color", &data.is_populate_vertex_color);
				ImGui::Checkbox("combine_unskinned_meshes", &data.is_combine_unskinned_meshes);
				ImGui::Checkbox("normalize_weight", &data.is_normalize_weight);

				ImGui::Checkbox("change_coordinate_system", &data.is_change_coordinate_system);
				ImGui::Checkbox("change_coordinate_system_indices", &data.is_change_coordinate_system_indices);
				ImGui::Separator();
				ImGui::Checkbox("generate_shadow_mesh", &data.is_generate_shadow_mesh);
				ImGui::SliderInt("generate_lod_mesh_count", &data.generate_lod_mesh_count, 0, 16);

				//	[エンジン改造:接線に再計算法線を含める対応]
				ImGui::Separator();
				ImGui::Checkbox("recomputed_normal", &data.is_recomputed_normal);
				ImGui::Checkbox("recomputed_normal_to_copy_tangent", &data.is_recomputed_normal_to_copy_tangent);
				//	[エンジン改造:接線に再計算法線を含める対応]

				if (ImGui::Button("Convert"))
				{
					std::shared_ptr<model_resource> resource = std::make_shared<model_resource>();
					if (data_converter::gltf::convert(*resource, data))
					{
						next_edit_model = std::make_shared<model_object>(load_model_path, resource);
					}
					load_model_path.clear();
				}
			}
			ImGui::End();
			//	閉じられたらクリアしておく
			if (!show_flag)
			{
				load_model_path.clear();
			}
		}
		else if (extension == std::filesystem::path(".emo"))
		{
			//	EMO で読み込み
			model_resource_ptr resource;
			if (model_resource_manageer::instance()->load_model(resource, load_model_path))
			{
				next_edit_model = std::make_shared<model_object>(load_model_path, resource);
			}
			load_model_path.clear();
		}
		else
		{
			assert(!L"Compatible Format Model.");
			load_model_path.clear();
		}
	}

	// 非表示対応
	if (next_edit_model)
	{
		next_edit_model->set_active(false);
		if (edit_model)
			edit_model->set_active(false);
	}
#pragma endregion // 読込みウィンドウ

	//	モデル読込み関係
	{
		ImGui::BeginDisabled(!load_model_path.empty());

		ImGui::Text("Model ");
		//	モデル読み込み
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			std::string file_path;
			if (editor_items::open_file_dialog(ModelFileDialogFilter, file_path))
			{
				load_model_path = file_path;
				is_edit_vertex_color = false;
			}
		}

		//	モデル保存
		if (edit_model)
		{
			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				save_model();
			}
		}

		ImGui::EndDisabled();
	}

	//	リターゲット情報ウィンドウ
	if (edit_model)
	{
		//	再設定
		std::string file_name = std::string(edit_model->get_model_instance()->get_resource()->get_file_path());
		if (!retarget_data.has_profile(file_name))
		{
			retarget_profile profile;
			profile.bone_map.clear();
			auto& node_list = edit_model->get_model_instance()->get_nodes();
			auto& mesh_list = edit_model->get_model_instance()->get_meshes();
			for (auto& mesh : mesh_list)
			{
				for (auto& bone : mesh.mesh->bones)
				{
					auto& bone_list = profile.bone_map;

					auto node_name = node_list[bone.node_index].name;
					if (bone_list.find(node_name) == bone_list.end())
					{
						retarget_profile::bone_data data;
						data.target_name = "";
						profile.bone_map.insert(std::make_pair(node_name, data));
					}
				}
			}
			std::vector<retarget_profile> vector;
			vector.push_back(profile);
			retarget_data.profiles.insert(std::make_pair(std::string(file_name), vector));
		}

		if (ImGui::Begin("Retarget Window"))
		{
			auto it = retarget_data.profiles.find(file_name);
			//	選択リスト
			{
				int max_index = static_cast<int>(it->second.size());
				int index = static_cast<int>(selected_retarget_data_index);
				ImGui::SliderInt("index", &index, 0, max_index - 1);
				selected_retarget_data_index = static_cast<UINT>(index);
				ImGui::SameLine();
				if (ImGui::Button("+"))
				{
					retarget_profile profile;
					profile.bone_map.clear();
					auto& node_list = edit_model->get_model_instance()->get_nodes();
					auto& mesh_list = edit_model->get_model_instance()->get_meshes();
					for (auto& mesh : mesh_list)
					{
						for (auto& bone : mesh.mesh->bones)
						{
							auto& bone_list = profile.bone_map;

							auto node_name = node_list[bone.node_index].name;
							if (bone_list.find(node_name) == bone_list.end())
							{
								retarget_profile::bone_data data;
								data.target_name = "";
								profile.bone_map.insert(std::make_pair(node_name, data));
							}
						}
					}
					it->second.emplace_back(profile);
				}
				if (it->second.size() > 1)
				{
					ImGui::SameLine();
					if (ImGui::Button("-"))
					{
						it->second.erase(it->second.begin() + selected_retarget_data_index);
						selected_retarget_data_index = std::min(selected_retarget_data_index, it->second.size() - 1);
					}
				}
			}
			auto& retarget_page = it->second[selected_retarget_data_index];
			switch (retarget_page.mode)
			{
				case retarget_profile::modes::normal:
				{
					//	ベーススケーリング
					ImGui::DragFloat3("base_scaling", &retarget_page.base_scaling.x, 0.001f, 0.0f, 1000.0f);

					ImGui::Separator();
					//	ボーン選択
					selected_bones.clear();
					for (auto& bone_data : retarget_page.bone_map)
					{
						//	名前のないボーンは無視
						if (bone_data.first.empty())
							continue;

						ImGui::PushID(bone_data.first.data());
						bool is_active = false;
						if (ImGui::TreeNode(bone_data.first.data()))
						{
							is_active = is_active || ImGui::IsItemActive();
							ImGui::InputText("target_node", bone_data.second.target_name); is_active = is_active || ImGui::IsItemActive();
							ImGui::DragFloat3("scale", &bone_data.second.scale.x); is_active = is_active || ImGui::IsItemActive();
							ImGui::DragFloat4("rotation", &bone_data.second.rotation.x); is_active = is_active || ImGui::IsItemActive();
							ImGui::DragFloat3("translation", &bone_data.second.translation.x); is_active = is_active || ImGui::IsItemActive();
							ImGui::Checkbox("lock_scale", &bone_data.second.is_lock_scale); is_active = is_active || ImGui::IsItemActive();
							ImGui::SameLine();
							ImGui::Checkbox("lock_rotation", &bone_data.second.is_lock_rotation); is_active = is_active || ImGui::IsItemActive();
							ImGui::SameLine();
							ImGui::Checkbox("lock_translation", &bone_data.second.is_lock_translation); is_active = is_active || ImGui::IsItemActive();
							ImGui::TreePop();
						}
						if (is_active)
						{
							selected_bones.push_back(bone_data.first);
						}
						ImGui::Separator();
						ImGui::PopID();
					}
					break;
				}
			}
		}
		ImGui::End();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//	頂点カラー編集系
bool edit_model_scene::initialize_vertex_color_edit()
{
	//	頂点表示マテリアル
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto mat = std::make_shared<material>();
		mat->set_domain(material::domains::surface);
		mat->set_blend_mode(material::blend_modes::translucent);
		mat->set_alpha_clip_threshold(-1.0f);
		mat->set_shading_model(material::shading_models::unlit);
		mat->set_flag(material::flags::wire, true);

		mat->set_pipeline_state(material::surface_paths::depth_write_pass, "edit_model_color_depth_write");
		mat->set_pipeline_state(material::surface_paths::g_buffer_pass, "edit_model_color_gbuffer");
		mat->set_pipeline_state(material::surface_paths::forward_pass, "edit_model_color_forward");
		mat->set_pipeline_state(material::surface_paths::shadow_write_pass, "edit_model_color_csm");
		mat->set_float("line_factor", 0.001f);
		mat->set_float3("line_color", { 0, 1, 0 });
		vertex_color_edit_material = mat;
	}

	//	マウス購読
	{
		static constexpr std::string_view keys[] =
		{
			editor_camera::left_click_down_identifier,
			editor_camera::left_click_hold_identifier,
		};
		for (int index = 0; index < ARRAYSIZE(keys); ++index)
		{
			left_click_data_identifier[index] = object_manager::instance()->get_messenger().add_receiver(
				keys[index],
				[this](void* data_ptr) { on_mouse_left_down_and_hold(data_ptr); });
		}

		left_click_data_identifier[2] = object_manager::instance()->get_messenger().add_receiver(
			editor_camera::left_click_release_identifier,
			[this](void* data_ptr) { on_mouse_left_release(data_ptr); });
	}
	return true;
}

bool edit_model_scene::uninitialize_vertex_color_edit()
{
	//	左クリック登録解除
	{
		for (auto& id : left_click_data_identifier)
		{
			object_manager::instance()->get_messenger().remove_receiver(id);
		}
	}
	return true;
}

void edit_model_scene::update_vertex_color_edit()
{
	//	頂点編集時の座標表示
	if (brush_data.on)
	{
		PUSH_DEBUG_PRIMITIVE_SPHERE(brush_data.position, brush_data.radius, DirectX::XMFLOAT4(1, 1, 1, 1), true);
	}
}

void edit_model_scene::imgui_render_vertex_color_edit()
{
	if (ImGui::Begin("Edit Vertex Color Window"))
	{
		if (ImGui::Button("Edit Vertex Color Mode"))
		{
			is_edit_vertex_color = !is_edit_vertex_color;
			if (vertex_color_edit_model)
				vertex_color_edit_model->set_active(is_edit_vertex_color);
		}
		ImGui::SliderFloat("radius", &brush_data.radius, 0.01f, 10.0f);
		ImGui::PushID("## vertex edit colors");
		ImGui::Checkbox("shading threshold offset", reinterpret_cast<bool*>(&brush_data.color_flags.x));
		ImGui::Checkbox("outline depth expansion factor", reinterpret_cast<bool*>(&brush_data.color_flags.y));
		ImGui::Checkbox("outline depth offet", reinterpret_cast<bool*>(&brush_data.color_flags.z));
		ImGui::Checkbox("outline scale factor", reinterpret_cast<bool*>(&brush_data.color_flags.w));
		ImGui::PopID();

		ImGui::SliderFloat("shading threshold offset", &brush_data.color.x, 0.0f, 1.0f);
		ImGui::SliderFloat("outline depth expansion factor", &brush_data.color.y, 0.0f, 1.0f);
		ImGui::SliderFloat("outline depth offet", &brush_data.color.z, 0.0f, 1.0f);
		ImGui::SliderFloat("outline scale factor", &brush_data.color.w, 0.0f, 1.0f);
	}
	ImGui::End();
}

void edit_model_scene::on_mouse_left_down_and_hold(void* data_ptr)
{
	if (!edit_model || !is_edit_vertex_color)
		return;

	auto data = static_cast<editor_camera::mouse_position_data*>(data_ptr);

	auto View = DirectX::XMLoadFloat4x4(&main_camera->get_camera()->get_view_matrix());
	auto Projection = DirectX::XMLoadFloat4x4(&main_camera->get_camera()->get_projection_matrix());

	DirectX::XMFLOAT3 start_position, end_position;
	DirectX::XMStoreFloat3(&start_position,
		DirectX::XMVector3Unproject(DirectX::XMVectorSet(data->position.x, data->position.y, 0, 1),
			0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1,
			Projection, View, DirectX::XMMatrixIdentity()));
	DirectX::XMStoreFloat3(&end_position,
		DirectX::XMVector3Unproject(DirectX::XMVectorSet(data->position.x, data->position.y, 1, 1),
			0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1,
			Projection, View, DirectX::XMMatrixIdentity()));

	//	ヒット位置算出
	float vertex_hit_length = FLT_MAX;
	for (auto& instance_mesh : edit_model->get_model_instance()->get_meshes())
	{
		if (!instance_mesh.mesh || !instance_mesh.visible)
			continue;

		auto& mesh = *instance_mesh.mesh;
		auto& positions = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
		auto& indiceds = mesh.index_buffer_view;
		if (positions == mesh.vertex_buffer_views.end() || indiceds.count() == 0)
			continue;

		collision::triangles triangles = {};
		triangles.positions = reinterpret_cast<const DirectX::XMFLOAT3*>(positions->second.row_data.data());
		triangles.indices = reinterpret_cast<const UINT*>(indiceds.row_data.data());
		triangles.index_start = indiceds.offset();
		triangles.index_count = indiceds.count();
		triangles.reverse_index = false;
		triangles.indices_stride_16 = indiceds.stride_in_bytes == 2;
		collision::intersect_data result;
		if (collision::ray_cast(start_position, end_position, triangles, &result))
		{
			if (result[0].length < vertex_hit_length)
			{
				brush_data.position = result[0].point;
				vertex_hit_length = result[0].length;
			}
		}
	}

	//	ヒット位置周辺の頂点色を修正
	if (vertex_hit_length < FLT_MAX)
	{
		brush_data.on = true;
		for (auto& instance_mesh : edit_model->get_model_instance()->get_meshes())
		{
			if (!instance_mesh.mesh || !instance_mesh.visible)
				continue;

			auto& mesh = *const_cast<model_resource::mesh*>(instance_mesh.mesh);
			auto& position_view = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
			auto& color_view = mesh.vertex_buffer_views.find(semantic("COLOR", 0));
			if (position_view == mesh.vertex_buffer_views.end() || color_view == mesh.vertex_buffer_views.end())
				continue;

			bool is_change = false;
			auto positions = reinterpret_cast<DirectX::XMFLOAT3*>(position_view->second.row_data.data());
			auto colors = reinterpret_cast<DirectX::XMFLOAT4*>(color_view->second.row_data.data());
			for (size_t index = position_view->second.offset(); index < position_view->second.count(); ++index)
			{
				auto p = positions + index;
				DirectX::XMFLOAT3 d;
				d.x = p->x - brush_data.position.x;
				d.y = p->y - brush_data.position.y;
				d.z = p->z - brush_data.position.z;
				if (d.x * d.x + d.y * d.y + d.z * d.z < brush_data.radius * brush_data.radius)
				{
					colors[index].x = math::lerp(colors[index].x, brush_data.color.x, static_cast<float>(brush_data.color_flags.x));
					colors[index].y = math::lerp(colors[index].y, brush_data.color.y, static_cast<float>(brush_data.color_flags.y));
					colors[index].z = math::lerp(colors[index].z, brush_data.color.z, static_cast<float>(brush_data.color_flags.z));
					colors[index].w = math::lerp(colors[index].w, brush_data.color.w, static_cast<float>(brush_data.color_flags.w));
					is_change = true;
				}
			}
			if (is_change)
			{
				auto device_context = render_data::instance()->get_immediate_context();
				device_context->UpdateSubresource(color_view->second.buffer.Get(), 0, nullptr, colors, 0, 0);
			}
		}
	}
}

void edit_model_scene::on_mouse_left_release(void* data_ptr)
{
	brush_data.on = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool edit_model_scene::initialize_animation()
{
	return true;
}

bool edit_model_scene::uninitialize_animation()
{
	return true;
}

void edit_model_scene::update_animation()
{
}

void edit_model_scene::imgui_render_animation()
{
	if (ImGui::Begin("Edit Animation Window"))
	{
		bool is_delete = false;
		//	アニメーションプレイヤー
		int current_animation_id = static_cast<int>(edit_model->get_animation_index());
		auto animation_names = edit_model->get_animation_names();
		{
			int animation_id = current_animation_id;
			int animation_max = static_cast<int>(edit_model->get_animation_max());
			float animation_end_time = edit_model->get_animation_end_time();
			bool animation_loop = edit_model->get_animation_loop();
			bool animation_playing = edit_model->get_animation_speed() > 0.5f;
			float animation_time = edit_model->get_animation_time();

			//	アニメーションリスト
			std::vector<const char*> name_list;
			for (auto& animation_name : animation_names)
			{
				name_list.emplace_back(animation_name.data());
			}
			ImGui::Combo("animation", &animation_id, name_list.data(), static_cast<INT>(name_list.size()));

			//	追加/削除
			ImGui::SameLine();
			if (ImGui::Button("Add"))
			{
				std::string file_path;
				if (editor_items::open_file_dialog(AnimationFileDialogFilter, file_path))
				{
					std::filesystem::path path(file_path);
					std::filesystem::path extension = path.extension();

					animation_resource animations;

					auto& resource = edit_model->get_model_instance()->get_resource();
					if (extension == std::filesystem::path(".vmd"))
					{
						//	MMD用
						data_converter::mmd::convert_animation_data data;
						data.file_path = file_path;
						data.retarget_resource = resource;
						//	アニメーション変換
						data_converter::mmd::convert(animations, data);
					}
					else if (extension == std::filesystem::path(".gltf") || extension == std::filesystem::path(".glb"))
					{
						data_converter::gltf::convert_animation_data data;
						data.file_path = file_path;
						//	リターゲッティング情報登録
						auto it = retarget_data.profiles.find(std::string(resource->get_file_path()));
						if (it != retarget_data.profiles.end())
						{
							data.retarget_profile = it->second[selected_retarget_data_index];
							data.retarget_resource = resource;
						}
						//	アニメーション変換
						data_converter::gltf::convert(animations, data);
					}
					else
					{
						assert(!L"Compatible Format Model.");
						load_model_path.clear();
					}
					//	アニメーション登録
					//	同名アニメーションは上書き
					auto& animation_list = resource->get_animations_for_editor();
					for (auto& animation : animations.get_animation())
					{
						bool find = false;
						for (auto& animation_data : animation_list)
						{
							if (animation.name == animation_data.name)
							{
								animation_data = animation;
								find = true;
								break;
							}
						}
						if (!find)
						{
							animation_list.push_back(animation);
						}
					}
				}
			}
			if (animation_id != 0)
			{
				ImGui::SameLine();
				if (ImGui::Button("delete"))
					is_delete = true;
			}

			//	操作系
			ImGui::Checkbox("loop", &animation_loop);
			ImGui::SameLine();
			if (ImGui::Button(animation_playing ? "stop" : "play"))
			{
				animation_playing = !animation_playing;
			}
			ImGui::SameLine();
			if (ImGui::Button(display_bone ? "hide bone" : "display bone"))
			{
				display_bone = !display_bone;
			}

			ImGui::SliderFloat("animation time", &animation_time, 0.0f, animation_end_time + 0.001f, "%.6f");
			ImGui::Separator();
			//	編集モデルのアニメーション変更
			if (current_animation_id != animation_id)
			{
				edit_model->play_animation(animation_id, animation_loop, 0.0f);
			}
			else
			{
				edit_model->set_animation_loop(animation_loop);
				edit_model->set_animation_time(animation_time);
			}
			edit_model->set_animation_speed(animation_playing ? 1.0f : 0.0f);
		}

		//	アニメーション情報編集
		if (current_animation_id > 0 && current_animation_id < animation_names.size())
		{
			auto& animation_datas = edit_model->get_model_instance()->get_resource()->get_animations_for_editor();
			editor_items::edit_animation_data(editor_context, animation_datas[current_animation_id]);
		}

		if (is_delete)
		{
			auto& animation_datas = edit_model->get_model_instance()->get_resource()->get_animations_for_editor();
			animation_datas.erase(animation_datas.begin() + current_animation_id);
			if (current_animation_id >= animation_names.size())
				edit_model->play_animation(animation_names.size(), edit_model->get_animation_loop(), 0.0f);
		}
	}
	ImGui::End();
}

#endif	//	defined(USE_IMGUI)
