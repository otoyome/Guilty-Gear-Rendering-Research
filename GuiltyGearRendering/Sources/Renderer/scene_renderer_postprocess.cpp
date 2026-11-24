#include "Renderer/scene_renderer_postprocess.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/deferred_lighting_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"

#include "PipelineState/post_process_pipeline_state.h"

#include "PipelineState/anti_alias_pipeline_state.h"

#include "System/engine_config_data.h"

namespace
{
	//	ポストプロセスボリューム取得
	post_process_volume_ptr search_volume(const DirectX::XMFLOAT3& camera_position, const render_data_context& context)
	{
		float length = FLT_MAX;
		post_process_volume_ptr result, unblind;
		for (auto& volume : context.post_process_volume_container.volumes)
		{
			if (volume->is_flag(post_process_volume::flags::unbind))
			{
				unblind = volume;
				continue;
			}

			//	箱との距離を算出して最も近いのをとりあえず入れておく
			auto box = collision::inner::axis_aligned_bounding_box(volume->get_outer_range(), volume->get_position());
			float box_to_length = box.calculate_point_to_length(camera_position);
			if (box_to_length < length)
			{
				length = box_to_length;
				result = volume;
			}
		}

		if (!result)
		{
			result = unblind;
		}
		return result;
	}

	void draw(const graphics_context& context,
			  graphics_pipeline_state_ptr pipeline_state,
			  UINT vertex_count,
			  D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			  UINT vertex_offset = 0)
	{
		buffer_datas buffer_data;
		buffer_data.vertex_count = vertex_count;
		context.device_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.device_context->IASetPrimitiveTopology(primitive_topology);
		context.device_context->Draw(vertex_count, vertex_offset);
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region PostprocessMaterial
post_process_renderer_base::post_process_renderer_base(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, work_buffer_rendering_pass()
, work_buffer_viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
, scene_color_rendering_pass()
, scene_color_viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	パイプラインステート生成
	pipeline_state_manager* instance = pipeline_state_manager::instance();
	instance->generate_pipeline_state<write_back_pipeline_state>("write_back", &write_back);
}

post_process_renderer_base::~post_process_renderer_base()
{
}

void post_process_renderer_base::render(renderer_context& renderer_context)
{
	//	ポストプロセスボリュームが無ければ適応しない
	DirectX::XMFLOAT3 camera_position;
	camera_position.x = renderer_context.scene_constant.camera_position.x;
	camera_position.y = renderer_context.scene_constant.camera_position.y;
	camera_position.z = renderer_context.scene_constant.camera_position.z;
	post_process_volume_ptr inner_volume = search_volume(camera_position, *renderer_context.render_data);
	if (!inner_volume)
		return;

	bool is_find = false;
	for (auto& material : inner_volume->get_materials())
	{
		if (material->get_domain() != material::domains::post_process ||
			material->get_blendable_location() != get_material_blendable_locations())
			continue;
		is_find = true;
		break;
	}
	if (!is_find)
		return;

	job_executor.execute([this, inner_volume](graphics_context& context)
	{
		//	ポストプロセス
		int rendering_count = 0;
		for (auto& material : inner_volume->get_materials())
		{
			if (material->get_domain() != material::domains::post_process ||
				material->get_blendable_location() != get_material_blendable_locations())
				continue;

			auto pipeline_state = material->get_pipeline_state();
			if (!pipeline_state)
				continue;

			int destination_index = (rendering_count + 1) % 2;
			int source_index = (rendering_count + 0) % 2;

			//	描画先設定
			work_buffer_rendering_pass[destination_index].activate(context.device_context);

			//	ビューポートの設定
			context.device_context->RSSetViewports(1, &work_buffer_viewport);

			//	シーン描画
			if (rendering_count == 0)
				context.scene_color_texture = render_targets->scene_color_texture;
			else
				context.scene_color_texture = render_targets->work_buffer_scene_textures[source_index];
			context.scene_depth_texture = render_targets->scene_depth_stencil_texture;

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *material, nullptr);
			draw(context, pipeline_state, 4);
			pipeline_state->end(context);

			work_buffer_rendering_pass[destination_index].deactivate(context.device_context);
			++rendering_count;
		}

		//	書き戻し
		if (rendering_count > 0)
		{
			int source_index = (rendering_count + 0) % 2;
			//	描画先設定
			scene_color_rendering_pass.activate(context.device_context);

			//	ビューポートの設定
			context.device_context->RSSetViewports(1, &scene_color_viewport);

			//	シーン描画
			context.scene_color_texture = render_targets->work_buffer_scene_textures[source_index];

			write_back->begin(context);
			draw(context, write_back, 4);
			write_back->end(context);

			scene_color_rendering_pass.deactivate(context.device_context);
		}
	}, "post_process_material_pass", renderer_context, get_render_basis_sort_order());
}

void post_process_renderer_base::resize_buffer(int width, int height)
{
	// ビューポートの設定
	for (UINT index = 0; index < ARRAYSIZE(work_buffer_rendering_pass); ++index)
	{
		work_buffer_rendering_pass[index].render_targets.clear();
		work_buffer_rendering_pass[index].render_targets.emplace_back(render_targets->work_buffer_scene_textures[index]);
	}
	work_buffer_viewport.TopLeftX = 0.0f;
	work_buffer_viewport.TopLeftY = 0.0f;
	work_buffer_viewport.Width = static_cast<float>(width);
	work_buffer_viewport.Height = static_cast<float>(height);
	work_buffer_viewport.MinDepth = 0.0f;
	work_buffer_viewport.MaxDepth = 1.0f;

	// ビューポートの設定
	scene_color_rendering_pass.render_targets.clear();
	scene_color_rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	scene_color_viewport.TopLeftX = 0.0f;
	scene_color_viewport.TopLeftY = 0.0f;
	scene_color_viewport.Width = static_cast<float>(width);
	scene_color_viewport.Height = static_cast<float>(height);
	scene_color_viewport.MinDepth = 0.0f;
	scene_color_viewport.MaxDepth = 1.0f;
}
#pragma endregion	//	PostprocessMaterial

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region SSR

//	半解像度でSSRを生成する
//	@remainder	G-Bufferを縮小して生成し、
//				バイラテラルアップサンプリングするようにしないと縁の部分がおかしくなるがやってない。
#define	USE_SSR_HALF_BUFFER

namespace ssr
{
	struct denoise_constants
	{
		DirectX::XMUINT2	texture_size;
		float				reset_flag;
		float				iteration;

		float				denoise_velocity_max;
		float				denoise_color_sigma;
		float				denoise_normal_sigma;
		float				denoise_depth_sigma;

		float				denoise_temporal_blend_factor;
		float				denoise_counter_max;
		DirectX::XMUINT2	denoise_dummy;
	};
	constexpr DirectX::XMUINT2 denoise_threads = { 16, 16 };
	constexpr UINT denoise_cbv_index = 3;
}

screen_space_reflection_renderer::screen_space_reflection_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	定数バッファの生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;

		buffer_desc.ByteWidth = sizeof(ssr::denoise_constants);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, denoise_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	SSR系シェーダー準備
	{
		CONST char* shader_names[] =
		{
			"screen_space_reflection_uv",
			"screen_space_reflection_color",
			"screen_space_reflection_composite",
		};
		instance->generate_pipeline_state<screen_space_reflection_uv_pipeline_state>(shader_names[0], &ssr_uv_pipeline_state);
		instance->generate_pipeline_state<screen_space_reflection_color_pipeline_state>(shader_names[1], &ssr_color_pipeline_state);
		instance->generate_pipeline_state<screen_space_reflection_composite_pipeline_state>(shader_names[2], &ssr_composite_pipeline_state);
	}

	//	デノイズ系シェーダー準備
	if (shader_manager* instance = shader_manager::instance())
	{
		load_shader_cso desc;
		desc.file_path = "Assets/CSO/screen_space_reflection_denoise_cs.cso";
		instance->load_shader<compute_shader>(denoise, desc);
		desc.file_path = "Assets/CSO/screen_space_reflection_temporal_filtering_cs.cso";
		instance->load_shader<compute_shader>(temporal_filter, desc);
	}

	//	SSR系合成シェーダー準備
	{
		instance->generate_pipeline_state<write_back_pipeline_state>("write_back", &write_back);
	}
}

screen_space_reflection_renderer::~screen_space_reflection_renderer()
{
}

