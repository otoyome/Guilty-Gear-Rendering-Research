#pragma once

#include "scenes/scene_base.h"

#include "model_instance.h"

#include "object/model_object.h"
#include "object/camera_object.h"
#include "object/post_process_volume_object.h"

#if defined(USE_IMGUI)
#include "data_converter.h"

#include "Editor/editor_items.h"

class edit_model_scene : public scene_base
{
	SCENE_DECRLEAR(edit_model_scene, scene_base);

public:
	edit_model_scene();
	edit_model_scene(const edit_model_scene&) = delete;
	edit_model_scene(edit_model_scene&&) = delete;
	~edit_model_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;

private:
	void load_model(std::string_view file_path);
	void save_model();

private:	//	編集ウィンドウ纏め系
	bool initialize_edit_core();
	bool uninitialize_edit_core();
	void update_edit_core(float elapsed_time);
	void imgui_render_edit_core();
	void imgui_model_loader_window();
	void imgui_render_preset_light();
	void imgui_render_mesh_list();
	void imgui_render_material_window();

private:	//	頂点カラー編集系
	bool initialize_vertex_color_edit();
	bool uninitialize_vertex_color_edit();
	void update_vertex_color_edit();
	void imgui_render_vertex_color_edit();
	void on_mouse_left_down_and_hold(void* data_ptr);
	void on_mouse_left_release(void* data_ptr);

private:	//	アニメーション編集系
	bool initialize_animation();
	bool uninitialize_animation();
	void update_animation();
	void imgui_render_animation();

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<post_process_volume_object> post_process_volume;

private:
	std::shared_ptr<model_object> edit_model;
	std::shared_ptr<model_object> next_edit_model;
	std::string load_model_path;
	std::shared_ptr<editor_items::context> editor_context;

private:	//	ボーン表示系
	bool display_bone = false;
	std::vector<std::string> selected_bones;

private:
	//	プリセットライト情報
	struct preset_lights
	{
		directional_lights direction;
		DirectX::XMFLOAT4 ambient_color;
	};
	std::vector<preset_lights> preset_light_list;
	int preset_light_index;

private:
	//	リターゲッティング情報
	retarget_profile_data retarget_data;
	std::size_t selected_retarget_data_index = 0;

private:	//	頂点カラー編集系
	bool is_edit_vertex_color = false;
	std::shared_ptr<model_object> vertex_color_edit_model;
	std::shared_ptr<material> vertex_color_edit_material;
	std::vector<material> reserve_materials;

	//	メッシュ情報リスト
	struct edit_model_mesh_data
	{
		size_t node_index;
		bool visible = true;
		std::vector<model_instance::mesh*> meshes;
	};
	std::vector<edit_model_mesh_data> edit_model_meshes;

	//	ブラシ情報
	struct brush_data
	{
		bool on = false;							//	処理
		DirectX::XMFLOAT3 position;					//	座標
		float radius = 0.1f;						//	範囲
		DirectX::XMFLOAT4 color{1, 1, 1, 0.5f};		//	色
		DirectX::XMINT4 color_flags{1, 1, 1, 1};	//	色適応フラグ
	};
	brush_data brush_data;

	//	左クリック
	uint64_t left_click_data_identifier[3] = { messenger::invalid_key, messenger::invalid_key, messenger::invalid_key };
};
#endif // defined(USE_IMGUI)
