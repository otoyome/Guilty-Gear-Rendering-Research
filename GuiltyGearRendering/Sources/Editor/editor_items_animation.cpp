#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>

namespace
{
	//	アニメーション補間編集
	class animation_interpolation_editor : public ImCurveEdit::Delegate, public editor_items::ibase_editor
	{
	public:
		animation_interpolation_editor()
		: ImCurveEdit::Delegate()
		, editor_items::ibase_editor()
		{
			visible = true;
			range_max = ImVec2(0.0f, 1.0f);
			range_min = ImVec2(0.0f, 0.0f);
			handle_lists[0].resize(10000);
			handle_lists[1].resize(10000);
		}
		~animation_interpolation_editor() override
		{
			selection_lists.clear();
			handle_lists[0].clear();
			handle_lists[1].clear();
			point_list.clear();
		}

		void set_data(animation::data* data)
		{
			animation_data = data;

			point_list.clear();
			if (animation_data == nullptr)
				return;

			range_max = ImVec2(0.0f, 1.0f);
			range_min = ImVec2(0.0f, 0.0f);

			for (auto& d : animation_data->time_curve.datas)
			{
				point_list.emplace_back(d.x, d.y);
			}
		}

		void render(std::shared_ptr<editor_items::context>& context) override
		{
			if (animation_data == nullptr)
				return;

			ImVec2 window_size = ImGui::GetWindowSize();

			//	補間モード設定
			{
				std::vector<const char*> texts;
				for (auto& name : magic_enum::enum_names<animation::interpolation::types>())
				{
					texts.emplace_back(name.data());
				}
				int type = static_cast<int>(animation_data->time_curve.type);
				if (ImGui::Combo("Curve Type", &type, texts.data(), static_cast<int>(texts.size())))
				{
					animation_data->time_curve.type = static_cast<animation::interpolation::types>(type);
				}
			}

			//	リセットボタン
			ImGui::SameLine();
			if (ImGui::Button("Reset##curve_reset"))
			{
				animation_data->time_curve.datas.clear();
				animation_data->time_curve.datas.emplace_back(0.0f, 0.0f);
				animation_data->time_curve.datas.emplace_back(1.0f, 1.0f);
			}

			//	カーブ編集
			ImVector<ImCurveEdit::EditPoint> selected_points;
			//DirectX::XMFLOAT4 range = { range_min.x, range_min.y, range_max.x, range_max.y };
			//ImGui::SliderFloat4(u8"Time Range", &range.x, 0.0f, 1.0f);
			int changed = ImCurveEdit::Edit(*this, ImVec2(window_size.x - 40, 300), 0xFFFF, nullptr, &selected_points, &selection_lists);
			if (!selected_points.empty())
			{
				//	とりあえず右クリックしてたら破棄
				if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
				{
					//	ポイントリストから破棄
					auto& time_curve_data = animation_data->time_curve.datas;
					for (auto& selected_point : selected_points)
					{
						auto it = time_curve_data.begin() + selected_point.pointIndex;
						if (it != time_curve_data.begin() && it != (time_curve_data.end() - 1))
						{
							animation_data->time_curve.datas.erase(it);
						}
					}

					//	選択リストを破棄
					selection_lists.clear();
				}
			}

			//	クランプ
			for (auto& data : animation_data->time_curve.datas)
			{
				data = math::saturate(data);
			}
			ImGui::Text(u8"    アニメーション時間");
		}

	private:
		size_t GetCurveCount() override { return 1; }

		bool IsVisible(size_t curve_index) override
		{
			return visible;
		}
		size_t GetPointCount(size_t curve_index) override
		{
			return point_list.size();
		}

		uint32_t GetCurveColor(size_t curve_index) override
		{
			uint32_t cols[] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000 };
			return cols[curve_index];
		}

		ImVec2* GetPoints(size_t curve_index) override
		{
			return point_list.data();
		}

		ImCurveEdit::CurveType GetCurveType(size_t curve_index) const override
		{
			if (animation_data == nullptr)
				return ImCurveEdit::CurveSmooth;

			return static_cast<ImCurveEdit::CurveType>(animation_data->time_curve.type);
		}

