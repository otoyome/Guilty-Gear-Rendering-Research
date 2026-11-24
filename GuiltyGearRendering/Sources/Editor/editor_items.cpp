#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>

namespace
{
	constexpr std::string_view domain_texts[] =
	{
		"surface",			//	モデル
		"deferred_decal",	//	遅延デカール
		"postprocess",		//	ポストプロセス
	};

	constexpr std::string_view blend_mode_texts[] =
	{
		"opaque",		//	不透明描画
		"mask",			//	不透明描画+アルファマスク
		"translucent",	//	半透明描画
	//	"addative"		//	加算合成
	};

	bool bool_item(std::string_view name, material::constant_buffer::subset subset, bool* pbool)
	{
		if (subset.columns == 1)
		{
			return ImGui::Checkbox(name.data(), pbool);
		}
		const char element_text[][2] =
		{
			"x",
			"y",
			"z",
			"w",
		};
		bool change_flag = false;
		if (ImGui::TreeNode(name.data()))
		{
			for (UINT col = 0; col < subset.columns; ++col)
			{
				ImGui::PushID(static_cast<int>(reinterpret_cast<std::size_t>(pbool + col)));
				if (ImGui::Checkbox(element_text[col], &pbool[col]))
					change_flag = true;
				if (col + 1 == subset.columns)
					ImGui::SameLine();
				ImGui::PopID();
			}

			ImGui::TreePop();
		}
		return change_flag;
	}
	bool int_item(std::string_view name, material::constant_buffer::subset subset, int* pint)
	{
		int limit_min = subset.editor_info.int_range.min;
		int limit_max = subset.editor_info.int_range.max;
		bool change_flag = false;
		switch (subset.columns)
		{
			case 4:
			{
				if (ImGui::DragInt4(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 3:
			{
				if (ImGui::DragInt3(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 2:
			{
				if (ImGui::DragInt2(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			default:
			{
				if (ImGui::DragInt(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
		}
		return change_flag;
	}
	bool uint_item(std::string_view name, material::constant_buffer::subset subset, UINT* puint)
	{
		int limit_min = std::max(subset.editor_info.uint_range.min, 0);
		int limit_max = subset.editor_info.uint_range.max;
		bool change_flag = false;
		switch (subset.columns)
		{
			case 4:
			{
				int pint[] =
				{
					static_cast<int>(puint[0]),
					static_cast<int>(puint[1]),
					static_cast<int>(puint[2]),
					static_cast<int>(puint[3])
				};
				if (ImGui::DragInt4(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					puint[1] = pint[1];
					puint[2] = pint[2];
					puint[3] = pint[3];
					change_flag = true;
				}
				break;
			}
			case 3:
			{
				int pint[] =
				{
					static_cast<int>(puint[0]),
					static_cast<int>(puint[1]),
					static_cast<int>(puint[2])
				};
				if (ImGui::DragInt3(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					puint[1] = pint[1];
					puint[2] = pint[2];
					change_flag = true;
				}
				break;
			}
			case 2:
			{
				int pint[] =
				{
					static_cast<int>(puint[0]),
					static_cast<int>(puint[1])
				};
				if (ImGui::DragInt2(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					puint[1] = pint[1];
					change_flag = true;
				}
				break;
			}
			default:
			{
				int pint[] =
				{
					static_cast<int>(puint[0])
				};
				if (ImGui::DragInt(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					change_flag = true;
				}
				break;
			}
		}
		return change_flag;
	}
	bool float_item(std::string_view name, material::constant_buffer::subset subset, float* pfloat)
	{
		float limit_min = subset.editor_info.float_range.min;
		float limit_max = subset.editor_info.float_range.max;
		bool change_flag = false;
		switch (subset.columns)
		{
			case 4:
			{
				if (ImGui::DragFloat4(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 3:
			{
				if (ImGui::DragFloat3(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 2:
			{
				if (ImGui::DragFloat2(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			default:
			{
				if (ImGui::DragFloat(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
		}
		return change_flag;
	}
	bool add_item_function(material::constant_buffer& constant_buffer, std::string_view name, material::constant_buffer::subset subset)
	{
		unsigned char* buffer_top = constant_buffer.data_buffer.data() + subset.start_offset;
		bool edit_flag = false;
		switch (subset.kind)
		{
			case material::variable_kind::MatrixRows:
			case material::variable_kind::MatrixColumns:
			{
				UINT array_max = subset.size / subset.stride;
				bool is_array = array_max > 1;
				bool tree_node_open = true;
				if (is_array)
					tree_node_open = ImGui::TreeNode(name.data());

				for (UINT array_count = 0; array_count < array_max && tree_node_open; ++array_count)
				{
					for (UINT row = 0; row < subset.rows; ++row)
					{
						std::string key = is_array ? ("[" + std::to_string(array_count) + "]")
										: std::string(name);

						unsigned char* top = buffer_top +
											 pipeline_state_base::get_variable_type_size(subset.kind, subset.type, 1, subset.columns) * row +
											 array_count * subset.stride;
						ImGui::PushID(static_cast<int>(reinterpret_cast<std::size_t>(top)));
						switch (subset.type)
						{
							case material::variable_type::Bool:
							{
								if (bool_item(name, subset, reinterpret_cast<bool*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
							case material::variable_type::Int:
							{
								if (int_item(name, subset, reinterpret_cast<int*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
							case material::variable_type::UInt:
							{
								if (uint_item(name, subset, reinterpret_cast<UINT*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
							case material::variable_type::Float:
							{
								if (float_item(name, subset, reinterpret_cast<float*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
						}
						ImGui::PopID();
					}
				}

				if (is_array && tree_node_open)
					ImGui::TreePop();

				break;
			}
			case material::variable_kind::Vector:
			case material::variable_kind::Scalar:
			{
				UINT array_max = subset.size / subset.stride;
				bool is_array = array_max > 1;
				bool tree_node_open = true;
				if (is_array)
					tree_node_open = ImGui::TreeNode(name.data());

				for (UINT array_count = 0; array_count < array_max && tree_node_open; ++array_count)
				{
					std::string key = is_array ? ("[" + std::to_string(array_count) + "]")
									: std::string(name);

					unsigned char* top = buffer_top + array_count * subset.stride;
					switch (subset.type)
					{
						case material::variable_type::Bool:
						{
							if (bool_item(key, subset, reinterpret_cast<bool*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
						case material::variable_type::Int:
						{
							if (int_item(key, subset, reinterpret_cast<int*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
						case material::variable_type::UInt:
						{
							if (uint_item(key, subset, reinterpret_cast<UINT*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
						case material::variable_type::Float:
						{
							if (float_item(key, subset, reinterpret_cast<float*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
					}
				}

				if (is_array && tree_node_open)
					ImGui::TreePop();

				break;
			}
		}
		return edit_flag;
	};
}

void editor_items::initialize()
{
	instance = std::make_unique<editor_items>();
}

void editor_items::terminate()
{
	instance.reset();
}

std::shared_ptr<editor_items::context> editor_items::generate_context()
{
	std::random_device rd{};
	auto context_ptr = std::make_shared<context>();
	context_ptr->reset_item_id = rd();
	context_ptr->inner_item_id = context_ptr->reset_item_id;
	context_ptr->blank_texture = texture_manager::instance()->search(texture_manager::black_texture2d_name);
	texture_manager::instance()->load_texture_from_file(context_ptr->cross_texture, "Assets/Resources/Textures/editor/cross.png");
	instance->context_ptrs.emplace_back(context_ptr);
	return context_ptr;
}

void editor_items::begin_contexts()
{
	for (auto& context : instance->context_ptrs)
	{
		auto context_ptr = context.lock();
		if (context_ptr)
			context_ptr->inner_item_id = context_ptr->reset_item_id;
	}
}

bool editor_items::edit_material(std::shared_ptr<context>& context, material& material)
{
	bool edit_flag = false;
	float alpha_clip_threshold = material.get_alpha_clip_threshold();
	ImGui::PushID(context->get_item_id());

	//	ドメイン
	{
		std::vector<const char*> combo_texts;
		for (auto& name : domain_texts)
		{
			combo_texts.emplace_back(name.data());
		}
		int domain = static_cast<int>(material.get_domain());
		if (ImGui::Combo("domain", &domain, combo_texts.data(), static_cast<int>(combo_texts.size())))
		{
			material.set_domain(static_cast<material::domains>(domain));
		}
	}

	//	ドメインごとの編集項目
	switch (material.get_domain())
	{
		case material::domains::surface:
		{
			//	適応タイミング毎のシェーダーの設定
			if (ImGui::TreeNode("shaders"))
			{
				auto pso_list = pipeline_state_manager::instance()->get_domain_lists(material::domains::surface);
				std::vector<const char*> combo_texts;
				combo_texts.reserve(pso_list.size());
				combo_texts.emplace_back("[BLANK]");
				for (auto& pso : pso_list)
				{
					combo_texts.emplace_back(pso->get_name().data());
				}

				for (auto& name : magic_enum::enum_names<material::surface_paths>())
				{
					auto enum_value = magic_enum::enum_cast<material::surface_paths>(name);
					if (!enum_value)
					{
						assert(!L"!enum_value");
						continue;
					}

					UINT enum_index = static_cast<UINT>(enum_value.value());

					//	選択番号取得
					int selected_index = 0;
					auto selected_pso = material.get_pipeline_state_name(enum_value.value());
					for (auto& pso : pso_list)
					{
						if (pso->get_name() == selected_pso)
							break;

						++selected_index;
					}
					//	選択できなかった場合は解除する
					if (selected_index >= pso_list.size())
					{
						selected_index = 0;
					}
					else
					{
						++selected_index;	//	BLANK分ずらす
					}
					if (ImGui::Combo(name.data(), &selected_index, combo_texts.data(), static_cast<int>(combo_texts.size())))
					{
						material.set_pipeline_state(enum_value.value(), combo_texts[selected_index]);
						edit_flag = true;
					}
				}
				ImGui::TreePop();
			}

			ImGui::Separator();

			//	シェーディングモデル設定
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material::shading_models>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int blend_mode = static_cast<int>(material.get_shading_model());
				if (ImGui::Combo("shading_model", &blend_mode, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_shading_model(static_cast<material::shading_models>(blend_mode));
					edit_flag = true;
				}
			}

			//	ブレンドモード設定
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material::blend_modes>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int value = static_cast<int>(material.get_blend_mode());
				if (ImGui::Combo("blend_mode", &value, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_blend_mode(static_cast<material::blend_modes>(value));
					edit_flag = true;
				}
			}

			//	ブレンドモードごとの対応
			if (material.get_blend_mode() == material::blend_modes::translucent ||
				material.get_blend_mode() == material::blend_modes::addative)
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material::translucency_paths>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int value = static_cast<int>(material.get_translucency_path());
				if (ImGui::Combo("translucency_path", &value, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_translucency_path(static_cast<material::translucency_paths>(value));
					edit_flag = true;
				}
			}

			//	αクリップ値
			if (ImGui::SliderFloat("alpha_clip_threshold", &alpha_clip_threshold, 0.0f, 1.0f))
			{
				material.set_alpha_clip_threshold(alpha_clip_threshold);
				edit_flag = true;
			}

			ImGui::Separator();

			//	物理マテリアル
			if (ImGui::TreeNode("physical material"))
			{
				material::physical_datas physical_data = material.get_physical_data();

				if (ImGui::DragFloat("friction", &physical_data.friction, 1.0f, 0.0f, 1000.0f))
				{
					edit_flag = true;
				}

				material.set_physical_data(physical_data);
				ImGui::TreePop();
			}

			ImGui::Separator();

			//	マテリアルフラグ
			if (ImGui::TreeNode("flags"))
			{
				for (auto data : magic_enum::enum_values<material::flags>())
				{
					bool flag = material.is_flag(data);
					if (ImGui::Checkbox(magic_enum::enum_name(data).data(), &flag))
					{
						material.set_flag(data, flag);
						edit_flag = true;
					}
				}
				ImGui::TreePop();
			}
			break;
		}
		case material::domains::post_process:
		{
			//	適応タイミング毎のシェーダーの設定
			{
				auto pso_list = pipeline_state_manager::instance()->get_domain_lists(material::domains::post_process);
				std::vector<const char*> combo_texts;
				combo_texts.reserve(pso_list.size());
				combo_texts.emplace_back("[BLANK]");
				for (auto& pso : pso_list)
				{
					combo_texts.emplace_back(pso->get_name().data());
				}

				{
					//	選択番号取得
					int selected_index = 0;
					auto selected_pso = material.get_pipeline_state_name();
					for (auto& pso : pso_list)
					{
						if (pso->get_name() == selected_pso)
							break;

						++selected_index;
					}
					//	選択できなかった場合は解除する
					if (selected_index >= pso_list.size())
					{
						selected_index = 0;
					}
					else
					{
						++selected_index;	//	BLANK分ずらす
					}
					if (ImGui::Combo("pipeline_state", &selected_index, combo_texts.data(), static_cast<int>(combo_texts.size())))
					{
						material.set_pipeline_state(combo_texts[selected_index]);
						edit_flag = true;
					}
				}
			}

			//	ブレンドモード設定
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material::blendable_locations>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int blend_mode = static_cast<int>(material.get_blendable_location());
				if (ImGui::Combo("blendable_locations", &blend_mode, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_blendable_location(static_cast<material::blendable_locations>(blend_mode));
					edit_flag = true;
				}
			}
			break;
		}
	}


	//	パラメーター
	for (auto& it : material.get_constant_buffers_for_editor())
	{
		material::constant_buffer& constant_buffer = it.second;
		if (ImGui::TreeNode(it.first.data()))
		{
			for (auto& identifier : constant_buffer.identifieres)
			{
				auto& subset = constant_buffer.subsets[identifier.second];

				if (add_item_function(constant_buffer, identifier.first, subset))
				{
					edit_flag = true;
				}
			}
			ImGui::TreePop();
		}
	}

	//	リソース
	for (auto& it : material.get_shader_resources_for_editor())
	{
		material::shader_resource& shader_resource = it.second;
		{
			ImGui::Text(it.first.data());
			UINT index = 0;
			for (auto& resource : shader_resource.resources)
			{
				const ID3D11ShaderResourceView* srv = context->blank_texture->get_shader_resource_view().Get();
				std::string text = std::string(context->blank_texture->get_file_path_or_key());
				if (resource.is_valid())
				{
					srv = resource.value->get_shader_resource_view().Get();
					text = resource.value->get_file_path_or_key();
					if (resource.value->is_file_path_or_key())
					{
						text = std::filesystem::path(text).filename().string();
					}
				}
				//	テクスチャ
				{
					ImGui::PushID(context->get_item_id());
					ImGui::Text((std::string("[") + std::to_string(index) + std::string("]")).data());
					ImGui::SameLine();
					bool is_selected_file = false;
					if (edit_texture_item(context, resource.value, &is_selected_file))
					{
						resource.is_file_path_or_key = is_selected_file;
						resource.path = resource.value->get_file_path_or_key();
						edit_flag = true;
					}
					ImGui::PopID();
				}
				++index;
			}
		}
	}

	ImGui::PopID();
	return edit_flag;
}

bool editor_items::edit_texture_item(std::shared_ptr<context>& context, texture_ptr& resource, bool* selected_file)
{
	bool edit_flag = false;
	if (selected_file)
		*selected_file = false;

	//	通常
	const ID3D11ShaderResourceView* srv = (resource ? resource : context->blank_texture)->get_shader_resource_view().Get();
	if (ImGui::ImageButton("", (const ImTextureID)srv, icon_size_srv))
	{
		//	ファイルのオープンダイアログシステム
		bool is_selected = false;
		std::string file_path;
		texture_ptr selected_texture;
		if (open_file_dialog(texture_file_dialog_filter, file_path) &&
			texture_manager::instance()->load_texture_from_file(selected_texture, file_path))
		{
			context->reserve_texture = resource;
			resource = selected_texture;
			if (selected_file)
				*selected_file = true;
			edit_flag = true;
		}
	}
	ImGui::SameLine();
	//	ファイル名
	{
		if (resource)
		{
			ImGui::Text(resource->get_file_path_or_key().data());
		}
		else
		{
			ImGui::Text(context->blank_texture->get_file_path_or_key().data());
		}
	}
	//	空選択
	if (resource && resource->is_file_path_or_key())
	{
		ImGui::PushID(context->get_item_id());
		ImGui::SameLine();
		srv = context->cross_texture->get_shader_resource_view().Get();
		if (ImGui::ImageButton("", (const ImTextureID)srv, icon_size_srv))
		{
			context->reserve_texture = resource;
			resource = context->blank_texture;
			edit_flag = true;
		}
		ImGui::PopID();
	}
	return edit_flag;
}

bool editor_items::open_file_dialog(TCHAR* text, std::string& out_selected_file_path)
{
	OPENFILENAME open_file_name; // 構造体
	TCHAR selected_file_path[MAX_PATH] = { 0 };
		
	ZeroMemory(&open_file_name, sizeof(open_file_name));
	open_file_name.lStructSize = sizeof(open_file_name);
	open_file_name.hwndOwner = GetForegroundWindow();
	open_file_name.lpstrFilter = text;
	open_file_name.lpstrFile = selected_file_path;
	open_file_name.nMaxFile = MAX_PATH;
	open_file_name.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

	bool opened = false;
	if (GetOpenFileName(&open_file_name))
	{
		out_selected_file_path = system_utility::convert_utf16_to_shift_jis(selected_file_path);
		out_selected_file_path = system_utility::convert_to_relative_path(out_selected_file_path);
		opened = true;
	}
	return opened;
}

bool editor_items::copy_to_clipboard(const char* text)
{
	// 文字列の長さを計算しメモリ確保
	size_t len = (strlen(text) + 1) * sizeof(char);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	if (!hMem)
		return false;

	// メモリに文字列を書き込む
	void* pMem = GlobalLock(hMem);
	memcpy(pMem, text, len);
	GlobalUnlock(hMem);

	// クリップボード操作
	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();
		SetClipboardData(CF_TEXT, hMem);
		CloseClipboard();
	}
	else
	{
		GlobalFree(hMem); // エラー時はメモリ解放
		return false;
	}
	return true;
}

#endif	//	defined(USE_IMGUI)