void screen_space_reflection_renderer::render(renderer_context& renderer_context)
{
	//	ポストプロセスボリュームが無ければ適応しない
	DirectX::XMFLOAT3 camera_position;
	camera_position.x = renderer_context.scene_constant.camera_position.x;
	camera_position.y = renderer_context.scene_constant.camera_position.y;
	camera_position.z = renderer_context.scene_constant.camera_position.z;
	post_process_volume_ptr inner_volume = search_volume(camera_position, *renderer_context.render_data);
	if (!inner_volume)
		return;
	auto& ssr_data = inner_volume->get_screen_space_reflection_constant();
	if (!ssr_data.is_active())
		return;

	//	SSR生成
	job_executor.execute([this, inner_volume](graphics_context& context)
	{
		auto& ssr_data = inner_volume->get_screen_space_reflection_constant();

		context.clear_constant_buffers();
		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
		context.clear_shader_slots();

		//	反射
		{
			context.debug_begin_event(L"reflection_pass");
			render_reflection(context, inner_volume);
			context.debug_end_event();
		}

		//	デノイズ
		if (ssr_data.is_flag(common_pipeline_states::screen_space_reflection_flags_step_random))
		{
			context.debug_begin_event(L"denoise_pass");
			render_denoise(context, inner_volume);
			context.debug_end_event();
		}

		//	合成
		{
			context.debug_begin_event(L"composite_pass");
			render_composite(context, inner_volume);
			context.debug_end_event();
		}

		context.debug_end_event();
	}, "screen_space_reflection_pass", renderer_context, get_render_basis_sort_order() + 2, true);
}

void screen_space_reflection_renderer::render_reflection(graphics_context& context, post_process_volume_ptr inner_volume)
{
	auto& ssr_data = inner_volume->get_screen_space_reflection_constant();

	//	リソース類設定
	{
		context.screen_space_reflection_constant.ray_resolution = ssr_data.ray_resolution;
		context.screen_space_reflection_constant.ray_max_distance = ssr_data.ray_max_distance;
		context.screen_space_reflection_constant.ray_substep_iteration_max = ssr_data.ray_substep_iteration_max;
		context.screen_space_reflection_constant.ray_flags = ssr_data.ray_flags;
		context.screen_space_reflection_constant.ray_tickness = ssr_data.ray_tickness;
		context.screen_space_reflection_constant.viewport_size.x = ssr_uv_pass_viewport.Width;
		context.screen_space_reflection_constant.viewport_size.y = ssr_uv_pass_viewport.Height;
		context.screen_space_reflection_constant.viewport_size.z = 1.0f / ssr_uv_pass_viewport.Width;
		context.screen_space_reflection_constant.viewport_size.w = 1.0f / ssr_uv_pass_viewport.Height;
	}

	//	反射UV情報を算出
	{
		ssr_uv_pass.activate(context.device_context);
		context.device_context->RSSetViewports(1, &ssr_uv_pass_viewport);

		//	SRV設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}
		context.g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::base_color)] = render_targets->scene_color_texture;

		ssr_uv_pipeline_state->begin(context);
		draw(context, ssr_uv_pipeline_state, 4);
		ssr_uv_pipeline_state->end(context);

		ssr_uv_pass.deactivate(context.device_context);
	}

	context.clear_render_target_slots();
	context.clear_shader_resorce_views();

	//	反射色情報を算出
	{
		ssr_color_pass.activate(context.device_context);
		context.device_context->RSSetViewports(1, &ssr_color_pass_viewport);

		//	SRV設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}
		context.g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::base_color)] = render_targets->scene_color_texture;
		context.work_texture[0] = render_targets->work_buffer_half_scene_textures[0];

		ssr_color_pipeline_state->begin(context);
		draw(context, ssr_color_pipeline_state, 4);
		ssr_color_pipeline_state->end(context);

		ssr_color_pass.deactivate(context.device_context);
	}

	context.clear_render_target_slots();
	context.clear_shader_resorce_views();
}

void screen_space_reflection_renderer::render_denoise(graphics_context& context, post_process_volume_ptr inner_volume)
{
	auto& ssr_data = inner_volume->get_screen_space_reflection_constant();

	DirectX::XMUINT2 texture_size;
	texture_size.x = static_cast<UINT>(ssr_color_pass_viewport.Width + 0.1f);
	texture_size.y = static_cast<UINT>(ssr_color_pass_viewport.Height + 0.1f);
	{
		//	リソース類設定
		ssr::denoise_constants constant;
		constant.texture_size = texture_size;
		constant.reset_flag = ssr_data.accumalate_reset ? 1.0f : 0.0f;
		constant.denoise_velocity_max = ssr_data.velocity_reset_threshold;
		constant.denoise_color_sigma = 1.0f;
		constant.denoise_normal_sigma = 0.5f;
		constant.denoise_depth_sigma = 0.25f;
		constant.denoise_temporal_blend_factor = 0.8f;
		constant.denoise_counter_max = 10;
		constant.iteration = 0;	//	1回だけに固定しておく
		context.device_context->UpdateSubresource(denoise_constant_buffer.Get(), 0, 0, &constant, 0, 0);

		context.device_context->CSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
		context.device_context->CSSetConstantBuffers(ssr::denoise_cbv_index, 1, denoise_constant_buffer.GetAddressOf());
	}

	UINT thread_x = math::alignment(texture_size.x, ssr::denoise_threads.x) / ssr::denoise_threads.x;
	UINT thread_y = math::alignment(texture_size.y, ssr::denoise_threads.y) / ssr::denoise_threads.y;

	//	デノイズ処理
	{
		auto src_srv = render_targets->work_buffer_scene_textures[1]->get_shader_resource_view().Get();
		auto nd_srv = render_targets->g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::normal_depth)]->get_shader_resource_view().Get();
		auto dest_uav = render_targets->work_buffer_scene_textures[0]->get_unordered_access_view().Get();
		auto accumlate_uav = render_targets->ssr_accumlate_textures[accumlate_flip_flag ? 0 : 1]->get_unordered_access_view().Get();

		context.device_context->CSSetShaderResources(0, 1, &src_srv);
		context.device_context->CSSetShaderResources(1, 1, &nd_srv);

		context.device_context->CSSetUnorderedAccessViews(0, 1, &dest_uav, nullptr);
		context.device_context->CSSetUnorderedAccessViews(1, 1, &accumlate_uav, nullptr);

		context.device_context->CSSetShader(denoise->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch(thread_x, thread_y, 1);

		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	//	蓄積
	{
		auto src_srv = render_targets->work_buffer_scene_textures[0]->get_shader_resource_view().Get();
		auto accumlate_srv = render_targets->ssr_accumlate_textures[accumlate_flip_flag ? 1 : 0]->get_shader_resource_view().Get();
		auto vel_srv = render_targets->g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::velocity)]->get_shader_resource_view().Get();
		auto counter_srv = render_targets->ssr_counter_textures[accumlate_flip_flag ? 1 : 0]->get_shader_resource_view().Get();

		auto dest_uav = render_targets->work_buffer_scene_textures[1]->get_unordered_access_view().Get();
		auto accumlate_uav = render_targets->ssr_accumlate_textures[accumlate_flip_flag ? 0 : 1]->get_unordered_access_view().Get();
		auto counter_uav = render_targets->ssr_counter_textures[accumlate_flip_flag ? 0 : 1]->get_unordered_access_view().Get();

		context.device_context->CSSetShaderResources(0, 1, &src_srv);
		context.device_context->CSSetShaderResources(1, 1, &accumlate_srv);
		context.device_context->CSSetShaderResources(2, 1, &vel_srv);
		context.device_context->CSSetShaderResources(3, 1, &counter_srv);

		context.device_context->CSSetUnorderedAccessViews(0, 1, &dest_uav, nullptr);
		context.device_context->CSSetUnorderedAccessViews(1, 1, &accumlate_uav, nullptr);
		context.device_context->CSSetUnorderedAccessViews(2, 1, &counter_uav, nullptr);

		context.device_context->CSSetShader(temporal_filter->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch(thread_x, thread_y, 1);

		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	accumlate_flip_flag = !accumlate_flip_flag;

	context.clear_constant_buffers();
	context.clear_shader_resorce_views();
	context.clear_unordered_access_views();
}

void screen_space_reflection_renderer::render_composite(graphics_context& context, post_process_volume_ptr inner_volume)
{
	auto& ssr_data = inner_volume->get_screen_space_reflection_constant();

	//	合成
	{
		ssr_composite_pass.activate(context.device_context);
		context.device_context->RSSetViewports(1, &ssr_composite_pass_viewport);

		//	SRV設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}
		context.g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::base_color)] = render_targets->scene_color_texture;
		context.work_texture[0] = render_targets->work_buffer_scene_textures[1];
		context.work_texture[1] = render_targets->work_buffer_scene_textures[1];

		ssr_composite_pipeline_state->begin(context);
		draw(context, ssr_composite_pipeline_state, 4);
		ssr_composite_pipeline_state->end(context);

		ssr_composite_pass.deactivate(context.device_context);
	}

	context.clear_shader_resorce_views();

	//	合成結果をシーンに描画
	{
		scene_color_rendering_pass.activate(context.device_context);
		context.device_context->RSSetViewports(1, &scene_color_viewport);

		context.scene_color_texture = render_targets->work_buffer_scene_textures[0];

		write_back->begin(context);
		draw(context, write_back, 4);
		write_back->end(context);

		scene_color_rendering_pass.deactivate(context.device_context);
	}

	context.clear_constant_buffers();
	context.clear_shader_resorce_views();
	context.clear_shader_slots();
}

