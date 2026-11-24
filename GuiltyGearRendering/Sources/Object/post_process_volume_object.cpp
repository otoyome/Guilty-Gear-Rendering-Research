#include "object/post_process_volume_object.h"

#include "implot/implot.h"

DirectX::XMFLOAT3 post_process_volume_object::get_position() const
{
	return instance->get_position();
}

void post_process_volume_object::set_position(const DirectX::XMFLOAT3& position)
{
	instance->set_position(position);
}

post_process_volume_object::post_process_volume_object(std::string_view name)
: game_object(name)
{
	post_process_volume_manager::instance()->generate_post_process_volume(instance);

#if defined(USE_IMGUI)
	editor_context = editor_items::generate_context();
#endif	//	defined(USE_IMGUI)
}

post_process_volume_object::~post_process_volume_object()
{
}

void post_process_volume_object::post_update(float elapsed_time)
{
}

void post_process_volume_object::post_render()
{
	auto data = instance->get_screen_space_reflection_constant();
	data.accumalate_reset = false;
	instance->set_screen_space_reflection_constant(data);
}

void post_process_volume_object::on_active()
{
	instance->set_flag(post_process_volume::flags::active, true);
}

void post_process_volume_object::on_deactive()
{
	instance->set_flag(post_process_volume::flags::active, false);
}

bool post_process_volume_object::is_flag(post_process_volume::flags flag) const
{
	return instance->is_flag(flag);
}

void post_process_volume_object::set_flag(post_process_volume::flags flag, bool value)
{
	instance->set_flag(flag, value);
}

DirectX::XMFLOAT3 post_process_volume_object::get_inner_range() const
{
	return instance->get_inner_range();
}

void post_process_volume_object::set_inner_range(const DirectX::XMFLOAT3& inner_range)
{
	instance->set_inner_range(inner_range);
}

DirectX::XMFLOAT3 post_process_volume_object::get_outer_range() const
{
	return instance->get_outer_range();
}

void post_process_volume_object::set_outer_range(const DirectX::XMFLOAT3& outer_range)
{
	instance->set_outer_range(outer_range);
}

const DirectX::XMFLOAT4& post_process_volume_object::get_color() const
{
	return instance->get_color();
}

void post_process_volume_object::set_color(const DirectX::XMFLOAT4& color)
{
	instance->set_color(color);
}

