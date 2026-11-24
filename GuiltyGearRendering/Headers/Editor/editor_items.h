#pragma once

#if defined(USE_IMGUI)
#include "material.h"
#include "animation_resource.h"
#include "model_instance.h"

#include "ImGuizmo/ImCurveEdit.h"
#include "ImGuizmo/ImSequencer.h"
#include "im-neo-sequencer/imgui_neo_sequencer.h"

//	エディターアイテム関係
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
class editor_items
{
public:
	//	取り扱いテクスチャの読み込み用
	static constexpr TCHAR* texture_file_dialog_filter =
		L"テクスチャ (*.bmp, *.png, *.jpg, *.tga, *.dds, *.hdr)\0*.bmp;*.png;*.jpg;*.tga;*.dds;*.hdr\0\
		すべてのファイル (*.*)\0*.*\0";

	//	アイコンの基本サイズ
	static constexpr ImVec2 icon_size_srv = { 32, 32 };

public:
	//	エディター用コンテキスト
	class ibase_editor;
	struct context
	{
		friend class editor_items;

		int get_item_id() {return inner_item_id++;}

	private:
		int inner_item_id; // 内部でItemIdで使っているだけの白物
		int reset_item_id; // 内部でItemIdで使っているだけの白物
		texture_ptr blank_texture; // 空テクスチャ
		texture_ptr cross_texture; // ×テクスチャ
		texture_ptr reserve_texture; // 内部で一時的に保持しておきたいテクスチャ

	private:	//	アニメーション関係
		system_utility::hash_key animation_data_hash_key = system_utility::hash_key(-1);
		std::unique_ptr<ibase_editor> interpolation_editor;
		std::unique_ptr<ibase_editor> timeline_editor;
	};

	//	内部で利用するエディターアイテムのインターフェース
	class ibase_editor
	{
	public:
		ibase_editor() {}
		virtual ~ibase_editor() {}

		virtual void render(std::shared_ptr<context>& context) = 0;
	};

public:
	static void initialize();
	static void terminate();

	static std::shared_ptr<context> generate_context();
	static void begin_contexts();

public:	//	表示系
	static void render_model_bones(const model_instance& model_instance, const std::vector<std::string_view>& selected_bones);
	static void render_model_bones(const model_instance& model_instance, const std::vector<std::string>& selected_bones);
	static void render_model_bones(const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size);

public:	//	編集アイテム系
	static bool edit_material(std::shared_ptr<context>& context, material& material);
	static bool edit_texture_item(std::shared_ptr<context>& context, texture_ptr& resource, bool* selected_file = nullptr);
	static bool edit_animation_data(std::shared_ptr<context>& context, animation::data& animation_data);

public:	//	汎用
	static bool open_file_dialog(TCHAR* text, std::string& out_selected_file_path);
	static bool copy_to_clipboard(const char* text);

private:
	inline static std::unique_ptr<editor_items> instance;
	std::vector<std::weak_ptr<context>> context_ptrs;
};

#endif	//	defined(USE_IMGUI)
