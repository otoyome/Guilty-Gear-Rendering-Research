#include "framework.h"

#if defined(USE_IMGUI)
#include "System/engine_config_data.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "imPlot/imPlot.h"
#include "ImGuizmo/ImCurveEdit.h"
#include "imtimeline/ImTimeline.h"
#include "im-neo-sequencer/imgui_neo_sequencer.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];

#include "Editor/editor_items.h"

#include "scenes/editscene/edit_model_scene.h"

#include "imPlot/imPlot.h"
#include "imtimeline/ImTimeline.h"

namespace
{
	//	常駐タイマーの記録サイズ
	int stationed_stamp_size = 512;

	template<typename Type>
	void change_scene()
	{
		if (ImGui::Button(Type::type_name().data()))
		{
			scene_manager::instance()->change_scene_for_imgui<Type>();
		}
	}
}

void framework::imgui_initialize()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/Resources/Textures/Fonts/Noto_Sans_JP/NotoSansJP-VariableFont_wght.ttf", 24.0f, nullptr, glyphRangesJapanese);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(render_data::instance()->get_device().Get(), render_data::instance()->get_immediate_context().Get());
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	for (auto& time_stamp_data : stationed_cpu_stamp_datas)
	{
		time_stamp_data.datas.resize(stationed_stamp_size);
	}
	stationed_gpu_stamp_datas.datas.resize(stationed_stamp_size);
}

void framework::imgui_terminate()
{
	ImGui::ShutdownNeoSequencer();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
}