void screen_space_reflection_renderer::resize_buffer(int width, int height)
{
	//	SSRパス
	{
		//	SSR:UVパス
		ssr_uv_pass.render_targets.clear();

		texture_ptr ssr_uv_texture;
#if	defined(USE_SSR_HALF_BUFFER)
		ssr_uv_texture = render_targets->work_buffer_half_scene_textures[0];
#else	//	defined(USE_SSR_HALF_BUFFER)
		ssr_uv_texture = render_targets->work_buffer_scene_textures[0];
#endif	//	defined(USE_SSR_HALF_BUFFER)
		ssr_uv_pass.render_targets.emplace_back(ssr_uv_texture);
		ssr_uv_pass.render_targets.back().clear_color_value[0] = 0.0f;
		ssr_uv_pass.render_targets.back().clear_color_value[1] = 0.0f;
		ssr_uv_pass.render_targets.back().clear_color_value[2] = 0.0f;
		ssr_uv_pass.render_targets.back().clear_color_value[3] = 1.0f;
		ssr_uv_pass_viewport.TopLeftX = 0.0f;
		ssr_uv_pass_viewport.TopLeftY = 0.0f;
		ssr_uv_pass_viewport.Width = static_cast<float>(ssr_uv_texture->get_width());
		ssr_uv_pass_viewport.Height = static_cast<float>(ssr_uv_texture->get_height());
		ssr_uv_pass_viewport.MinDepth = 0.0f;
		ssr_uv_pass_viewport.MaxDepth = 1.0f;

		//	SSR:色パス
		ssr_color_pass.render_targets.clear();
		ssr_color_pass.render_targets.emplace_back(render_targets->work_buffer_scene_textures[1]);
		ssr_color_pass.render_targets.back().clear_color_value[0] = 0.0f;
		ssr_color_pass.render_targets.back().clear_color_value[1] = 0.0f;
		ssr_color_pass.render_targets.back().clear_color_value[2] = 0.0f;
		ssr_color_pass.render_targets.back().clear_color_value[3] = 1.0f;
		ssr_color_pass_viewport.TopLeftX = 0.0f;
		ssr_color_pass_viewport.TopLeftY = 0.0f;
		ssr_color_pass_viewport.Width = static_cast<float>(render_targets->work_buffer_scene_textures[1]->get_width());
		ssr_color_pass_viewport.Height = static_cast<float>(render_targets->work_buffer_scene_textures[1]->get_height());
		ssr_color_pass_viewport.MinDepth = 0.0f;
		ssr_color_pass_viewport.MaxDepth = 1.0f;

		//	SSR:マージパス
		ssr_composite_pass.render_targets.clear();
		ssr_composite_pass.render_targets.emplace_back(render_targets->work_buffer_scene_textures[0]);
		ssr_composite_pass.render_targets.back().clear_color_value[0] = 0.0f;
		ssr_composite_pass.render_targets.back().clear_color_value[1] = 0.0f;
		ssr_composite_pass.render_targets.back().clear_color_value[2] = 0.0f;
		ssr_composite_pass.render_targets.back().clear_color_value[3] = 1.0f;
		ssr_composite_pass_viewport.TopLeftX = 0.0f;
		ssr_composite_pass_viewport.TopLeftY = 0.0f;
		ssr_composite_pass_viewport.Width = static_cast<float>(render_targets->work_buffer_scene_textures[0]->get_width());
		ssr_composite_pass_viewport.Height = static_cast<float>(render_targets->work_buffer_scene_textures[0]->get_height());
		ssr_composite_pass_viewport.MinDepth = 0.0f;
		ssr_composite_pass_viewport.MaxDepth = 1.0f;
	}

	//	合成パス
	{
		scene_color_rendering_pass.render_targets.clear();
		scene_color_rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);

		// ビューポート
		scene_color_viewport.TopLeftX = 0.0f;
		scene_color_viewport.TopLeftY = 0.0f;
		scene_color_viewport.Width = static_cast<float>(render_targets->scene_color_texture->get_width());
		scene_color_viewport.Height = static_cast<float>(render_targets->scene_color_texture->get_height());
		scene_color_viewport.MinDepth = 0.0f;
		scene_color_viewport.MaxDepth = 1.0f;
	}
}

#undef USE_SSR_HALF_BUFFER

#pragma endregion	//	SSR

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region DOF
namespace dof
{
	struct parameter_constants
	{
		DirectX::XMFLOAT4 parameter[3];	//	x : focal_length(焦点距離)
										//	y : focus_distance(ピントの合う距離)
										//	z : circle_of_confusion(許容散乱円)
										//	w : apeture(絞り値)

										//	xy : 被写界深度の調整距離
										//	z : 空き
										//	w : デバッグフラグ
    
    									//	xy : 空き
    									//	z : ブラー最大半径
										//  w : スケールファクター

		DirectX::XMINT4 resolutions;	//  xy : シーン解像度, zw : バッファ解像度
	};
	constexpr int parameter_cbv_index = 1;
	constexpr int scene_color_srv_index = 0;
	constexpr int scene_depth_srv_index = 1;
	constexpr int sat_color_srv_index = 2;

	static constexpr DirectX::XMUINT3 filter_thread_count = {8, 8, 1};
	static const int max_blur_radius = 64;

	static constexpr DirectX::XMUINT3 vertical_integrate_thread_count = {64, 1, 1};
}

depth_of_field_post_process_renderer::depth_of_field_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	コンピュートシェーダー生成関数
	if (shader_manager* instance = shader_manager::instance())
	{
		load_shader_cso desc;
		desc.file_path = "Assets/CSO/dof_filter_setup_cs.cso";
		instance->load_shader<compute_shader>(dof_filter_setup, desc);
		desc.file_path = "Assets/CSO/dof_vertical_integrate_cs.cso";
		instance->load_shader<compute_shader>(dof_vertical_integrate, desc);
		desc.file_path = "Assets/CSO/dof_apply_cs.cso";
		instance->load_shader<compute_shader>(dof_apply, desc);
	}

	//	定数バッファの生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;

		buffer_desc.ByteWidth = sizeof(dof::parameter_constants);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

depth_of_field_post_process_renderer::~depth_of_field_post_process_renderer()
{
}