		int EditPoint(size_t curve_index, int point_index, ImVec2 value) override
		{
			point_list[point_index] = ImVec2(value.x, value.y);
			set_values(curve_index);
			for (size_t index = 0; index < GetPointCount(curve_index); ++index)
			{
				if (GetPoints(curve_index)[index].x == value.x)
					return static_cast<int>(index);
			}
			return point_index;
		}

		void AddPoint(size_t curve_index, ImVec2 value) override
		{
			point_list.emplace_back(value);
			set_values(curve_index);
		}

		ImVec2& GetMax() override
		{
			return range_max;
		}

		ImVec2& GetMin() override
		{
			return range_min;
		}

		UINT GetBackgroundColor() override
		{
			return 0x00000000;
		}

		void set_values(size_t curve_index)
		{
			std::sort(point_list.begin(), point_list.end(), [](ImVec2 a, ImVec2 b) { return a.x < b.x; });
			if (animation_data == nullptr)
				return;

			animation_data->time_curve.datas.clear();
			for (auto& d : point_list)
			{
				animation_data->time_curve.datas.emplace_back(d.x, d.y);
			}
		}

	private:
		animation::data* animation_data;
		std::set<ImCurveEdit::EditPoint> selection_lists;
		std::vector<ImCurveEdit::EditPoint> handle_lists[2];
		std::vector<ImVec2> point_list;
		ImVec2 range_min, range_max;
		bool visible;

	};

	//	アニメーションタイムライン編集
	//	ﾃｷﾄｰ('ω')-ﾄｷﾃ
	class animation_timeline_editor : public editor_items::ibase_editor
	{
		enum class timeline_category
		{
			none,
			event,
			node,
		};
		struct selected_data
		{
			timeline_category category;
			UINT indices[2];
		};

		static constexpr std::string_view SequencerName = "Sequencer";

		static constexpr UINT skip_display_count = 10;

	private:
		//	時間からフレーム番号算出
		int calcurate_frame_index(float second) const
		{
			return static_cast<int>(system_units::convert_sec_to_fps(second * animation_frame_scaler));
		}
		//	フレーム番号から時間算出
		float calcurate_second(float frame_index) const
		{
			return system_units::convert_fps_to_sec(frame_index) / animation_frame_scaler;
		}

	public:
		animation_timeline_editor()
		: editor_items::ibase_editor()
		{
		}

		~animation_timeline_editor() override
		{
		}

		void clear_selected_selection_data()
		{
			selected.clear();
			ImGui::NeoClearSelection(sequencerID);
		}

		void clear_selected_timeline_data()
		{
			selected_timeline = timeline_category::none;
			ImGui::SetSelectedTimeline(sequencerID, "");
		}

		void set_data(animation::data* data)
		{
			if (animation_data == data)
				return;

			animation_data = data;
			if (!animation_data)
				return;

			current_frame = 0;
			start_frame = first_frame = 0;
			end_frame = last_frame = calcurate_frame_index(animation_data->end_time);
			clear_selected_selection_data();
			clear_selected_timeline_data();
		}

		void render(std::shared_ptr<editor_items::context>& context)
		{
			if (!animation_data)
				return;

			start_frame = std::max(start_frame, first_frame);
			end_frame = std::min(end_frame, last_frame);

			auto content_region_avail = ImGui::GetContentRegionAvail();
			float region_avail_width = content_region_avail.x * 2.0f / 3.0f;
			if (ImGui::BeginChild("##Sequencer", ImVec2(region_avail_width, 0), 0, ImGuiWindowFlags_NoMove))
			{
				if (ImGui::BeginNeoSequencer(SequencerName.data(), &current_frame, &start_frame, &end_frame, { 0, 0 },
					ImGuiNeoSequencerFlags_AllowLengthChanging | ImGuiNeoSequencerFlags_EnableSelection |
					ImGuiNeoSequencerFlags_AlwaysShowHeader | ImGuiNeoSequencerFlags_Selection_EnableDragging |
					ImGuiNeoSequencerFlags_Selection_EnableDeletion))
				{
					//	シーケンサーのメモリリーク修正のため
					//	@remainder	選択中にモーションを変えてしまうとリークする（時間がある時に修正）
					{
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						sequencerID = window->IDStack[window->IDStack.size() - 1];
					}
					selected.clear();
					render_events(context);
					//render_nodes(context);
				}

				ImGui::EndNeoSequencer();
			}
			ImGui::EndChild();
			ImGui::SameLine();
			region_avail_width = content_region_avail.x / 3.0f;
			if (ImGui::BeginChild("##ValueItem", ImVec2(region_avail_width, 0), 0, ImGuiWindowFlags_NoMove))
			{
				render_inspector(context);
			}
			ImGui::EndChild();
		}
		void render_events(std::shared_ptr<editor_items::context>& context)
		{
			ImGui::PushID("##Events");
			if (ImGui::BeginNeoGroup(""))
			{
				if (ImGui::BeginNeoTimelineEx("Events"))
				{
					UINT node_index = 0;
					for (auto& event : animation_data->events)
					{
						ImGui::PushID(context->get_item_id());
						int frame = calcurate_frame_index(event.time);
						ImGui::NeoKeyframe(&frame);
						if (ImGui::IsNeoKeyframeSelected())
						{
							if (selected.empty())
								clear_selected_timeline_data();
							selected.push_back({ timeline_category::event, node_index, 0 });
						}
						ImGui::PopID();
						++node_index;
					}
					//	このタイムラインの選択中か判定
					if (ImGui::IsNeoTimelineSelected())
					{
						clear_selected_selection_data();
						selected_timeline = timeline_category::event;
					}
					ImGui::EndNeoTimeLine();
				}
				ImGui::EndNeoGroup();
			}
			ImGui::PopID();
		}
		void render_nodes(std::shared_ptr<editor_items::context>& context)
		{
			ImGui::PushID("##Nodes");
			if (ImGui::BeginNeoGroup(""))
			{
				UINT node_index = 0;
				for (auto& node : animation_data->nodes)
				{
					auto text = system_utility::convert_shift_jis_to_utf8(node.node_name);
					if (ImGui::BeginNeoTimelineEx(text.data()))
					{
						UINT time_index = 0;
						for (auto&& node : node.nodes)
						{
							ImGui::PushID(context->get_item_id());

							int frame = calcurate_frame_index(node.time);
							ImGui::NeoKeyframe(&frame);

							if (ImGui::IsNeoKeyframeSelected())
							{
								if (selected.empty())
									clear_selected_timeline_data();
								selected.push_back({ timeline_category::node, node_index, time_index });
							}
							ImGui::PopID();
							++time_index;
						}
						//	このタイムラインの選択中か判定
						if (ImGui::IsNeoTimelineSelected())
						{
							clear_selected_selection_data();
							selected_timeline = timeline_category::node;
						}
						ImGui::EndNeoTimeLine();
					}
					++node_index;
				}
				ImGui::EndNeoGroup();
			}
			ImGui::PopID();
		}

		void render_inspector(std::shared_ptr<editor_items::context>& context)
		{
			//	Event編集用関数
			auto node_edit = [&](animation::node& node)
			{
				ImGui::DragFloat("time", &node.time,
					system_units::convert_fps_to_sec(1.0f),
					static_cast<float>(first_frame),
					static_cast<float>(last_frame),
					"%.6f");
				ImGui::DragFloat3("scale", &node.scale.x, 0.01f);
				math::rotator rotator = node.rotation;
				ImGui::DragFloat3("rotation", &rotator.x, 0.01f);
				node.rotation = rotator;
				ImGui::DragFloat3("translation", &node.translation.x, 0.01f);
			};
			auto event_edit = [&](animation::event& event)
			{
				ImGui::DragFloat("time", &event.time,
					system_units::convert_fps_to_sec(1.0f),
					static_cast<float>(first_frame),
					static_cast<float>(last_frame),
					"%.6f");

				ImGui::InputText("Function Name", event.function_name);
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<animation::event::variable_type>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int index = static_cast<int>(event.type);
				ImGui::Combo("variable_type", &index, combo_texts.data(), static_cast<int>(combo_texts.size()));
				event.type = static_cast<animation::event::variable_type>(index);
				switch (event.type)
				{
					case ::animation::event::variable_type::String:
					{
						ImGui::InputText("Value", event.variable);
						break;
					}
					case ::animation::event::variable_type::Float:
					{
						float value;
						event.get_variable<float>(value);
						ImGui::DragFloat("Value", &value);
						event.set_variable<float>(value);
						break;
					}
					case ::animation::event::variable_type::Int:
					{
						int value;
						event.get_variable<int>(value);
						ImGui::DragInt("Value", &value);
						event.set_variable<int>(value);
						break;
					}
				}
			};

			auto content_region_avail = ImGui::GetContentRegionAvail();
			float region_avail_hegiht = content_region_avail.y * 1.0f / 6.0f;
			if (ImGui::BeginChild("##Category Item", ImVec2(0, region_avail_hegiht), 0, ImGuiWindowFlags_NoMove))
			{
				switch (selected_timeline)
				{
					case timeline_category::node:
					{
						//	特になし
						break;
					}
					case timeline_category::event:
					{
						//	ダミー情報設定
						dummy_event.time = calcurate_second(current_frame);
						event_edit(dummy_event);
						current_frame = calcurate_frame_index(dummy_event.time);
						if (ImGui::Button("Add Event"))
						{
							animation_data->events.push_back(dummy_event);
							std::sort(
								animation_data->events.begin(),
								animation_data->events.end(),
								[](const animation::event& l, const animation::event& r)
								{
									return l.time < r.time;
								});
						}
						break;
					}
					case timeline_category::none:
					{
						if (selected.empty())
							break;

						//	何かしらあれば AllDelete コマンドを表示する
						if (ImGui::Button("Selected All Delete"))
						{
							//	データ破棄
							for (auto& data : selected)
							{
								if (data.category == timeline_category::event)
								{
									auto it = animation_data->events.begin() + data.indices[0];
									//	後方の物をずらす
									for (auto& data2 : selected)
									{
										if (data2.category != timeline_category::event)
											continue;

										if (data.indices[0] < data2.indices[0])
											--data2.indices[0];
									}
									animation_data->events.erase(it);
								}
								else if (data.category == timeline_category::node)
								{
									auto& nodes = animation_data->nodes[data.indices[0]];
									auto it = nodes.nodes.begin() + data.indices[1];
									//	後方の物をずらす
									for (auto& data2 : selected)
									{
										if (data2.category != timeline_category::node ||
											data.indices[0] != data2.indices[0])
											continue;

										if (data.indices[1] < data2.indices[1])
											--data2.indices[1];
									}
									nodes.nodes.erase(it);
								}
							}

							//	各種ソート
							std::sort(
								animation_data->events.begin(),
								animation_data->events.end(),
								[](const animation::event& l, const animation::event& r)
								{
									return l.time < r.time;
								});
							for (auto& node : animation_data->nodes)
							{
								std::sort(
									node.nodes.begin(),
									node.nodes.end(),
									[](const animation::node& l, const animation::node& r)
									{
										return l.time < r.time;
									});
							}

							//	選択解除
							clear_selected_selection_data();
							clear_selected_timeline_data();
						}
						break;
					}
				}
			}
			ImGui::EndChild();
			ImGui::Separator();
			region_avail_hegiht = content_region_avail.y * 5.0f / 6.0f;
			if (ImGui::BeginChild("##Selected Edit", ImVec2(0, region_avail_hegiht), 0, ImGuiWindowFlags_NoMove))
			{
				int data_index = 0, delete_data_index = -1;
				for (auto& data : selected)
				{
					ImGui::PushID(context->get_item_id());
					switch (data.category)
					{
						case timeline_category::event:
						{
							ImGui::Text("Event[%d]", data.indices[0]);
							ImGui::SameLine();
							if (ImGui::Button("Delete"))
							{
								delete_data_index = data_index;
							}
							event_edit(animation_data->events[data.indices[0]]);
							break;
						}
						case timeline_category::node:
						{
							auto& node_data = animation_data->nodes[data.indices[0]];
							ImGui::Text("%s[%d]", node_data.node_name.c_str(), data.indices[1]);
							ImGui::SameLine();
							if (ImGui::Button("Delete"))
							{
								delete_data_index = data_index;
							}
							node_edit(node_data.nodes[data.indices[1]]);
							break;
						}
					}
					ImGui::PopID();
					ImGui::Separator();
					++data_index;
				}
				if (delete_data_index >= 0)
				{
					auto& data = selected[delete_data_index];
					switch (data.category)
					{
						case timeline_category::event:
						{
							auto it = animation_data->events.begin() + data.indices[0];
							//	後方の物をずらす
							for (auto& data2 : selected)
							{
								if (data2.category != timeline_category::event)
									continue;

								if (data.indices[0] < data2.indices[0])
									--data2.indices[0];
							}
							animation_data->events.erase(it);
							break;
						}
						case timeline_category::node:
						{
							delete_data_index = data_index;
							auto& nodes = animation_data->nodes[data.indices[0]];
							auto it = nodes.nodes.begin() + data.indices[1];
							//	後方の物をずらす
							for (auto& data2 : selected)
							{
								if (data2.category != timeline_category::node ||
									data.indices[0] != data2.indices[0])
									continue;

								if (data.indices[1] < data2.indices[1])
									--data2.indices[1];
							}
							nodes.nodes.erase(it);
							break;
						}
					}
					//	各種ソート
					std::sort(
						animation_data->events.begin(),
						animation_data->events.end(),
						[](const animation::event& l, const animation::event& r)
						{
							return l.time < r.time;
						});
					for (auto& node : animation_data->nodes)
					{
						std::sort(
							node.nodes.begin(),
							node.nodes.end(),
							[](const animation::node& l, const animation::node& r)
							{
								return l.time < r.time;
							});
					}
				}
			}
			ImGui::EndChild();
		}

	private:
		ImGuiID sequencerID;
		timeline_category selected_timeline;
		animation::data* animation_data;
		float animation_frame_scaler = 10.0f;
		int32_t current_frame = 0;
		int32_t start_frame, end_frame;
		int32_t first_frame, last_frame;
		std::vector<selected_data> selected;

		animation::event dummy_event;
		animation::node dummy_node;
	};
}