void framework::imgui_engine_config_window()
{
	if (ImGui::Begin("engine_config"))
	{
		//	垂直同期
		ImGui::Checkbox("vertical_sync", &engine_config_data::instance()->windows.vertical_sync);

		//	アンチエイリアス
		if (ImGui::TreeNode("antialias"))
		{
			std::vector<const char*> combo_texts;
			for (auto& name : magic_enum::enum_names<engine_config_data::antialias_data::modes>())
			{
				combo_texts.emplace_back(name.data());
			}
			int mode = static_cast<int>(engine_config_data::instance()->antialias.mode);
			if (ImGui::Combo("mode", &mode, combo_texts.data(), static_cast<int>(combo_texts.size())))
			{
				engine_config_data::instance()->antialias.mode = static_cast<engine_config_data::antialias_data::modes>(mode);
			}
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void framework::imgui_main_window()
{
	if (ImGui::Begin("main"))
	{
		bool pause = object_manager::instance()->get_pause();
		ImGui::Checkbox("pause", &pause);
		object_manager::instance()->set_pause(pause);
		ImGui::SameLine();
		if (ImGui::Button("step"))
		{
			object_manager::instance()->called_step();
		}
	}
	ImGui::End();
}

void framework::imgui_log_window()
{
	if (!logger::is_valid())
		return;

	if (ImGui::Begin("log"))
	{
		{
			auto category_texts = logger::instance()->get_category_texts();
			int category = static_cast<int>(display_log_category);
			if (ImGui::Combo("Category", &category, category_texts.data(), static_cast<INT>(category_texts.size())))
			{
				display_log_category = static_cast<logger::categorys>(category);
			}
			ImGui::SameLine();
			if (ImGui::Button(is_log_descending_order ? u8"降順" : u8"昇順"))
				is_log_descending_order = !is_log_descending_order;
		}
		{
			auto category_texts = logger::instance()->get_levels_texts();
			for (UINT index = 0; index < static_cast<UINT>(logger::levels::max); ++index)
			{
				auto level = magic_enum::enum_value<logger::levels>(index);
				bool b = system_utility::is_bitflag(display_log_flags, 1 << static_cast<UINT>(level));
				ImGui::Checkbox(magic_enum::enum_name(level).data(), &b);
				//if (index + 1 != static_cast<UINT>(logger::levels::max))
				ImGui::SameLine();
				system_utility::set_bitflag(display_log_flags, 1 << static_cast<UINT>(level), b);
			}
			if (ImGui::Button("Log Clear"))
			{
				logger::instance()->clear();
			}
		}
		ImGui::Separator();
		if (ImGui::BeginChild("##Log"))
		{
			auto display_log = [&](const logger::data& data)
			{
				if ((display_log_category != logger::categorys::max && display_log_category != data.category) ||
					!system_utility::is_bitflag(display_log_flags, 1 << static_cast<UINT>(data.level)))
					return;

				ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
				switch (data.level)
				{
					case logger::levels::warning: color = { 1.0f, 1.0f, 0.0f, 1.0f }; break;
					case logger::levels::error: color = { 1.0f, 0.0f, 0.0f, 1.0f }; break;
				}
				ImGui::TextColored(color, data.text.data());
				ImGui::Separator();
			};

			if (is_log_descending_order)
			{
				for (auto& log : logger::instance()->get_logs())
				{
					display_log(log);
				}
			}
			else
			{
				std::list<logger::data>::const_reverse_iterator it = logger::instance()->get_logs().rbegin();
				std::list<logger::data>::const_reverse_iterator it_end = logger::instance()->get_logs().rend();
				while (it != it_end)
				{
					display_log(*it);
					it++;
				}
			}

			ImGui::EndChild();
		}
	}
	ImGui::End();
}

void framework::imgui_render_infos_window()
{
	if (ImGui::Begin("render infos"))
	{
		ImGui::Text("fixed_update_count : %d", object_manager::instance()->get_fixed_update_count());

		bool single_thread_mode = render_executor->is_single_thread_mode();
		ImGui::Checkbox("is_single_thread_rendering", &single_thread_mode);
		render_executor->set_single_thread_mode(single_thread_mode);

		ImGui::Checkbox("xz", &display_grid_xz); ImGui::SameLine();
		ImGui::Checkbox("xy", &display_grid_xy); ImGui::SameLine();
		ImGui::Checkbox("yz", &display_grid_yz);
		ImGui::DragInt("display_grid_length", &display_grid_length, 1.0f, 0, 1000);
	}
	ImGui::End();
}

void framework::imgui_textures_window()
{
	if (ImGui::Begin("textures"))
	{
		auto sorted_textures = texture_manager::instance()->get_textures();
		std::vector<std::shared_ptr<texture>> texture_list;
		for (auto& texture_ptr : sorted_textures)
		{
			auto texture = texture_ptr.second.lock();
			if (!texture)
				continue;

			if (texture->get_dimension() != D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_TEXTURE2D ||
				!texture->get_shader_resource_view())
				continue;

			texture2d* p = static_cast<texture2d*>(texture.get());
			auto desc = p->get_desc();

			if (graphics_utility::is_int_format(desc.Format))
				continue;

			texture_list.emplace_back(texture);
		}

		std::sort(texture_list.begin(), texture_list.end(),
			[](const std::shared_ptr<texture>& l, const std::shared_ptr<texture>& r)
			{
				return l->get_file_path_or_key() < r->get_file_path_or_key();
			});

		for (auto& texture_ptr : texture_list)
		{
			ImGui::Separator();
			ImGui::PushID(texture_ptr->get_file_path_or_key().data());
			ImGui::Text(texture_ptr->get_file_path_or_key().data());
			if (texture_ptr->is_file_path_or_key())
			{
				if (ImGui::Button("Save"))
				{
					texture_manager::instance()->save_texture(texture_ptr);
				}
				bool cube_map = texture_ptr->is_flag(texture::flags::cube_map);
				if (ImGui::Checkbox("cube_map", &cube_map))
				{
					texture_ptr->set_flag(texture::flags::cube_map, cube_map);
				}
				bool srgb = texture_ptr->is_flag(texture::flags::srgb);
				if (ImGui::Checkbox("srgb", &srgb))
				{
					texture_ptr->set_flag(texture::flags::srgb, srgb);
				}
			}
			if (!texture_ptr->is_flag(texture::flags::cube_map))
			{
				ImGui::Image(texture_ptr->get_shader_resource_view().Get(), { 256, 256 });
			}
			else
			{
				ImGui::Text("Not Display Image");
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
}

void framework::imgui_scene_change()
{
	if (ImGui::Begin("Chane Change Window"))
	{
		change_scene<edit_model_scene>();
	}
	ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
	constexpr int timeline_section_data = 100;
}

void framework::imgui_profiler_window(float elapsed_time)
{
	//	常駐タイマー
	if (ImGui::Begin("stationed_profiler"))
	{
		//	表示フラグ
		{
			ImGui::PushID(0xF1274094);
			for (UINT index = 0; index < static_cast<UINT>(timer::max); ++index)
			{
				auto kind = static_cast<timer>(index);
				auto& stamp_data = stationed_cpu_stamp_datas[index];
				ImGui::Checkbox(magic_enum::enum_name(kind).data(), &stamp_data.flag);
				ImGui::SameLine();
			}
			{
				auto& stamp_data = stationed_gpu_stamp_datas;
				ImGui::Checkbox("gpu", &stamp_data.flag);
			}
			ImGui::PopID();
		}
#if defined(_DEBUG) || defined(USE_IMGUI)
		//	FPS表示
		{
			ImGui::SameLine();
			ImGui::Text("FPS : %d ", static_cast<int>(display_frame));
		}
#endif	//	defined(_DEBUG) || defined(USE_IMGUI)

		//	スタンプ表示
		ImPlot::SetNextAxisLimits(ImAxis_X1, 0.0f, stationed_stamp_size * 0.01f, ImGuiCond_Always);
		ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, 16.666f, ImGuiCond_Always);
		if (ImPlot::BeginPlot("profiller", ImVec2(-1, -1)))
		{
			ImPlot::SetupAxis(ImAxis_X1, "frame", ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxis(ImAxis_Y1, "millisecond");
			//	CPU
			for (UINT index = 0; index < static_cast<UINT>(timer::max); ++index)
			{
				auto& stamp_data = stationed_cpu_stamp_datas[index];
				if (!stamp_data.flag)
					continue;

				auto& timer_data = stamp_data.datas;
				float min = FLT_MAX, max = -FLT_MAX;
				for (auto& data : timer_data)
				{
					min = std::min(min, data);
					max = std::max(max, data);
				}
				auto kind = static_cast<timer>(index);
				std::string text = std::string(magic_enum::enum_name(kind));
				text += " : " + std::to_string(min) + ", " + std::to_string(max);

				ImPlot::PlotLine(text.data(),
					timer_data.data(),
					static_cast<int>(timer_data.size()),
					0.01f);
			}
			//	GPU
			if (stationed_gpu_stamp_datas.flag)
			{
				auto& timer_data = stationed_gpu_stamp_datas.datas;
				for (size_t index = 0; index < timer_data.size() - 1; ++index)
				{
					timer_data[index] = timer_data[index + 1];
				}
				timer_data.back() = render_executor->get_stationed_time_stamp();

				float min = FLT_MAX, max = -FLT_MAX;
				for (auto& data : timer_data)
				{
					min = std::min(min, data);
					max = std::max(max, data);
				}

				std::string text;
				text = "gpu : " + std::to_string(min) + ", " + std::to_string(max);

				ImPlot::PlotLine(text.data(),
					timer_data.data(),
					static_cast<int>(timer_data.size()),
					0.01f);
			}
			ImPlot::EndPlot();
		}
	}
	ImGui::End();

	//	GPUタイムスタンプ表示
	if (show_gpu_profiler)
	{
		if (ImGui::Begin("gpu_time_stamp_datas", &show_gpu_profiler))
		{
			float total_time = 0;
			for (auto& time_stamp : gpu_time_stamp_datas)
			{
				total_time += time_stamp.elapsed_time;
			}
			if (ImGui::ImTimeline::BeginTimeline("GPU", 0, total_time, 1))
			{
				total_time = 0;
				for (auto& time_stamp : gpu_time_stamp_datas)
				{
					double s = total_time;
					double e = total_time + time_stamp.elapsed_time;
					ImGui::PushID(static_cast<int>(total_time * 1000.0f));
					ImGui::ImTimeline::TimelineEvent(time_stamp.name.data(), s, e, 0);
					total_time += time_stamp.elapsed_time;
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("%s\n%f", time_stamp.name.data(), time_stamp.elapsed_time);
						ImGui::EndTooltip();
					}
					ImGui::Separator();
					ImGui::PopID();
				}
				static double dummy1 = 0;
				static double dummy2 = 1;
				ImGui::ImTimeline::EndTimeline(10, dummy1, dummy2, 1.0f);
			}
		}
		ImGui::End();
	}
}

void framework::gpu_snap_shot_begin()
{
	if (!is_gpu_snap_shot)
		return;

	//	GPUのスナップショット処理
	is_gpu_snap_shot_inner = is_gpu_snap_shot;
	is_gpu_snap_shot = false;
	render_executor->begin_gpu_snap_shot();
}

void framework::gpu_snap_shot_end()
{
	if (!is_gpu_snap_shot_inner)
		return;

	Sleep(1000);
	//	スナップショット終了処理
	is_gpu_snap_shot_inner = false;
	show_gpu_profiler = true;
	render_executor->end_gpu_snap_shot(gpu_time_stamp_datas);
}

#endif // defined(USE_IMGUI)