void depth_of_field_post_process_renderer::render(renderer_context& renderer_context)
{
	//	ポストプロセスボリュームが無ければ適応しない
	DirectX::XMFLOAT3 camera_position;
	camera_position.x = renderer_context.scene_constant.camera_position.x;
	camera_position.y = renderer_context.scene_constant.camera_position.y;
	camera_position.z = renderer_context.scene_constant.camera_position.z;
	post_process_volume_ptr inner_volume = search_volume(camera_position, *renderer_context.render_data);
	if (!inner_volume)
		return;

	auto& depth_of_field_constant = inner_volume->get_depth_of_field_constant();
	if (!depth_of_field_constant.is_active())
		return;

	job_executor.execute([this, inner_volume](graphics_context& context)
	{
		auto& depth_of_field_constant = inner_volume->get_depth_of_field_constant();

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		context.device_context->CSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
		context.device_context->CSSetConstantBuffers(dof::parameter_cbv_index, 1, constant_buffer.GetAddressOf());

		update_constant_buffer(context, depth_of_field_constant, filter_buffer_width, filter_buffer_height);

		//	フィルターセットアップ
		{
			DirectX::XMUINT4 clear_value = {0, 0, 0, 0};
			context.device_context->ClearUnorderedAccessViewUint(filter_buffer_uav.Get(), &clear_value.x);
			context.device_context->CSSetUnorderedAccessViews(0, 1, filter_buffer_uav.GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(1, 1, render_targets->work_buffer_rgba32float_texture->get_unordered_access_view().GetAddressOf(), nullptr);

			context.device_context->CSSetShaderResources(dof::scene_color_srv_index, 1, render_targets->scene_color_texture->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(dof::scene_depth_srv_index, 1, render_targets->scene_depth_stencil_texture->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShader(dof_filter_setup->get_shader().Get(), nullptr, 0);
			UINT x = static_cast<UINT>( ceil(scene_color_viewport.Width / dof::filter_thread_count.x) + 0.1f);
			UINT y = static_cast<UINT>(ceil(scene_color_viewport.Height / dof::filter_thread_count.y) + 0.1f);
			context.device_context->Dispatch(x, y, 1);
		}
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		//	範囲総和テーブル生成
		{
			context.device_context->CSSetShader(dof_vertical_integrate->get_shader().Get(), nullptr, 0);
			//	Y
			{
				context.device_context->CSSetUnorderedAccessViews(0, 1, filter_buffer_uav.GetAddressOf(), nullptr);
				context.device_context->CSSetUnorderedAccessViews(1, 1, filter_buffer_transpose_uav.GetAddressOf(), nullptr);

				UINT x = ((filter_buffer_width + dof::vertical_integrate_thread_count.x - 1) / dof::vertical_integrate_thread_count.x);
				context.device_context->Dispatch(x, 1, 1);
			}
			context.clear_unordered_access_views();

			//	X
			{
				update_constant_buffer(context, depth_of_field_constant, filter_buffer_height, filter_buffer_width);
				context.device_context->CSSetUnorderedAccessViews(0, 1, filter_buffer_transpose_uav.GetAddressOf(), nullptr);
				context.device_context->CSSetUnorderedAccessViews(1, 1, filter_buffer_uav.GetAddressOf(), nullptr);

				UINT x = ((filter_buffer_height + dof::vertical_integrate_thread_count.x - 1) / dof::vertical_integrate_thread_count.x);
				context.device_context->Dispatch(x, 1, 1);
			}
			context.clear_unordered_access_views();
		}

		//	シーンに書き戻し
		{
			update_constant_buffer(context, depth_of_field_constant, filter_buffer_width, filter_buffer_height);
			context.device_context->CSSetUnorderedAccessViews(0, 1, render_targets->scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);
			context.device_context->CSSetShaderResources(0, 1, filter_buffer_srv.GetAddressOf());

			context.device_context->CSSetShader(dof_apply->get_shader().Get(), nullptr, 0);
			UINT x = static_cast<UINT>( ceil(scene_color_viewport.Width / dof::filter_thread_count.x) + 0.1f);
			UINT y = static_cast<UINT>(ceil(scene_color_viewport.Height / dof::filter_thread_count.y) + 0.1f);
			context.device_context->Dispatch(x, y, 1);
		}

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}, "depth_of_field_pass", renderer_context, get_render_basis_sort_order(), true);
}

void depth_of_field_post_process_renderer::resize_buffer(int width, int height)
{
	HRESULT hr;
	auto device = render_data::instance()->get_device().Get();

	//	各種サイズ計算
	{
		UINT blur_radius = dof::max_blur_radius + 2;
		filter_buffer_width = width + 2 * blur_radius;
		filter_buffer_height = height + 2 * blur_radius;
	}

	//	色蓄積バッファ
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_desc.StructureByteStride = sizeof(DirectX::XMINT4);
		buffer_desc.ByteWidth = buffer_desc.StructureByteStride * filter_buffer_width * filter_buffer_height;
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Format = DXGI_FORMAT_UNKNOWN;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = filter_buffer_width * filter_buffer_height;
		// uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Microsoft::WRL::ComPtr<ID3D11Buffer> filter_buffer;
		hr = device->CreateBuffer(&buffer_desc, nullptr, filter_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateShaderResourceView(filter_buffer.Get(), nullptr, filter_buffer_srv.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(filter_buffer.Get(), nullptr, filter_buffer_uav.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<ID3D11Buffer> filter_buffer_transpose;
		hr = device->CreateBuffer(&buffer_desc, nullptr, filter_buffer_transpose.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(filter_buffer_transpose.Get(), nullptr, filter_buffer_transpose_uav.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	// シーンカラー
	{
		scene_color_rendering_pass.render_targets.clear();
		scene_color_rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
		scene_color_viewport.TopLeftX = 0.0f;
		scene_color_viewport.TopLeftY = 0.0f;
		scene_color_viewport.Width = static_cast<float>(width);
		scene_color_viewport.Height = static_cast<float>(height);
		scene_color_viewport.MinDepth = 0.0f;
		scene_color_viewport.MaxDepth = 1.0f;
	}
}

void depth_of_field_post_process_renderer::update_constant_buffer(graphics_context& context, const depth_of_field_data& dof_data, UINT buffer_width, UINT buffer_height)
{
	//	リソース類設定
	dof::parameter_constants constant;
	constant.parameter[0].x = (dof_data.focal_length);
	constant.parameter[0].y = system_units::convert_m_to_mm(dof_data.focus_distance);
	constant.parameter[0].z = (dof_data.circle_of_confusion);
	constant.parameter[0].w = (dof_data.calculate_f_number());
	constant.parameter[1].x = system_units::convert_m_to_mm(dof_data.adjust_length.x);
	constant.parameter[1].y = system_units::convert_m_to_mm(dof_data.adjust_length.y);
	constant.parameter[1].z = 0;
	constant.parameter[1].w = dof_data.display_dof_area;
	constant.parameter[2].x = 0;
	constant.parameter[2].y = 0;
	constant.parameter[2].z = static_cast<float>(std::clamp(dof_data.blur_size, 1, dof::max_blur_radius));
	constant.parameter[2].w = float(1 << dof_data.precision_scale_factor);

	constant.resolutions.x = static_cast<int>(scene_color_viewport.Width);
	constant.resolutions.y = static_cast<int>(scene_color_viewport.Height);
	constant.resolutions.z = buffer_width;
	constant.resolutions.w = buffer_height;

	context.device_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &constant, 0, 0);
}
#pragma endregion	//	DOF

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region MotionBlur
namespace motion_blur
{
	struct parameter_constants
	{
		int iteration;
		float jitter;
		float velocity_multiplyer;
		float dummy;
	};
	constexpr int cbv_index = 1;

	constexpr int tile_size = 20;

	static constexpr DirectX::XMUINT3 max_threads = {8, 8, 1};
	static constexpr DirectX::XMUINT3 blur_threads = {32, 32, 1};
}

motion_blur_post_process_renderer::motion_blur_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	コンピュートシェーダー生成関数
	if (shader_manager* instance = shader_manager::instance())
	{
		load_shader_cso desc;
		desc.file_path = "Assets/CSO/reconstract_velocity_cs.cso";
		instance->load_shader<compute_shader>(reconstract_velocity, desc);
		desc.file_path = "Assets/CSO/motion_blur_cs.cso";
		instance->load_shader<compute_shader>(motion_blur, desc);
	}

	//	定数バッファの生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;

		buffer_desc.ByteWidth = sizeof(motion_blur::parameter_constants);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

motion_blur_post_process_renderer::~motion_blur_post_process_renderer()
{
}

void motion_blur_post_process_renderer::render(renderer_context& renderer_context)
{
	//	ポストプロセスボリュームが無ければ適応しない
	DirectX::XMFLOAT3 camera_position;
	camera_position.x = renderer_context.scene_constant.camera_position.x;
	camera_position.y = renderer_context.scene_constant.camera_position.y;
	camera_position.z = renderer_context.scene_constant.camera_position.z;
	post_process_volume_ptr inner_volume = search_volume(camera_position, *renderer_context.render_data);
	if (!inner_volume)
		return;

	auto& motion_blur_constant = inner_volume->get_motion_blur_constant();
	if (!motion_blur_constant.is_active())
		return;

	job_executor.execute([this, inner_volume](graphics_context& context)
	{
		auto& motion_blur_constant = inner_volume->get_motion_blur_constant();

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		//	定数バッファ更新
		{
			motion_blur::parameter_constants constant;
			constant.iteration = motion_blur_constant.iteration;
			constant.jitter = motion_blur_constant.jitter_noise_value;
			constant.velocity_multiplyer = motion_blur_constant.velocity_multiplyer;

			context.device_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &constant, 0, 0);
		}

		//	速度バッファをタイル化して周囲の最大速度を記録
		{
			constexpr int tile_max_velocity_uav_index = 0;
			constexpr int copy_scene_uav_index = 1;

			constexpr int velocity_srv_index = 0;
			constexpr int scene_color_srv_index = 1;
			context.device_context->CSSetUnorderedAccessViews(tile_max_velocity_uav_index, 1,
				reconstract_velocity_map->get_unordered_access_view().GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(copy_scene_uav_index, 1,
				render_targets->work_buffer_scene_textures[0]->get_unordered_access_view().GetAddressOf(), nullptr);

			constant_buffer.GetAddressOf();
			context.device_context->CSSetShaderResources(velocity_srv_index, 1,
				render_targets->g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::velocity)]->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(scene_color_srv_index, 1,
				render_targets->scene_color_texture->get_shader_resource_view().GetAddressOf());

			context.device_context->CSSetShader(reconstract_velocity->get_shader().Get(), nullptr, 0);
			UINT x = (static_cast<UINT>(scene_color_viewport.Width + 0.1f) + motion_blur::tile_size - 1) / motion_blur::tile_size;
			UINT y = (static_cast<UINT>(scene_color_viewport.Height + 0.1f) + motion_blur::tile_size - 1) / motion_blur::tile_size;
			x = (x + motion_blur::max_threads.x - 1) / motion_blur::max_threads.x;
			y = (y + motion_blur::max_threads.y - 1) / motion_blur::max_threads.y;
			context.device_context->Dispatch(x, y, 1);
		}
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		//	シーンに対してモーションブラーをかける
		{
			constexpr int scene_color_uav_index = 0;

			constexpr int scene_color_srv_index = 0;
			constexpr int scene_depth_srv_index = 1;
			constexpr int velocity_srv_index = 2;
			constexpr int neigbor_max_velocity_srv_index = 3;
			context.device_context->CSSetUnorderedAccessViews(scene_color_uav_index, 1,
				render_targets->scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);

			context.device_context->CSSetShaderResources(scene_color_srv_index, 1,
				render_targets->work_buffer_scene_textures[0]->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(scene_depth_srv_index, 1,
				render_targets->scene_depth_stencil_texture->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(velocity_srv_index, 1,
				render_targets->g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::velocity)]->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(neigbor_max_velocity_srv_index, 1,
				reconstract_velocity_map->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetConstantBuffers(motion_blur::cbv_index, 1, constant_buffer.GetAddressOf());

			context.device_context->CSSetShader(motion_blur->get_shader().Get(), nullptr, 0);
			UINT x = ( render_targets->scene_color_texture->get_width() + motion_blur::blur_threads.x - 1) / motion_blur::blur_threads.x;
			UINT y = (render_targets->scene_color_texture->get_height() + motion_blur::blur_threads.y - 1) / motion_blur::blur_threads.y;
			context.device_context->Dispatch(x, y, 1);
		}

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}, "motion_blur_pass", renderer_context, get_render_basis_sort_order(), true);
}

void motion_blur_post_process_renderer::resize_buffer(int width, int height)
{
	auto device = render_data::instance()->get_device().Get();
	auto tex_manager = texture_manager::instance();
	int tile_buffer_width = (width + motion_blur::tile_size - 1) / motion_blur::tile_size;
	int tile_buffer_height = (height+ motion_blur::tile_size - 1) / motion_blur::tile_size;

	//	タイルバッファ
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = tile_buffer_width;
		texture2d_desc.Height = tile_buffer_height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		texture2d_desc.Format = DXGI_FORMAT_R32G32_FLOAT;

		reconstract_velocity_map.reset();
		tex_manager->generate_texture2d(reconstract_velocity_map, "reconstract_velocity", texture2d_desc);
	}

	// シーンカラー
	{
		scene_color_rendering_pass.render_targets.clear();
		scene_color_rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
		scene_color_viewport.TopLeftX = 0.0f;
		scene_color_viewport.TopLeftY = 0.0f;
		scene_color_viewport.Width = static_cast<float>(width);
		scene_color_viewport.Height = static_cast<float>(height);
		scene_color_viewport.MinDepth = 0.0f;
		scene_color_viewport.MaxDepth = 1.0f;
	}
}
#pragma endregion	//	MotionBlur

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region BloomAndToneMapping
namespace luminance_extract
{
	struct parameter_constants
	{
		float bloom_threshold;
		float bloom_intensity;
		float lens_ghost_threshold;
		float lens_ghost_intensity;

	    bool exposure_adjust;
		DirectX::XMFLOAT3 dummy;
	};
	constexpr int cbv_index = 0;
	constexpr int bloom_uav_index = 0;
	constexpr int lens_ghost_uav_index = 1;
	constexpr int color_srv_index = 0;
	constexpr int average_luminance_srv_index = 1;
}

namespace bloom
{
	struct fft_gaussian_constants
	{
		float deviation;
		DirectX::XMFLOAT3 dummy;
	};

	struct lens_ghost_constants
	{
		float dispersion_scale;
		float dispersion_offset;
		float ghost_mask_radius;
		float ghost_mask_fade;

		UINT iteration;
		UINT ghost_color_offset;
		float ghost_size;
		float dummy;
	};
	constexpr int lens_ghost_cbv_index = 0;

	constexpr int fft_gaussian_cbv_index = 0;

	static constexpr DirectX::XMUINT3 thread_count = {32, 32, 1};
}

namespace tone_mapping
{
	struct parameter_constants
	{
		//  露出補正処理
		DirectX::XMFLOAT4 exposure_compensation_data;  //  x : 使用フラグ, y : 適応レート, zw : 適応輝度範囲

		//  トーンマッピング
		DirectX::XMFLOAT4 tone_mapping_variable[2]; //  0.xは種類として利用、以下は場合によりけり
	};
	constexpr int cbv_index = 0;

	static constexpr DirectX::XMUINT3 max_threads = { 32, 32, 1 };
}

//	処理負荷軽減のために低解像度版でやる
#define USE_HALF_FFT
#if defined(USE_HALF_FFT)
#define FFT_TEXTURE_SIZE (fast_fourier_transform::sizes::_512)
#define FFT_TEXTURE_WIDTH (512)
#define FFT_TEXTURE_HEIGHT (512)
#else	//	defined(USE_HALF_FFT)
#define FFT_TEXTURE_SIZE (fast_fourier_transform::sizes::_1024)
#define FFT_TEXTURE_WIDTH (1024)
#define FFT_TEXTURE_HEIGHT (1024)
#endif	//	defined(USE_HALF_FFT)

bloom_and_tone_mapping_post_process_renderer::bloom_and_tone_mapping_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	高速フーリエ変換クラス
	scene_fft = std::make_unique<fast_fourier_transform>(FFT_TEXTURE_SIZE);
	bloom_kernel_fft = std::make_unique<fast_fourier_transform>(FFT_TEXTURE_SIZE);
	lens_ghost_kernel_fft = std::make_unique<fast_fourier_transform>(FFT_TEXTURE_SIZE);

	//	コンピュートシェーダー生成関数
	if (shader_manager* instance = shader_manager::instance())
	{
		load_shader_cso desc;
		desc.file_path = "Assets/CSO/correct_luminance_cs.cso";
		instance->load_shader<compute_shader>(correct_luminance, desc);
		desc.file_path = "Assets/CSO/tone_mapping_cs.cso";
		instance->load_shader<compute_shader>(tone_mapping, desc);

		desc.file_path = "Assets/CSO/fast_fourier_transform_gaussian_filter_cs.cso";
		instance->load_shader<compute_shader>(fft_gaussian_filter, desc);
		desc.file_path = "Assets/CSO/fast_fourier_transform_complex_multiply_cs.cso";
		instance->load_shader<compute_shader>(fft_multiply, desc);

		desc.file_path = "Assets/CSO/resize_copy_cs.cso";
		instance->load_shader<compute_shader>(copy_texture, desc);
		desc.file_path = "Assets/CSO/bloom_additional_half_cs.cso";
		instance->load_shader<compute_shader>(scene_additive, desc);

		desc.file_path = "Assets/CSO/luminance_extract_cs.cso";
		instance->load_shader<compute_shader>(luminance_extract, desc);

		desc.file_path = "Assets/CSO/generate_ghost_cs.cso";
		instance->load_shader<compute_shader>(generate_ghost, desc);
	}

	//	定数バッファの生成
	if (auto& device = render_data::instance()->get_device())
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;

		buffer_desc.ByteWidth = sizeof(tone_mapping::parameter_constants);
		HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, tone_mapping_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		buffer_desc.ByteWidth = sizeof(luminance_extract::parameter_constants);
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, luminance_extract_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		buffer_desc.ByteWidth = sizeof(bloom::fft_gaussian_constants);
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, fft_gaussian_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		buffer_desc.ByteWidth = sizeof(bloom::lens_ghost_constants);
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, lens_ghost_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	平均輝度テクスチャ
	if (auto tex_manager = texture_manager::instance())
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = 1;
		texture2d_desc.Height = 1;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		texture2d_desc.Format = DXGI_FORMAT_R32G32_FLOAT;

		tex_manager->generate_texture2d(average_luminance_buffer[0], "average_luminance_buffer[0]", texture2d_desc);
		tex_manager->generate_texture2d(average_luminance_buffer[1], "average_luminance_buffer[1]", texture2d_desc);
		average_luminance_buffer_index = 0;

		texture2d_desc.Width = FFT_TEXTURE_WIDTH;
		texture2d_desc.Height = FFT_TEXTURE_HEIGHT;
		texture2d_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		tex_manager->generate_texture2d(lens_ghost_luminance_map, "lens_ghost_luminance_map", texture2d_desc);
	}
}

bloom_and_tone_mapping_post_process_renderer::~bloom_and_tone_mapping_post_process_renderer()
{
}

void bloom_and_tone_mapping_post_process_renderer::render(renderer_context& renderer_context)
{
	//	ポストプロセスボリュームが無ければ適応しない
	DirectX::XMFLOAT3 camera_position;
	camera_position.x = renderer_context.scene_constant.camera_position.x;
	camera_position.y = renderer_context.scene_constant.camera_position.y;
	camera_position.z = renderer_context.scene_constant.camera_position.z;
	post_process_volume_ptr inner_volume = search_volume(camera_position, *renderer_context.render_data);
	if (!inner_volume)
		return;

	auto& tone_mapping_constant = inner_volume->get_tone_mapping_constant();
	auto& exposure_compensation_constant = inner_volume->get_exposure_compensation_constant();
	auto& bloom_constant = inner_volume->get_bloom_constant();
	auto& lens_ghosts_data = inner_volume->get_lens_ghosts_constant();

	int sort_order = get_render_basis_sort_order();
	int current_index = (average_luminance_buffer_index + 0) % 2;
	int previous_index = (average_luminance_buffer_index + 1) % 2;
	average_luminance_buffer_index++;

	//	平均輝度
	bool exposure_adjust = inner_volume->get_tone_mapping_constant().get_tone_map_type() != tone_mapping_data::tone_mapping_type::none;

	//	平均輝度算出
	{
		job_executor.execute([this, inner_volume, current_index, previous_index](graphics_context& context)
		{
			auto& tone_mapping_constant = inner_volume->get_tone_mapping_constant();
			auto& exposure_compensation_constant = inner_volume->get_exposure_compensation_constant();

			context.clear_render_target_slots();
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();

			//	定数バッファ更新
			{
				tone_mapping::parameter_constants constant;
				constant.exposure_compensation_data.x = exposure_compensation_constant.is_active() ? 1.0f : 0.0f;
				float elapsed_time = context.scene_constant.time_data.x - context.scene_constant.time_data.y;
				float elapsed_frame = system_units::convert_sec_to_fps(elapsed_time);
				constant.exposure_compensation_data.y = exposure_compensation_constant.adapt_rate * elapsed_frame;
				constant.exposure_compensation_data.z = exposure_compensation_constant.luminance_range.x;
				constant.exposure_compensation_data.w = exposure_compensation_constant.luminance_range.y;

				constant.tone_mapping_variable[0].x = static_cast<float>(tone_mapping_constant.get_tone_map_type());
				constant.tone_mapping_variable[0].y = tone_mapping_constant.tone_mapping_variables[0];
				constant.tone_mapping_variable[0].z = tone_mapping_constant.tone_mapping_variables[1];
				constant.tone_mapping_variable[0].w = tone_mapping_constant.tone_mapping_variables[2];
				constant.tone_mapping_variable[1].x = tone_mapping_constant.tone_mapping_variables[3];
				constant.tone_mapping_variable[1].y = tone_mapping_constant.tone_mapping_variables[4];
				constant.tone_mapping_variable[1].z = tone_mapping_constant.tone_mapping_variables[5];
				constant.tone_mapping_variable[1].w = tone_mapping_constant.tone_mapping_variables[6];

				context.device_context->UpdateSubresource(tone_mapping_constant_buffer.Get(), 0, 0, &constant, 0, 0);
			}

			//	平均輝度算出
			context.device_context->CSSetConstantBuffers(tone_mapping::cbv_index, 1,
				tone_mapping_constant_buffer.GetAddressOf());
			context.device_context->CSSetShaderResources(0, 1,
				render_targets->scene_color_texture->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(1, 1,
				average_luminance_buffer[previous_index]->get_shader_resource_view().GetAddressOf());

			context.device_context->CSSetUnorderedAccessViews(0, 1,
				average_luminance_buffer[current_index]->get_unordered_access_view().GetAddressOf(), nullptr);

			context.device_context->CSSetShader(correct_luminance->get_shader().Get(), nullptr, 0);
			context.device_context->Dispatch(1, 1, 1);

			context.clear_render_target_slots();
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();
		}, "average_exposure_pass", renderer_context, sort_order++, true);
	}

	//	輝度値算出
	if (bloom_constant.is_active() || lens_ghosts_data.is_active())
	{
		job_executor.execute([this, inner_volume, current_index, exposure_adjust](graphics_context& context)
		{
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();

			scene_fft->clear_uavs(context.device_context);

			auto& bloom_constant = inner_volume->get_bloom_constant();
			auto& lens_ghosts_data = inner_volume->get_lens_ghosts_constant();
			luminance_extract::parameter_constants constant;
			constant.bloom_threshold = bloom_constant.threshold;
			constant.bloom_intensity = bloom_constant.is_active() ? bloom_constant.intensity : -1.0f;
			constant.lens_ghost_threshold = lens_ghosts_data.threshold;
			constant.lens_ghost_intensity = lens_ghosts_data.is_active() ? lens_ghosts_data.intensity : -1.0f;
			constant.exposure_adjust = exposure_adjust;
			context.device_context->UpdateSubresource(luminance_extract_constant_buffer.Get(), 0, 0, &constant, 0, 0);
			context.device_context->CSSetConstantBuffers(luminance_extract::cbv_index, 1, luminance_extract_constant_buffer.GetAddressOf());

			context.device_context->CSSetUnorderedAccessViews(luminance_extract::bloom_uav_index, 1,
						scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(luminance_extract::lens_ghost_uav_index, 1,
						lens_ghost_luminance_map->get_unordered_access_view().GetAddressOf(), nullptr);

			context.device_context->CSSetShaderResources(luminance_extract::color_srv_index, 1,
						render_targets->scene_color_texture->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(luminance_extract::average_luminance_srv_index, 1,
						average_luminance_buffer[current_index]->get_shader_resource_view().GetAddressOf());

			context.device_context->CSSetShader(luminance_extract->get_shader().Get(), nullptr, 0);
			UINT x = (scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.device_context->Dispatch(x, y, 1);
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();
		}, "luminance_extract_pass", renderer_context, sort_order++, true);
	}

	//	ブルーム処理
	if (bloom_constant.is_active())
	{
		job_executor.execute([this, inner_volume, current_index](graphics_context& context)
		{
			bloom_pass(context, inner_volume, current_index);

		}, "bloom_pass", renderer_context, sort_order++, true);
	}

	//	レンズゴースト
	if (lens_ghosts_data.is_active())
	{
		job_executor.execute([this, inner_volume, current_index](graphics_context& context)
		{
			lens_ghost_pass(context, inner_volume, current_index);
		}, "lens_ghost_pass", renderer_context, sort_order++, true);
	}

	//	トーンマッピング
	if (exposure_adjust)
	{
		job_executor.execute([this, inner_volume, current_index](graphics_context& context)
		{
			auto& tone_mapping_constant = inner_volume->get_tone_mapping_constant();
			auto& exposure_compensation_constant = inner_volume->get_exposure_compensation_constant();

			context.clear_render_target_slots();
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();

			context.device_context->CSSetConstantBuffers(tone_mapping::cbv_index, 1,
				tone_mapping_constant_buffer.GetAddressOf());

			context.device_context->CSSetShaderResources(0, 1,
				average_luminance_buffer[current_index]->get_shader_resource_view().GetAddressOf());

			context.device_context->CSSetUnorderedAccessViews(0, 1,
				render_targets->scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);

			context.device_context->CSSetShader(tone_mapping->get_shader().Get(), nullptr, 0);
			UINT x = render_targets->scene_color_texture->get_width();
			UINT y = render_targets->scene_color_texture->get_height();
			x = (x + tone_mapping::max_threads.x - 1) / tone_mapping::max_threads.x;
			y = (y + tone_mapping::max_threads.y - 1) / tone_mapping::max_threads.y;
			context.device_context->Dispatch(x, y, 1);

			context.clear_render_target_slots();
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();
		}, "tone_mapping_pass", renderer_context, sort_order++, true);
	}
}

void bloom_and_tone_mapping_post_process_renderer::bloom_pass(graphics_context& context, post_process_volume_ptr inner_volume, int average_buffer_index)
{
	auto& bloom_constant = inner_volume->get_bloom_constant();

	//	スペクトル変換
	scene_fft->solve(context.device_context, fast_fourier_transform::modes::forward);

	//	ブルームの処理モード
	bloom_data::modes mode = bloom_constant.mode;
	if (!bloom_constant.image_kernel)
	{
		mode = bloom_data::modes::standard;
	}

	switch (mode)
	{
		case bloom_data::modes::standard:
		{
			//	ガウスフィルター
			bloom::fft_gaussian_constants constant;
			constant.deviation = bloom_constant.deviation;
			context.device_context->UpdateSubresource(fft_gaussian_constant_buffer.Get(), 0, 0, &constant, 0, 0);
			context.device_context->CSSetConstantBuffers(bloom::fft_gaussian_cbv_index, 1, fft_gaussian_constant_buffer.GetAddressOf());

			context.device_context->CSSetUnorderedAccessViews(0, 1, scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(1, 1, scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf(), nullptr);

			context.device_context->CSSetShader(fft_gaussian_filter->get_shader().Get(), nullptr, 0);
			UINT x = (scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.device_context->Dispatch(x, y, 1);
			break;
		}
		case bloom_data::modes::convolution:
		{
			//	カーネルをスペクトル変換
			if (bloom_image_kernel_texture != bloom_constant.image_kernel)
			{
				bloom_image_kernel_texture = bloom_constant.image_kernel;
				bloom_kernel_fft->clear_uavs(context.device_context);

				context.device_context->CSSetUnorderedAccessViews(0, 1, bloom_kernel_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
				context.device_context->CSSetShaderResources(0, 1, bloom_image_kernel_texture->get_shader_resource_view().GetAddressOf());

				context.device_context->CSSetShader(copy_texture->get_shader().Get(), nullptr, 0);
				UINT x = (bloom_kernel_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
				UINT y = (bloom_kernel_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
				context.device_context->Dispatch(x, y, 1);

				context.clear_shader_resorce_views();
				context.clear_unordered_access_views();

				bloom_kernel_fft->solve(context.device_context, fast_fourier_transform::modes::forward);
				context.clear_unordered_access_views();
			}

			context.device_context->CSSetUnorderedAccessViews(0, 1, scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(1, 1, scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf(), nullptr);

			context.device_context->CSSetShaderResources(0, 1, bloom_kernel_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(1, 1, bloom_kernel_fft->get_complex_imaginary()->get_shader_resource_view().GetAddressOf());

			context.device_context->CSSetShader(fft_multiply->get_shader().Get(), nullptr, 0);
			UINT x = (scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.device_context->Dispatch(x, y, 1);
			break;
		}
	}
	context.clear_shader_resorce_views();
	context.clear_unordered_access_views();

	//	逆変換
	scene_fft->solve(context.device_context, fast_fourier_transform::modes::inverse);

	//	加算合成
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, render_targets->scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetShaderResources(0, 1, scene_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());

		context.device_context->CSSetShader(scene_additive->get_shader().Get(), nullptr, 0);
		UINT x = (render_targets->scene_color_texture->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
		UINT y = (render_targets->scene_color_texture->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
		context.device_context->Dispatch(x, y, 1);
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	context.clear_render_target_slots();
	context.clear_shader_resorce_views();
	context.clear_unordered_access_views();
}

void bloom_and_tone_mapping_post_process_renderer::lens_ghost_pass(graphics_context& context, post_process_volume_ptr inner_volume, int average_buffer_index)
{
	auto& ghosts_constant = inner_volume->get_lens_ghosts_constant();

	//	UAVクリア
	scene_fft->clear_uavs(context.device_context);

	//	UAVにコピー
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetShaderResources(0, 1, lens_ghost_luminance_map->get_shader_resource_view().GetAddressOf());

		context.device_context->CSSetShader(copy_texture->get_shader().Get(), nullptr, 0);
		UINT x = (scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
		UINT y = (scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
		context.device_context->Dispatch(x, y, 1);

		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	//	カーネルをスペクトル変換
	if (lens_ghost_image_kernel_texture != ghosts_constant.image_kernel)
	{
		lens_ghost_image_kernel_texture = ghosts_constant.image_kernel;
		lens_ghost_kernel_fft->clear_uavs(context.device_context);

		context.device_context->CSSetUnorderedAccessViews(0, 1, lens_ghost_kernel_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetShaderResources(0, 1, lens_ghost_image_kernel_texture->get_shader_resource_view().GetAddressOf());

		context.device_context->CSSetShader(copy_texture->get_shader().Get(), nullptr, 0);
		UINT x = (lens_ghost_kernel_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
		UINT y = (lens_ghost_kernel_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
		context.device_context->Dispatch(x, y, 1);

		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
		lens_ghost_kernel_fft->solve(context.device_context, fast_fourier_transform::modes::forward);
	}

	//	ゴースト生成
	{
		context.device_context->CSSetConstantBuffers(bloom::lens_ghost_cbv_index, 1, lens_ghost_constant_buffer.GetAddressOf());
		context.device_context->CSSetShader(generate_ghost->get_shader().Get(), nullptr, 0);
		context.device_context->CSSetShaderResources(1, 1, ghosts_constant.ghost_color_lut->get_shader_resource_view().GetAddressOf());

		UINT x = (scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
		UINT y = (scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;

		bloom::lens_ghost_constants constant = {};
		constant.dispersion_scale = ghosts_constant.dispersion_scale;
		constant.dispersion_offset = ghosts_constant.dispersion_offset;
		constant.ghost_mask_radius = ghosts_constant.ghost_mask_radius;
		constant.ghost_mask_fade = ghosts_constant.ghost_mask_fade;
		constant.iteration = 4;
		constant.ghost_size = ghosts_constant.ghost_bokeh;
		constant.ghost_color_offset = 0;
		//	ピンポンブラー
		for (int index = 0; index < ghosts_constant.iteration; ++index)
		{
			constant.ghost_color_offset++;
			context.device_context->UpdateSubresource(lens_ghost_constant_buffer.Get(), 0, 0, &constant, 0, 0);

			context.device_context->CSSetUnorderedAccessViews(0, 1, scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf(), nullptr);
			context.device_context->CSSetShaderResources(0, 1, scene_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(1, 1, ghosts_constant.ghost_color_lut->get_shader_resource_view().GetAddressOf());
			context.device_context->Dispatch(x, y, 1);
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();

			context.device_context->CSSetUnorderedAccessViews(0, 1, scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
			context.device_context->CSSetShaderResources(0, 1, scene_fft->get_complex_imaginary()->get_shader_resource_view().GetAddressOf());
			context.device_context->CSSetShaderResources(1, 1, ghosts_constant.ghost_color_lut->get_shader_resource_view().GetAddressOf());
			context.device_context->Dispatch(x, y, 1);
			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();
		}
	}

	//	スペクトル変換
	scene_fft->solve(context.device_context, fast_fourier_transform::modes::forward);

	//	ゴーストサイズ調整でガウスフィルター
	if (ghosts_constant.ghost_bokeh > 1.0f)
	{
		bloom::fft_gaussian_constants constant;
		constant.deviation = std::pow(2.0f, ghosts_constant.ghost_bokeh - 1.0f);
		context.device_context->UpdateSubresource(fft_gaussian_constant_buffer.Get(), 0, 0, &constant, 0, 0);
		context.device_context->CSSetConstantBuffers(bloom::fft_gaussian_cbv_index, 1, fft_gaussian_constant_buffer.GetAddressOf());

		context.device_context->CSSetUnorderedAccessViews(0, 1, scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetUnorderedAccessViews(1, 1, scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf(), nullptr);

		context.device_context->CSSetShader(fft_gaussian_filter->get_shader().Get(), nullptr, 0);
		UINT x = (scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
		UINT y = (scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
		context.device_context->Dispatch(x, y, 1);

		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	//	イメージカーネル適応
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetUnorderedAccessViews(1, 1, scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf(), nullptr);

		context.device_context->CSSetShaderResources(0, 1, lens_ghost_kernel_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());
		context.device_context->CSSetShaderResources(1, 1, lens_ghost_kernel_fft->get_complex_imaginary()->get_shader_resource_view().GetAddressOf());

		context.device_context->CSSetShader(fft_multiply->get_shader().Get(), nullptr, 0);
		UINT x = (scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
		UINT y = (scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
		context.device_context->Dispatch(x, y, 1);
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	//	逆変換
	scene_fft->solve(context.device_context, fast_fourier_transform::modes::inverse);

	//	加算合成
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, render_targets->scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetShaderResources(0, 1, scene_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());

		context.device_context->CSSetShader(scene_additive->get_shader().Get(), nullptr, 0);
		UINT x = (render_targets->scene_color_texture->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
		UINT y = (render_targets->scene_color_texture->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
		context.device_context->Dispatch(x, y, 1);
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	context.clear_render_target_slots();
	context.clear_shader_resorce_views();
	context.clear_unordered_access_views();
}

void bloom_and_tone_mapping_post_process_renderer::resize_buffer(int width, int height)
{
}
#pragma endregion	//	BloomAndToneMapping

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region Antialias
namespace antialias
{
	constexpr int scene_color_srv_index = 0;
	constexpr int scene_depth_srv_index = 1;
	constexpr int sat_color_srv_index = 2;

	static constexpr DirectX::XMUINT3 thread_count = { 16, 9, 1 };
}

anti_alias_post_process_renderer::anti_alias_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	パイプラインステート生成
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		//	FXAA
		instance->generate_pipeline_state<fxaa_pipeline_state>("fxaa", &fxaa);
		//	SMAA
		instance->generate_pipeline_state<smaa_edge_detection_pipeline_state>("smaa_edge_detection", &smaa_edge_detection);
		instance->generate_pipeline_state<smaa_blending_weight_calculation_pipeline_state>("smaa_blending_weight_calculation", &smaa_blending_weight_calculation);
		instance->generate_pipeline_state<smaa_neighborhood_blending_pipeline_state>("smaa_neighborhood_blending", &smaa_neighborhood_blending);
		instance->generate_pipeline_state<write_back_pipeline_state>("write_back", &write_back);
	}
}

anti_alias_post_process_renderer::~anti_alias_post_process_renderer()
{
}

void anti_alias_post_process_renderer::render(renderer_context& renderer_context)
{
	switch (engine_config_data::instance()->antialias.mode)
	{
		case engine_config_data::antialias_data::modes::FXAA:
		{
			job_executor.execute([this](graphics_context& context)
			{
				{
					work_color_rendering_pass.activate(context.device_context);
					context.device_context->RSSetViewports(1, &work_color_viewport);

					context.scene_color_texture = render_targets->scene_color_texture;

					fxaa->begin(context);
					draw(context, fxaa, 4);
					fxaa->end(context);

					work_color_rendering_pass.deactivate(context.device_context);
				}

				context.clear_render_target_slots();
				context.clear_shader_resorce_views();

				//	シーンへ合成
				{
					scene_color_rendering_pass.activate(context.device_context);
					context.device_context->RSSetViewports(1, &scene_color_viewport);

					context.scene_color_texture = render_targets->work_buffer_scene_textures[0];
					context.work_texture[0] = smaa_blend_buffer;
					context.work_texture[1] = smaa_edge_buffer;

					write_back->begin(context);
					draw(context, write_back, 4);
					write_back->end(context);

					scene_color_rendering_pass.deactivate(context.device_context);
				}

				context.clear_render_target_slots();
				context.clear_shader_resorce_views();
				context.clear_shader_resorce_views();
				context.clear_unordered_access_views();
			}, "anti_alias_pass", renderer_context, get_render_basis_sort_order());
			break;
		}
		case engine_config_data::antialias_data::modes::SMAA:
		{
			job_executor.execute([this](graphics_context& context)
			{
				//	エッジ検出
				{
					smaa_edge_buffer_rendering_pass.activate(context.device_context);
					context.device_context->RSSetViewports(1, &smaa_edge_buffer_viewport);

					context.scene_color_texture = render_targets->scene_color_texture;

					smaa_edge_detection->begin(context);
					draw(context, smaa_edge_detection, 4);
					smaa_edge_detection->end(context);

					smaa_edge_buffer_rendering_pass.deactivate(context.device_context);
				}

				context.clear_render_target_slots();
				context.clear_shader_resorce_views();

				//	ブレンドウェイト算出
				{
					smaa_blend_buffer_rendering_pass.activate(context.device_context);
					context.device_context->RSSetViewports(1, &smaa_blend_buffer_viewport);

					context.scene_color_texture = smaa_edge_buffer;

					smaa_blending_weight_calculation->begin(context);
					draw(context, smaa_blending_weight_calculation, 4);
					smaa_blending_weight_calculation->end(context);

					smaa_blend_buffer_rendering_pass.deactivate(context.device_context);
				}

				context.clear_render_target_slots();
				context.clear_shader_resorce_views();

				//	合成
				{
					work_color_rendering_pass.activate(context.device_context);
					context.device_context->RSSetViewports(1, &work_color_viewport);

					context.scene_color_texture = render_targets->scene_color_texture;
					context.work_texture[0] = smaa_blend_buffer;
					context.work_texture[1] = smaa_edge_buffer;

					smaa_neighborhood_blending->begin(context);
					draw(context, smaa_neighborhood_blending, 4);
					smaa_neighborhood_blending->end(context);

					work_color_rendering_pass.deactivate(context.device_context);
				}
				;

				context.clear_render_target_slots();
				context.clear_shader_resorce_views();

				//	シーンへ合成
				{
					scene_color_rendering_pass.activate(context.device_context);
					context.device_context->RSSetViewports(1, &scene_color_viewport);

					context.scene_color_texture = render_targets->work_buffer_scene_textures[0];
					context.work_texture[0] = smaa_blend_buffer;
					context.work_texture[1] = smaa_edge_buffer;

					write_back->begin(context);
					draw(context, write_back, 4);
					write_back->end(context);

					scene_color_rendering_pass.deactivate(context.device_context);
				}

				context.clear_render_target_slots();
				context.clear_shader_resorce_views();
				context.clear_shader_resorce_views();
				context.clear_unordered_access_views();
			}, "anti_alias_pass", renderer_context, get_render_basis_sort_order());
			break;
		}
	}
}

void anti_alias_post_process_renderer::resize_buffer(int width, int height)
{
	// シーンカラー
	{
		scene_color_rendering_pass.render_targets.clear();
		scene_color_rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
		scene_color_viewport.TopLeftX = 0.0f;
		scene_color_viewport.TopLeftY = 0.0f;
		scene_color_viewport.Width = static_cast<float>(width);
		scene_color_viewport.Height = static_cast<float>(height);
		scene_color_viewport.MinDepth = 0.0f;
		scene_color_viewport.MaxDepth = 1.0f;
	}
	// シーンカラー
	{
		work_color_rendering_pass.render_targets.clear();
		work_color_rendering_pass.render_targets.emplace_back(render_targets->work_buffer_scene_textures[0]);
		work_color_viewport.TopLeftX = 0.0f;
		work_color_viewport.TopLeftY = 0.0f;
		work_color_viewport.Width = static_cast<float>(width);
		work_color_viewport.Height = static_cast<float>(height);
		work_color_viewport.MinDepth = 0.0f;
		work_color_viewport.MaxDepth = 1.0f;
	}

	//	SMAA用バッファ
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		//	Edge
		{
			texture2d_desc.Format = DXGI_FORMAT_R16G16_FLOAT;
			texture_manager::instance()->generate_texture2d(smaa_edge_buffer, "smaa_edge_buffer", texture2d_desc);
			smaa_edge_buffer_rendering_pass.render_targets.clear();
			smaa_edge_buffer_rendering_pass.render_targets.emplace_back(smaa_edge_buffer);
			smaa_edge_buffer_viewport.TopLeftX = 0.0f;
			smaa_edge_buffer_viewport.TopLeftY = 0.0f;
			smaa_edge_buffer_viewport.Width = static_cast<float>(width);
			smaa_edge_buffer_viewport.Height = static_cast<float>(height);
			smaa_edge_buffer_viewport.MinDepth = 0.0f;
			smaa_edge_buffer_viewport.MaxDepth = 1.0f;
		}
		//	BlendWeight
		{
			texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			texture_manager::instance()->generate_texture2d(smaa_blend_buffer, "smaa_blend_buffer", texture2d_desc);
			smaa_blend_buffer_rendering_pass.render_targets.clear();
			smaa_blend_buffer_rendering_pass.render_targets.emplace_back(smaa_blend_buffer);
			smaa_blend_buffer_viewport.TopLeftX = 0.0f;
			smaa_blend_buffer_viewport.TopLeftY = 0.0f;
			smaa_blend_buffer_viewport.Width = static_cast<float>(width);
			smaa_blend_buffer_viewport.Height = static_cast<float>(height);
			smaa_blend_buffer_viewport.MinDepth = 0.0f;
			smaa_blend_buffer_viewport.MaxDepth = 1.0f;
		}
	}
}
#pragma endregion	//	AntiAlias

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region FinalPass
finalpass_renderer::finalpass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	resize_buffer(0, 0);

	//	パイプラインステート生成
	pipeline_state_manager::instance()->generate_pipeline_state<convert_linear_to_srgb_pipeline_state>("convert_linear_to_srgb", &convert_linear_to_srgb_pipeline_states);
	convert_linear_to_srgb_pipeline_states->set_domain(pipeline_state_base::domains::system);
}

finalpass_renderer::~finalpass_renderer()
{
}

void finalpass_renderer::render(renderer_context& renderer_context)
{
	job_executor.execute([this](graphics_context& context)
	{
		rendering_pass.activate(context.device_context);
		context.device_context->RSSetViewports(1, &viewport);
		context.scene_color_texture = render_targets->scene_color_texture;

		convert_linear_to_srgb_pipeline_states->begin(context);
		draw(context, convert_linear_to_srgb_pipeline_states, 4);
		convert_linear_to_srgb_pipeline_states->end(context);

		rendering_pass.deactivate(context.device_context);
	}, "final_pass", renderer_context, get_render_basis_sort_order());
}

void finalpass_renderer::resize_buffer(int width, int height)
{
	rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());

	DXGI_SWAP_CHAIN_DESC desc;
	render_data::instance()->get_swap_chain()->GetDesc(&desc);

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(desc.BufferDesc.Width);
	viewport.Height = static_cast<float>(desc.BufferDesc.Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}
#pragma endregion	//	FinalPass