#if defined(USE_IMGUI)
#define CHECK_BOX(FLAG) \
{\
	bool is_##FLAG = instance->is_flag(post_process_volume::flags::##FLAG);\
	ImGui::Checkbox(#FLAG, &is_##FLAG);\
	instance->set_flag(post_process_volume::flags::##FLAG, is_##FLAG);\
}

void post_process_volume_object::imgui_render_core()
{
	CHECK_BOX(unbind);
	auto position = instance->get_position();
	auto inner_range = instance->get_inner_range();
	auto outer_range = instance->get_outer_range();
	if (ImGui::DragFloat3("position", &position.x, 0.01f))
	{
		instance->set_position(position);
	}
	if (ImGui::DragFloat3("inner_range", &inner_range.x, 0.01f))
	{
		instance->set_inner_range(inner_range);
	}
	if (ImGui::DragFloat3("outer_range", &outer_range.x, 0.01f))
	{
		instance->set_outer_range(outer_range);
	}

	//	ブルーム
	imgui_bloom();

	//	レンズゴースト
	imgui_lens_ghost();

	//	被写界深度
	imgui_depth_of_field();

	//	SSR
	imgui_screen_space_reflection();

	//	モーションブラー
	imgui_motion_blur();

	//	露出補正
	imgui_exposure_compensation();

	//	トーンマッピング
	imgui_tone_mapping();

	//	マテリアル
	for (auto& material : instance->get_materials())
	{
		if (ImGui::TreeNode(system_utility::convert_shift_jis_to_utf8(material->get_name()).data()))
		{
			editor_items::edit_material(editor_context, *material);
			ImGui::TreePop();
		}
	}
}

void post_process_volume_object::imgui_bloom()
{
	ImGui::PushID("##bloom");
	bool is_change = false;
	auto bloom_constant = instance->get_bloom_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = bloom_constant.is_active();
	if (ImGui::Checkbox("bloom", &flag))
	{
		bloom_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("threshold", &bloom_constant.threshold, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("intensity", &bloom_constant.intensity, 0.01f, 0.0f, 10.0f, "%.6f"))
		{
			is_change = true;
		}

		//	モード設定
		{
			std::vector<std::string> row_texts;
			std::vector<char*> combo_texts;
			for (auto& name : magic_enum::enum_names<bloom_data::modes>())
			{
				row_texts.emplace_back(name);
			}
			for (auto& name : row_texts)
			{
				combo_texts.emplace_back(name.data());
			}
			int value = static_cast<int>(bloom_constant.mode);
			if (ImGui::Combo("modes", &value, combo_texts.data(), static_cast<int>(combo_texts.size())))
			{
				bloom_constant.mode = static_cast<bloom_data::modes>(value);
				is_change = true;
			}
		}
		switch (bloom_constant.mode)
		{
			case bloom_data::modes::standard:
			{
				if (ImGui::DragFloat("deviation", &bloom_constant.deviation, 0.001f))
				{
					is_change = true;
				}
				break;
			}
			case bloom_data::modes::convolution:
			{
				ImGui::PushID(editor_context->get_item_id());
				ImGui::Text("image_kernel");
				ImGui::SameLine();
				if (editor_items::edit_texture_item(editor_context, bloom_constant.image_kernel))
				{
					is_change = true;
				}
				ImGui::PopID();
				break;
			}
		}

		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_bloom_constant(bloom_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_lens_ghost()
{
	ImGui::PushID("##lens_ghost");
	bool is_change = false;
	auto lens_ghosts_constant = instance->get_lens_ghosts_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = lens_ghosts_constant.is_active();
	if (ImGui::Checkbox("lens_ghost", &flag))
	{
		lens_ghosts_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("threshold", &lens_ghosts_constant.threshold, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("intensity", &lens_ghosts_constant.intensity, 0.01f, 0.0f, 10.0f, "%.6f"))
		{
			is_change = true;
		}

		if (ImGui::DragFloat("dispersion_scale", &lens_ghosts_constant.dispersion_scale, 0.01f, 0.0f, 2.0f, "%.6f"))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("dispersion_offset", &lens_ghosts_constant.dispersion_offset, 0.01f))
		{
			is_change = true;
		}

		if (ImGui::DragFloat("ghost_mask_radius", &lens_ghosts_constant.ghost_mask_radius, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("ghost_mask_fade", &lens_ghosts_constant.ghost_mask_fade, 0.01f, 0.0f, 1.0f, "%.6f"))
		{
			is_change = true;
		}

		if (ImGui::SliderInt("iteration", &lens_ghosts_constant.iteration, 0, 10))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("ghost_bokeh", &lens_ghosts_constant.ghost_bokeh, 1.0f, 8.0f, "%.6f"))
		{
			is_change = true;
		}

		{
			ImGui::PushID(editor_context->get_item_id());
			ImGui::Text("image_kernel");
			ImGui::SameLine();
			if (editor_items::edit_texture_item(editor_context, lens_ghosts_constant.image_kernel))
			{
				is_change = true;
			}
			ImGui::PopID();
		}

		{
			ImGui::PushID(editor_context->get_item_id());
			ImGui::Text("ghost_color_lut");
			ImGui::SameLine();
			if (editor_items::edit_texture_item(editor_context, lens_ghosts_constant.ghost_color_lut))
			{
				is_change = true;
			}
			ImGui::PopID();
		}

		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_lens_ghosts_constant(lens_ghosts_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_depth_of_field()
{
	ImGui::PushID("##depth_of_field");
	bool is_change = false;
	auto depth_of_field_constant = instance->get_depth_of_field_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = depth_of_field_constant.is_active();
	if (ImGui::Checkbox("depth_of_field", &flag))
	{
		depth_of_field_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("focal_length(mm)", &depth_of_field_constant.focal_length, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("focus_distance(m)", &depth_of_field_constant.focus_distance, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("circle_of_confusion(mm)", &depth_of_field_constant.circle_of_confusion, 0.001f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("apeture", &depth_of_field_constant.apeture, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat2("adjust_length(m)", &depth_of_field_constant.adjust_length.x, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::Checkbox("display_dof_area", &depth_of_field_constant.display_dof_area))
		{
			is_change = true;
		}
		if (ImGui::DragInt("blur_size", &depth_of_field_constant.blur_size, 1.0f, 1, 64))
		{
			is_change = true;
		}
		if (ImGui::DragInt("precision_scale_factor", &depth_of_field_constant.precision_scale_factor, 1.0f, 1, 0xFF))
		{
			is_change = true;
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_depth_of_field_constant(depth_of_field_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_screen_space_reflection()
{
	ImGui::PushID("##screen_space_reflection");
	bool is_change = false;
	auto screen_space_reflection_constant = instance->get_screen_space_reflection_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = screen_space_reflection_constant.is_active();
	if (ImGui::Checkbox("screen_space_reflection", &flag))
	{
		screen_space_reflection_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("max_distance", &screen_space_reflection_constant.ray_max_distance, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("ray_resolution", &screen_space_reflection_constant.ray_resolution, 0.0f, 1.0f))
		{
			is_change = true;
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_consideration_uv_hole);
		if (ImGui::Checkbox("uv_hole_fill", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_consideration_uv_hole, flag);
			is_change = true;
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_reflection_dir_adjust);
		if (ImGui::Checkbox("dir_adjust", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_reflection_dir_adjust, flag);
			is_change = true;
		}
		if (flag)
		{
			if (ImGui::DragFloat2("tickness", &screen_space_reflection_constant.ray_tickness.x, 0.0001f))
			{
				is_change = true;
			}
		}
		else
		{
			if (ImGui::DragFloat("tickness", &screen_space_reflection_constant.ray_tickness.x, 0.0001f))
			{
				is_change = true;
			}
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_substep_check);
		if (ImGui::Checkbox("depthstep_check", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_substep_check, flag);
			is_change = true;
		}
		if (flag)
		{
			if (ImGui::DragInt("depthstep_iteration_max", &screen_space_reflection_constant.ray_substep_iteration_max, 1, 20))
			{
				is_change = true;
			}
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_step_random);
		if (ImGui::Checkbox("step_random", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_step_random, flag);
			is_change = true;
		}
		if (flag)
		{
			if (ImGui::Checkbox("accumalate_reset", &screen_space_reflection_constant.accumalate_reset))
			{
				is_change = true;
			}
			if (ImGui::DragFloat("velocity_reset_threshold", &screen_space_reflection_constant.velocity_reset_threshold, 0.1f, 0.0f, 100.0f))
			{
				is_change = true;
			}
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_screen_space_reflection_constant(screen_space_reflection_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_motion_blur()
{
	ImGui::PushID("##motion_blur");
	bool is_change = false;
	auto motion_blur_constant = instance->get_motion_blur_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = motion_blur_constant.is_active();
	if (ImGui::Checkbox("motion_blur", &flag))
	{
		motion_blur_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragInt("iteration", &motion_blur_constant.iteration, 0.01f, 0, 20))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("jitter_noise_value", &motion_blur_constant.jitter_noise_value, 0.0f, 1.0f))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("velocity_multiplyer", &motion_blur_constant.velocity_multiplyer, 0.0f, 10.0f))
		{
			is_change = true;
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_motion_blur_constant(motion_blur_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_exposure_compensation()
{
	ImGui::PushID("##exposure_compensation");
	bool is_change = false;
	auto exposure_compensation_constant = instance->get_exposure_compensation_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = exposure_compensation_constant.is_active();
	if (ImGui::Checkbox("exposure", &flag))
	{
		exposure_compensation_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::SliderFloat("exposure_adapt_rate", &exposure_compensation_constant.adapt_rate, 0.0f, +1.0f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat2("luminance_range", &exposure_compensation_constant.luminance_range.x, 0.1f, 0, 100000))
		{
			is_change = true;
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_exposure_compensation_constant(exposure_compensation_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_tone_mapping()
{
	ImGui::PushID("##tone_mapping_constant");
	bool is_change = false;
	auto tone_mapping_constant = instance->get_tone_mapping_constant();
	if (ImGui::TreeNode("tone_mapping"))
	{
		//	モード設定
		{
			std::vector<std::string> row_texts;
			std::vector<char*> combo_texts;
			for (auto& name : magic_enum::enum_names<tone_mapping_data::tone_mapping_type>())
			{
				row_texts.emplace_back(name);
			}
			for (auto& name : row_texts)
			{
				combo_texts.emplace_back(name.data());
			}
			int value = static_cast<int>(tone_mapping_constant.get_tone_map_type());
			if (ImGui::Combo("tone_map_type", &value, combo_texts.data(), static_cast<int>(combo_texts.size())))
			{
				tone_mapping_constant.set_tone_map_type(static_cast<tone_mapping_data::tone_mapping_type>(value));
				is_change = true;
			}
		}

		//	トーンカーブ
		if (ImPlot::BeginPlot("tone curve"))
		{
			ImPlot::SetupAxis(ImAxis_X1, "input luminance");
			ImPlot::SetupAxis(ImAxis_Y1, "output luminance");
			auto function = [&](float x)
			{
				auto params = tone_mapping_constant.tone_mapping_variables;
				if (tone_mapping_constant.get_tone_map_type() == tone_mapping_data::tone_mapping_type::reinhard)
				{
					return std::clamp(x / (1 + x), 0.0f, 1.0f);
				}
				if (tone_mapping_constant.get_tone_map_type() == tone_mapping_data::tone_mapping_type::aces_krzysztof_narkowicz)
				{
					float a = x * (params[0] * x + params[1]);
					float b = x * (params[2] * x + params[3]) + params[4];
					return std::clamp(a / b, 0.0f, 1.0f);
				}
				if (tone_mapping_constant.get_tone_map_type() == tone_mapping_data::tone_mapping_type::aces_stephen_hill)
				{
					float a = x * (params[0] + x) - params[1];
					float b = x * (params[2] * x + params[3]) + params[4];
					return std::clamp(a / b, 0.0f, 1.0f);
				}
				if (tone_mapping_constant.get_tone_map_type() == tone_mapping_data::tone_mapping_type::gran_turismo)
				{
					auto W_f = [](float x,float e0,float e1)
					{
						if (x <= e0)
							return 0.0f;
						if (x >= e1)
							return 1.0f;
						float a = (x - e0) / (e1 - e0);
						return a * a * (3.0f - 2.0f * a);
					};
					auto H_f = [](float x, float e0, float e1)
					{
						if (x <= e0)
							return 0.0f;
						if (x >= e1)
							return 1.0f;
						return (x - e0) / (e1 - e0);
					};
					float P = params[0];
					float a = params[1];
					float m = params[2];
					float l = params[3];
					float c = params[4];
					float b = params[5];
					static const float e = 2.71828f;

					float l0 = (P - m) * l / a;
					float L0 = m - m / a;
					float L1 = m + (1 - m) / a;
					float L_x = m + a * (x - m);
					float T_x = m * pow(x / m, c) + b;
					float S0 = m + l0;
					float S1 = m + a * l0;
					float C2 = a * P / (P - S1);
					float S_x = P - (P - S1) * pow(e, -(C2 * (x - S0) / P));
					float w0_x = 1 - W_f(x, 0, m);
					float w2_x = H_f(x, m + l0, m + l0);
					float w1_x = 1 - w0_x - w2_x;
					float f_x = T_x * w0_x + L_x * w1_x + S_x * w2_x;
					return std::clamp(f_x, 0.0f, 1.0f);
				}
				return x;
			};
			// 横軸：輝度値, 縦軸：トーンマッピング後の出力輝度
			std::vector<float> plot_buffer;
			plot_buffer.resize(1000);
			for (int index = 0; index < plot_buffer.size(); ++index)
			{
				plot_buffer[index]	= function(static_cast<float>(index) / 100.0f);
			}
			ImPlot::PlotLine("", plot_buffer.data(), static_cast<int>(plot_buffer.size()), 0.01f);
			ImPlot::EndPlot();
		}
		//	モード毎のパラメーター設定
		switch (tone_mapping_constant.get_tone_map_type())
		{
			case tone_mapping_data::tone_mapping_type::aces_krzysztof_narkowicz:
			{
				if (ImGui::SliderFloat("highlight", tone_mapping_constant.tone_mapping_variables + 0, 0.0f, 5.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("shade_offset", tone_mapping_constant.tone_mapping_variables + 1, 0.0f, 5.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("highlight_compression", tone_mapping_constant.tone_mapping_variables + 2, 0.0f, 5.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("midtone_balance", tone_mapping_constant.tone_mapping_variables + 3, 0.0f, 5.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("normalize_factor", tone_mapping_constant.tone_mapping_variables + 4, 0.0f, 1.0f, "%.6f"))
				{
					is_change = true;
				}
				break;
			}
			case tone_mapping_data::tone_mapping_type::aces_stephen_hill:
			{
				if (ImGui::SliderFloat("shade_offset", tone_mapping_constant.tone_mapping_variables + 0, 0.0f, 1.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("low_end_correction", tone_mapping_constant.tone_mapping_variables + 1, -0.1f, 0.1f, "%.6f"))
				{
					is_change = true;
				}

				if (ImGui::SliderFloat("highlight_scale", tone_mapping_constant.tone_mapping_variables + 2, 0.0f, 5.0f, "%.6f"))
				{
					is_change = true;
				}

				if (ImGui::SliderFloat("midtone_balance", tone_mapping_constant.tone_mapping_variables + 3, 0.0f, 5.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("normalize_factor", tone_mapping_constant.tone_mapping_variables + 4, 0.0f, 5.0f, "%.6f"))
				{
					is_change = true;
				}
				break;
			}
			case tone_mapping_data::tone_mapping_type::gran_turismo:
			{
				//	Pはモニターの最大輝度なので、デバイスから調べて設定する
				if (ImGui::SliderFloat("contrast", tone_mapping_constant.tone_mapping_variables + 1, 0.0f, 2.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("linear_start", tone_mapping_constant.tone_mapping_variables + 2, 0.0f, 2.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("linear_length", tone_mapping_constant.tone_mapping_variables + 3, 0.0f, 2.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("toe_strength", tone_mapping_constant.tone_mapping_variables + 4, 0.0f, 2.0f, "%.6f"))
				{
					is_change = true;
				}
				if (ImGui::SliderFloat("toe_offset", tone_mapping_constant.tone_mapping_variables + 5, 0.0f, 2.0f, "%.6f"))
				{
					is_change = true;
				}
				break;
			}
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_tone_mapping_constant(tone_mapping_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

#undef CHECK_BOX
#endif	//	defined(USE_IMGUI)