bool editor_items::edit_animation_data(std::shared_ptr<context>& context, animation::data& animation_data)
{
	auto data_hash_key = system_utility::fnv1a_hash(sizeof(animation_data), reinterpret_cast<char*>(&animation_data));
	ImGui::InputText("name", animation_data.name);
	ImGui::Text("root_motion");
	ImGui::SameLine();
	ImGui::Checkbox("xz", &animation_data.root_motion_xz);
	ImGui::SameLine();
	ImGui::Checkbox("y", &animation_data.root_motion_y);
	ImGui::SameLine();
	ImGui::Checkbox("rotation", &animation_data.root_rotation);
	if (animation_data.root_motion_xz || animation_data.root_motion_y || animation_data.root_rotation)
	{
		char root_motion_node_name[256] = {};
		memcpy_s(root_motion_node_name, sizeof(root_motion_node_name),
			animation_data.root_motion_node_name.data(), animation_data.root_motion_node_name.size());
		if (ImGui::InputText("root_motion_node_name", root_motion_node_name, sizeof(root_motion_node_name)))
			animation_data.root_motion_node_name = root_motion_node_name;
	}
	ImGui::Separator();

	//	アニメーションカーブエディター
	//	フレーム間のカーブは取り扱っていない
	if (ImGui::TreeNode("Curve Editor"))
	{
		if (!context->interpolation_editor)
			context->interpolation_editor = std::make_unique<animation_interpolation_editor>();

		animation_interpolation_editor* editor = static_cast<animation_interpolation_editor*>(context->interpolation_editor.get());
		editor->set_data(&animation_data);
		editor->render(context);

		ImGui::TreePop();
	}
	ImGui::Separator();

	//	タイムラインエディター
	{
		if (!context->timeline_editor)
			context->timeline_editor = std::make_unique<animation_timeline_editor>();

		animation_timeline_editor* editor = static_cast<animation_timeline_editor*>(context->timeline_editor.get());
		editor->set_data(&animation_data);
		editor->render(context);
	}

	//	データハッシュキー更新
	context->animation_data_hash_key = data_hash_key;
	return true;
}

#endif	//	defined(USE_IMGUI)


