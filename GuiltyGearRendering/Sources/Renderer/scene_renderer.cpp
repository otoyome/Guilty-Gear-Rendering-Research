#include "Renderer/scene_renderer.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/deferred_lighting_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"

#include "PipelineState/post_process_pipeline_state.h"

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#endif // defined(USE_IMGUI)

namespace
{
	float convert_linear_depth_to_projection_depth(float depth, float z_buffer_parameteres_y)
	{
		float z_buffer_parameteres_x = 1.0f - z_buffer_parameteres_y;
		return (1.0f / depth - z_buffer_parameteres_y) / z_buffer_parameteres_x;
	};

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

	void draw_indexed(const graphics_context& context,
					  float mesh_length,
					  const buffer_datas& buffer_data,
					  graphics_pipeline_state_ptr pipeline_state)
	{
		const std::unordered_map<semantic, buffer_view>* vertex_buffer_views = nullptr;
		const buffer_view* index_buffer_view = nullptr;
		D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		if (context.is_shadow_rendering)
		{
			vertex_buffer_views = buffer_data.shadow_mesh.vertex_buffer_views;
			index_buffer_view = buffer_data.shadow_mesh.index_buffer_view;
			primitive_topology = buffer_data.shadow_mesh.primitive_topology;
		}
		else
		{
			//	LOD対応
			UINT lod_level = buffer_data.color_meshs.size();
			float lod_range_min = buffer_data.lod_range_rate.x * context.scene_constant.camera_clip_distance.x;
			float lod_range_max = buffer_data.lod_range_rate.y * context.scene_constant.camera_clip_distance.y;
			float lod_rate = math::saturate((mesh_length - lod_range_min) / (lod_range_max - lod_range_min));
			auto lod_mesh_index = math::lerp(0u, lod_level - 1, lod_rate);
			auto* draw_mesh = &buffer_data.color_meshs[lod_mesh_index];
			vertex_buffer_views = draw_mesh->vertex_buffer_views;
			index_buffer_view = draw_mesh->index_buffer_view;
			primitive_topology = draw_mesh->primitive_topology;
		}

		//	頂点ストリーム更新
		UINT stream_index = 0;
		for (auto& semantic : pipeline_state->get_semantic_list())
		{
			auto it = vertex_buffer_views->find(semantic.first);
			if (it != vertex_buffer_views->end())
			{
				auto& buffer_data = it->second;
				UINT offset = static_cast<UINT>(buffer_data.offset());
				UINT stride = static_cast<UINT>(buffer_data.stride_in_bytes);
				context.device_context->IASetVertexBuffers(stream_index, 1, buffer_data.buffer.GetAddressOf(), &stride, &offset);
			}
			else
			{
				//	空のオブジェクトを納入しておく
				UINT offset = 0;
				UINT stride = 0;
				ID3D11Buffer* buffer = nullptr;
				context.device_context->IASetVertexBuffers(stream_index, 1, &buffer, &stride, &offset);
			}
			++stream_index;
		}

		context.device_context->IASetIndexBuffer(index_buffer_view->buffer.Get(), index_buffer_view->format,
													static_cast<UINT>(index_buffer_view->offset()));
		context.device_context->IASetPrimitiveTopology(primitive_topology);

		context.device_context->DrawIndexed(static_cast<UINT>(index_buffer_view->count()), 0, 0);

	}

	void model_render(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const std::vector<float>& mesh_lengths,
					  material::surface_paths surface_path)
	{
		auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
		auto material_data_ptr = context.render_data->mesh_container.material_datas.data();
		auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

		//	モデル類描画
		graphics_pipeline_state_ptr current_pipeline_state;
		for (auto& draw_index : mesh_indices)
		{
			auto draw_data = draw_data_ptr + draw_index;
			auto material_data = material_data_ptr + draw_data->material_index;

			if (auto pipeline_state = material_data->material->get_pipeline_state(surface_path))
			{
				if (pipeline_state != current_pipeline_state)
				{
					if (current_pipeline_state)
						current_pipeline_state->end(context);

					current_pipeline_state = pipeline_state;
					current_pipeline_state->begin(context);
				}

				auto buffer_data = buffer_data_ptr + draw_data->buffer_index;

				material_data->material->update_parameters(context);
				current_pipeline_state->update_parameters(context, *material_data->material, buffer_data);
				draw_indexed(context, mesh_lengths[draw_index], *buffer_data, current_pipeline_state);
			}
		}
		if (current_pipeline_state)
			current_pipeline_state->end(context);
	}

	void model_render(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const collision::frustum_culling& frustum_culling,
					  material::surface_paths surface_path)
	{
		auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
		auto material_data_ptr = context.render_data->mesh_container.material_datas.data();
		auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

		//	モデル類描画
		graphics_pipeline_state_ptr current_pipeline_state;
		for (auto& draw_index : mesh_indices)
		{
			auto draw_data = draw_data_ptr + draw_index;
			auto material_data = material_data_ptr + draw_data->material_index;

			//	AABB判定
			auto buffer_data = buffer_data_ptr + draw_data->buffer_index;
			if (!frustum_culling.to_aabb(buffer_data->bounds_min, buffer_data->bounds_max))
				continue;

			if (auto pipeline_state = material_data->material->get_pipeline_state(surface_path))
			{
				if (pipeline_state != current_pipeline_state)
				{
					if (current_pipeline_state)
						current_pipeline_state->end(context);

					current_pipeline_state = pipeline_state;
					current_pipeline_state->begin(context);
				}

				material_data->material->update_parameters(context);
				current_pipeline_state->update_parameters(context, *material_data->material, buffer_data);
				draw_indexed(context, 0.0f, *buffer_data, current_pipeline_state);
			}
		}
		if (current_pipeline_state)
			current_pipeline_state->end(context);
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
depth_prepass_renderer::depth_prepass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
}

depth_prepass_renderer::~depth_prepass_renderer()
{
}

void depth_prepass_renderer::render(renderer_context& renderer_context)
{
	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass);
		model_render(context, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass);

		rendering_pass.deactivate(context.device_context);
	}, "depth_pre_pass", renderer_context, get_render_basis_sort_order());
}

void depth_prepass_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	描画パス生成
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gbuffer_renderer::gbuffer_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	シェーダー準備
	pipeline_state_manager::instance()->generate_pipeline_state<skybox_pipeline_state>("skybox", &skybox_pipeline_states);
	skybox_pipeline_states->set_domain(pipeline_state_base::domains::system);
}

gbuffer_renderer::~gbuffer_renderer()
{
}

void gbuffer_renderer::render(renderer_context& renderer_context)
{
	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass);
		model_render(context, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass);

		//	スカイライト処理
		for (const auto& data : context.render_data->light_container.light_probes)
		{
			const light_probe* light_probe = data.get();
			if (!light_probe->is_flag(light_probe::flags::active) || !light_probe->is_flag(light_probe::flags::unbind))
			{
				continue;
			}

			//	IBLテクスチャ設定
			context.diffuse_ibl_texture = light_probe->get_diffuse_ibl_texture();
			context.specluar_ibl_texture = light_probe->get_specular_ibl_texture();
			context.lut_ggx_texture = light_probe->get_lut_ggx_texture();

			//	間接光の情報を設定
			light_probes probe;
			probe.position.x = light_probe->get_position().x;
			probe.position.y = light_probe->get_position().y;
			probe.position.z = light_probe->get_position().z;
			probe.inner_area.x = light_probe->get_inner_range().x;
			probe.inner_area.y = light_probe->get_inner_range().y;
			probe.inner_area.z = light_probe->get_inner_range().z;
			probe.outer_area.x = light_probe->get_outer_range().x;
			probe.outer_area.y = light_probe->get_outer_range().y;
			probe.outer_area.z = light_probe->get_outer_range().z;
			probe.color = light_probe->get_color();
			context.deferred_light_constant = {};
			context.deferred_light_constant.light_data = light_data_utility::convert_light_data(probe);
			context.deferred_light_constant.light_attribute = light_probe->get_attribute();

			//	全画面描画
			skybox_pipeline_states->begin(context);
			draw(context, skybox_pipeline_states, 4);
			skybox_pipeline_states->end(context);
			break;
		}

		rendering_pass.deactivate(context.device_context);
	}, "gbuffer_pass", renderer_context, get_render_basis_sort_order());
}

void gbuffer_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	GBufferパス生成
	{
		FLOAT clear_color[][4] =
		{
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	base_color
			{ 0.0f, 0.0f, 0.0f, 1.0f },	//	normal_depth
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	parameter
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	velocity
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	unique_data
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	shade_color [エンジン改造:G-Buffer]
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	ilm_color [エンジン改造:G-Buffer]
		};
		rendering_pass.render_targets.clear();
		for (int i = 0; i < static_cast<int>(common_pipeline_states::gbuffer_id::max); ++i)
		{
			rendering_pass.render_targets.emplace_back(render_targets->g_buffer_textures[i]);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color_value[0] = clear_color[i][0];
			rtv.clear_color_value[1] = clear_color[i][1];
			rtv.clear_color_value[2] = clear_color[i][2];
			rtv.clear_color_value[3] = clear_color[i][3];
		}
		{
			//	emissive用にシーンバッファ追加
			rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color_value[0] = 0.0f;
			rtv.clear_color_value[1] = 0.0f;
			rtv.clear_color_value[2] = 0.0f;
			rtv.clear_color_value[3] = 0.0f;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_view_desc.Texture2D.MipSlice = 0;
		rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
		rendering_pass.depth_stencil.clear_depth = false;
	}

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
deffered_lighting_renderer::deffered_lighting_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, lighting_pass_viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
, shadow_map_pass_viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
	//	シャドウマップ生成
	if (texture = std::static_pointer_cast<texture2d>(render_targets->shadowmap_depth_texture))
	{
		resize_shadow_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	ディファードレンダリング系シェーダー準備
	{
		static constexpr char* shader_names[] =
		{
			"deferred_indirect_lighting",
			"deferred_directional_lighting",
			"deferred_point_lighting",
			"deferred_spot_lighting",
		};
		//	
#if defined(USE_CUSTOM_PIPELINE)
		deferred_lighting_material = std::make_shared<material>();
		deferred_lighting_material->set_domain(material::domains::system);
		deferred_lighting_material->set_flag(material::flags::two_sided, true);
		deferred_lighting_material->set_blend_mode(material::blend_modes::addative);	//	加算合成

		static constexpr std::string_view vs_path_list[] =
		{
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
		};
		static constexpr std::string_view ps_path_list[] =
		{
			"Shaders/DeferredRendering/deferred_rendering_indirect_ps.hlsl",
			"Shaders/DeferredRendering/deferred_rendering_directional_ps.hlsl",
			"Shaders/DeferredRendering/deferred_rendering_point_ps.hlsl",
			"Shaders/DeferredRendering/deferred_rendering_spot_ps.hlsl",
		};
		static constexpr material::system_paths system_paths_list[] =
		{
			material::system_paths::deferred_indirect,
			material::system_paths::deferred_directional,
			material::system_paths::deferred_point,
			material::system_paths::deferred_spot,
		};
			
		//	deferred_indirect_lighting
		for (UINT index = 0; index < ARRAYSIZE(shader_names); ++ index)
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>(shader_names[index], &pso);
			pso->set_domain(pipeline_state_base::domains::system);
			pso->set_flag(pipeline_state_base::flags::hidden, false);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = vs_path_list[index];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = ps_path_list[index];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
			deferred_lighting_material->set_pipeline_state(system_paths_list[index], shader_names[index]);
		}
#else
		instance->generate_pipeline_state<deferred_indirect_lighting_pipeline_state>(shader_names[0], &indirect_lighting_pipeline_state);
		instance->generate_pipeline_state<deferred_directional_lighting_pipeline_state>(shader_names[1], &directional_lighting_pipeline_state);
		instance->generate_pipeline_state<deferred_point_lighting_pipeline_state>(shader_names[2], &point_lighting_pipeline_state);
		instance->generate_pipeline_state<deferred_spot_lighting_pipeline_state>(shader_names[3], &spot_lighting_pipeline_state);
#endif
	}
}

deffered_lighting_renderer::~deffered_lighting_renderer()
{
}

void deffered_lighting_renderer::render(renderer_context& context)
{
	//	描画番号
	register_index = 0;

	//	間接光
	render_indirect(context);

	//	ライティング処理
	for (const auto& data : context.render_data->light_container.lights)
	{
		const light* light = data.get();
		if (!light->is_flag(light::flags::active))
			continue;

		switch (light->get_light_type())
		{
			case light_type::directional:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant;
				if (light->is_flag(light::flags::use_shadow))
				{
					render_directional_shadow_map(*light, shadow_map_constant, context);
				}

				//	ライティング処理
				render_directional(*light, shadow_map_constant, context);
				break;
			}
			case light_type::point:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant;
				if (light->is_flag(light::flags::use_shadow))
				{
					render_point_shadow_map(*light, shadow_map_constant, context);
				}

				//	ライティング処理
				render_point(*light, shadow_map_constant, context);
				break;
			}
			case light_type::spot:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant;
				if (light->is_flag(light::flags::use_shadow))
				{
					render_spot_shadow_map(*light, shadow_map_constant, context);
				}

				//	ライティング処理
				render_spot(*light, shadow_map_constant, context);
				break;
			}
		}
	}
}

void deffered_lighting_renderer::render_indirect(renderer_context& context)
{
	job_executor.execute([this](graphics_context& context)
	{
		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	AOバッファ生成
		{
		}

		//	描画先設定
		lighting_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);

		//	間接光
		for (const auto& data : context.render_data->light_container.light_probes)
		{
			const light_probe* light_probe = data.get();
			if (!light_probe->is_flag(light_probe::flags::active))
				continue;

			//	間接光の情報を設定
			light_probes probe;
			probe.position.x = light_probe->get_position().x;
			probe.position.y = light_probe->get_position().y;
			probe.position.z = light_probe->get_position().z;
			probe.inner_area.x = light_probe->get_inner_range().x;
			probe.inner_area.y = light_probe->get_inner_range().y;
			probe.inner_area.z = light_probe->get_inner_range().z;
			probe.outer_area.x = light_probe->get_outer_range().x;
			probe.outer_area.y = light_probe->get_outer_range().y;
			probe.outer_area.z = light_probe->get_outer_range().z;
			probe.color = light_probe->get_color();
			context.deferred_light_constant = {};
			context.deferred_light_constant.light_data = light_data_utility::convert_light_data(probe);
			context.deferred_light_constant.light_attribute = light_probe->get_attribute();

			if (light_probe->is_flag(light_probe::flags::unbind))
			{
				//	全画面描画
#if defined(USE_CUSTOM_PIPELINE)
				//	IBLテクスチャ設定
				deferred_lighting_material->set_texture("diffuse_iem", light_probe->get_diffuse_ibl_texture());
				deferred_lighting_material->set_texture("specular_pmrem", light_probe->get_specular_ibl_texture());
				deferred_lighting_material->set_texture("lut_ggx", light_probe->get_lut_ggx_texture());

				auto pipeline_state =
					deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_indirect);

				pipeline_state->begin(context);
				pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
				draw(context, pipeline_state, 4);
				pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
				//	IBLテクスチャ設定
				context.diffuse_ibl_texture = light_probe->get_diffuse_ibl_texture();
				context.specluar_ibl_texture = light_probe->get_specular_ibl_texture();
				context.lut_ggx_texture = light_probe->get_lut_ggx_texture();

				indirect_lighting_pipeline_state->begin(context);
				draw(context, indirect_lighting_pipeline_state, 4);
				indirect_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
			}
			else
			{
				//	範囲制限をかける
			}
		}

		lighting_pass.deactivate(context.device_context);
	}, "render_indirect_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_directional(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this, light, shadow_map_constant](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		//	コンテキストに情報登録
		context.deferred_light_constant.light_data = light.convert_light_data();
		context.deferred_light_constant.light_attribute = light.get_attribute();
		context.shadow_map_texture = render_targets->shadowmap_depth_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
#if defined(USE_CUSTOM_PIPELINE)
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_directional);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			draw(context, pipeline_state, 4);
			pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
			directional_lighting_pipeline_state->begin(context);
			draw(context, directional_lighting_pipeline_state, 4);
			directional_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_directional_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_directional_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	DirectX::XMFLOAT4 rotation = light.get_rotation().convert_quaternion();
	DirectX::XMFLOAT3 position = light.get_position();
	DirectX::XMMATRIX CameraTransformMatrix = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&context.scene_constant.view_transform));
	DirectX::XMMATRIX LightTransformMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)) * 
												DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMMATRIX InverseProjectionTransform;
	InverseProjectionTransform = DirectX::XMLoadFloat4x4(&context.scene_constant.inverse_projection_transform);

	DirectX::XMVECTOR LightRight, LightUp, LightFront, LightPosition;
	LightRight = LightTransformMatrix.r[0];
	LightUp = LightTransformMatrix.r[1];
	LightFront = LightTransformMatrix.r[2];
	LightPosition = LightTransformMatrix.r[3];

	//	クリップ距離を算出
	float near_clip, far_clip;
	{
		DirectX::XMVECTOR NearPosition, FarPosition;
		NearPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), InverseProjectionTransform);
		FarPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), InverseProjectionTransform);
		near_clip = DirectX::XMVectorGetZ(NearPosition);
		far_clip = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(FarPosition, NearPosition)));
	}

	//	CSM用に遠クリップ面に対して距離制限をかける
	float projection_near_clip, projection_far_clip;
	{
		projection_near_clip = 0.0f;
		projection_far_clip = convert_linear_depth_to_projection_depth(light.get_csm_distance_factor(), far_clip / near_clip);

		//	クリップ距離を再算出
		DirectX::XMVECTOR NearPosition, FarPosition;
		NearPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, projection_near_clip, 0.0f), InverseProjectionTransform);
		FarPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, projection_far_clip, 0.0f), InverseProjectionTransform);
		far_clip = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(FarPosition, NearPosition)));
	}

	//	シャドウマップ情報算出
	shadow_map_constant.shadow_map_count = directional_shadow_map_count;
	shadow_map_constant.shadow_attenuation = light.get_shadow_attenuation();
	shadow_map_constant.shadow_mask = 0;
	collision::frustum_culling frustum_culling;
	{
		DirectX::XMMATRIX InverseViewProjectionTransform;
		InverseViewProjectionTransform = DirectX::XMLoadFloat4x4(&context.scene_constant.inverse_view_projection_transform);
		float top_min_distance, top_max_distance;
		//	視錐台を囲むライト空間上の行列を生成
		DirectX::XMMATRIX LightViewMatrix, LightProjectionMatrix;
		{
			//	AABBを囲む球体を算出
			DirectX::XMVECTOR Center;
			float radius;
			{
				DirectX::XMVECTOR FrustumPoints[] =
				{
					DirectX::XMVectorSet(-1.0f, -1.0f, projection_near_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, -1.0f, projection_near_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, +1.0f, projection_near_clip, 1.0f),
					DirectX::XMVectorSet(-1.0f, +1.0f, projection_near_clip, 1.0f),

					DirectX::XMVectorSet(-1.0f, -1.0f, projection_far_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, -1.0f, projection_far_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, +1.0f, projection_far_clip, 1.0f),
					DirectX::XMVectorSet(-1.0f, +1.0f, projection_far_clip, 1.0f),
				};

				for (auto& Position : FrustumPoints)
				{
					Position = DirectX::XMVector3TransformCoord(Position, InverseViewProjectionTransform);
				}
				DirectX::XMVECTOR BoundsMin = DirectX::XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 0),
					BoundsMax = DirectX::XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0);
				for (auto& Position : FrustumPoints)
				{
					BoundsMin = DirectX::XMVectorMin(BoundsMin, Position);
					BoundsMax = DirectX::XMVectorMax(BoundsMax, Position);
				}
				DirectX::XMVECTOR Vector = DirectX::XMVectorScale(DirectX::XMVectorSubtract(BoundsMax, BoundsMin), 0.5f);
				Center = DirectX::XMVectorAdd(BoundsMin, Vector);
				radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
			}

			//	視線行列算出
			{
				LightPosition = DirectX::XMVectorSubtract(Center, DirectX::XMVectorScale(LightFront, radius));
				LightTransformMatrix.r[0] = LightRight;
				LightTransformMatrix.r[1] = LightUp;
				LightTransformMatrix.r[2] = LightFront;
				LightTransformMatrix.r[3] = LightPosition;
				LightTransformMatrix.r[0] = DirectX::XMVectorSetW(LightTransformMatrix.r[0], 0.0f);
				LightTransformMatrix.r[1] = DirectX::XMVectorSetW(LightTransformMatrix.r[1], 0.0f);
				LightTransformMatrix.r[2] = DirectX::XMVectorSetW(LightTransformMatrix.r[2], 0.0f);
				LightTransformMatrix.r[3] = DirectX::XMVectorSetW(LightTransformMatrix.r[3], 1.0f);
				LightViewMatrix = DirectX::XMMatrixInverse(nullptr, LightTransformMatrix);
			}

			//	投影行列生成
			{
				radius *= 1.01f;
				top_min_distance = 0.0f;
				top_max_distance = top_min_distance + radius * 2.0f;
				LightProjectionMatrix = DirectX::XMMatrixOrthographicOffCenterLH(-radius, +radius, -radius, +radius, top_min_distance, top_max_distance);
			}

			//	フラスタムカリング生成
			{
				DirectX::XMFLOAT4X4 frustum_culling_light_view_projection_transform;
				DirectX::XMStoreFloat4x4(&frustum_culling_light_view_projection_transform,
										  LightViewMatrix * LightProjectionMatrix);
				frustum_culling.create_planes(frustum_culling_light_view_projection_transform);
				frustum_culling.create_frustum_corners(DirectX::XMMatrixInverse(nullptr,
														DirectX::XMLoadFloat4x4(&frustum_culling_light_view_projection_transform)));
			}
		}

		//	PSSM(面白分割距離算出)
		std::vector<float> split_distance;
		split_distance.resize(directional_shadow_map_count + 1);
		{
			float split_weight = light.get_pssm_split_weight();
			float weight = math::lerp(0.1f, 0.9f, split_weight);
			float divN = far_clip / near_clip, subN = far_clip - near_clip;

			//	距離算出
			UINT index = 0;
			split_distance[index]	= near_clip;
			for (++index; index < directional_shadow_map_count; ++index)
			{
				float spliit = static_cast<float>(index) / static_cast<float>(directional_shadow_map_count);
				float fLog = near_clip * pow(divN, spliit);
				float fUni = near_clip + subN * spliit;
				split_distance[index]	= fLog * weight + fUni * ( 1.0f - weight);
			}
			split_distance[index]	= far_clip;
		}

		//	視錐台を距離で分割してライトビュープロジェクション行列を生成
		for (UINT index = 0; index < directional_shadow_map_count; ++index)
		{
			//	AABBを囲む球体を算出
			DirectX::XMVECTOR Center;
			float radius;
			{
				//	分割距離単位での視錐台生成
				float n_clip, f_clip;
				n_clip = convert_linear_depth_to_projection_depth(split_distance[index + 0] / far_clip, far_clip / near_clip);
				f_clip = convert_linear_depth_to_projection_depth(split_distance[index + 1] / far_clip, far_clip / near_clip);

				DirectX::XMVECTOR FrustumPoints[8] =
				{
					DirectX::XMVectorSet(-1.0f, -1.0f, n_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, -1.0f, n_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, +1.0f, n_clip, 1.0f),
					DirectX::XMVectorSet(-1.0f, +1.0f, n_clip, 1.0f),

					DirectX::XMVectorSet(-1.0f, -1.0f, f_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, -1.0f, f_clip, 1.0f),
					DirectX::XMVectorSet(+1.0f, +1.0f, f_clip, 1.0f),
					DirectX::XMVectorSet(-1.0f, +1.0f, f_clip, 1.0f),
				};

				for (auto& Position : FrustumPoints)
				{
					Position = DirectX::XMVector3TransformCoord(Position, InverseViewProjectionTransform);
				}

				DirectX::XMVECTOR	BoundsMin = DirectX::XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 0),
									BoundsMax = DirectX::XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0);
				for (auto& Position : FrustumPoints)
				{
					BoundsMin = DirectX::XMVectorMin(BoundsMin, Position);
					BoundsMax = DirectX::XMVectorMax(BoundsMax, Position);
				}
				DirectX::XMVECTOR Vector = DirectX::XMVectorScale(DirectX::XMVectorSubtract(BoundsMax, BoundsMin), 0.5f);
				Center = DirectX::XMVectorAdd(BoundsMin, Vector);
				radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
			}

			//	各行列調整
			{
				float min_distance = top_min_distance;
				float max_distance = top_max_distance;

				float X = static_cast<float>(static_cast<int>(ceilf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(Center, LightRight)) * shadow_map_pass_viewport.Width / radius ) * radius / shadow_map_pass_viewport.Width ) );
				float Y = static_cast<float>(static_cast<int>(ceilf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(Center,    LightUp)) * shadow_map_pass_viewport.Height / radius ) * radius / shadow_map_pass_viewport.Height ) );
				float Z = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Center, LightFront));
				Center = DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMVectorScale(LightRight, X), DirectX::XMVectorScale(LightUp, Y)), DirectX::XMVectorScale(LightFront, Z));

				DirectX::XMVECTOR Origin = DirectX::XMVectorSubtract(Center, DirectX::XMVectorScale(LightFront, (max_distance - min_distance) * 0.5f + radius));
				LightTransformMatrix.r[0] = DirectX::XMVectorSetW(LightRight, 0.0f);
				LightTransformMatrix.r[1] = DirectX::XMVectorSetW(LightUp, 0.0f);
				LightTransformMatrix.r[2] = DirectX::XMVectorSetW(LightFront, 0.0f);
				LightTransformMatrix.r[3] = DirectX::XMVectorSetW(Origin, 1.0f);
				DirectX::XMMATRIX LightViewMatrix = DirectX::XMMatrixInverse(nullptr, LightTransformMatrix);

				//	投影行列調整
				radius += 1.0f;
				DirectX::XMMATRIX LightProjectionMatrix = DirectX::XMMatrixOrthographicOffCenterLH(-radius, +radius, -radius, +radius, min_distance, max_distance + radius);
				DirectX::XMStoreFloat4x4(&shadow_map_constant.light_view_projection_transform[index], LightViewMatrix * LightProjectionMatrix);
			}

			{
				float near_clip = split_distance[index + 0];
				float far_clip = split_distance[index + 1];
				shadow_map_constant.shadow_split_distance[index].x = near_clip;
				shadow_map_constant.shadow_split_distance[index].y = near_clip;
				shadow_map_constant.shadow_split_distance[index].z = far_clip;
				shadow_map_constant.shadow_split_distance[index].w = far_clip;

				//	バイアス納入
				auto& bias_data = light.get_shadow_bias(index);
				float depth_bias, slope_depth_bias;
				depth_bias = bias_data.max_depth_bias / (far_clip - near_clip);
				depth_bias *= (far_clip - near_clip) / directional_shadow_map_count;
				depth_bias *= 0.5f;
				depth_bias *= bias_data.depth_bias;
				slope_depth_bias	= bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
				shadow_map_constant.shadow_bias[index].x = std::max(0.0f, depth_bias);
				shadow_map_constant.shadow_bias[index].y = std::max(0.0f, depth_bias * slope_depth_bias);
				shadow_map_constant.shadow_bias[index].z = bias_data.max_depth_bias;
				shadow_map_constant.shadow_bias[index].w = 1.0f / bias_data.sigma_factor; // 分散値
			}
		}
	}

	job_executor.execute([this, frustum_culling, shadow_map_constant](graphics_context& context)
	{
		shadow_map_pass.activate(context.device_context);

		//	定数バッファ情報設定
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &shadow_map_pass_viewport);

		//	モデル類描画
		context.is_shadow_rendering = true;
		model_render(context, context.render_data->mesh_container.shadow_datas, frustum_culling, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		shadow_map_pass.deactivate(context.device_context);
	}, "render_directional_shadow_map_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_point(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this, light, shadow_map_constant](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		context.deferred_light_constant.light_data = light.convert_light_data();
		context.deferred_light_constant.light_attribute = light.get_attribute();
		context.shadow_map_texture = render_targets->shadowmap_depth_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
#if defined(USE_CUSTOM_PIPELINE)
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_point);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			draw(context, pipeline_state, 4);
			pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
			point_lighting_pipeline_state->begin(context);
			draw(context, point_lighting_pipeline_state, 4);
			point_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_point_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_point_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this](graphics_context& context)
	{
	}, "render_point_shadow_map_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_spot(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this, light, shadow_map_constant](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		context.deferred_light_constant.light_data = light.convert_light_data();
		context.deferred_light_constant.light_attribute = light.get_attribute();
		context.shadow_map_texture = render_targets->shadowmap_depth_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
#if defined(USE_CUSTOM_PIPELINE)
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_spot);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			draw(context, pipeline_state, 4);
			pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
			spot_lighting_pipeline_state->begin(context);
			draw(context, spot_lighting_pipeline_state, 4);
			spot_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_spot_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_spot_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	auto reserve_scene_constant = context.scene_constant;

	DirectX::XMFLOAT4 rotation = light.get_rotation().convert_quaternion();
	DirectX::XMFLOAT3 position = light.get_position();
	DirectX::XMMATRIX TransformMatrix, ViewMatrix, ProjectionMatrix;
	TransformMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation))
					* DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	ViewMatrix = DirectX::XMMatrixInverse(nullptr, TransformMatrix);
	ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(light.get_outer_cone() * 2.0f, 1.0f, 0.01f, light.get_range());

	//	定数バッファ情報設定
	shadow_map_constant.shadow_map_count = 1;
	shadow_map_constant.shadow_attenuation = light.get_shadow_attenuation();
	shadow_map_constant.shadow_split_distance[0].x = -1.0f;
	shadow_map_constant.shadow_split_distance[0].y = -1.0f;
	shadow_map_constant.shadow_split_distance[0].z = FLT_MAX;
	shadow_map_constant.shadow_split_distance[0].w = FLT_MAX;

	//	バイアス納入
	{
		//	ライトの範囲で処理
		float near_clip = 0.1f;
		float far_clip = near_clip + light.get_range();

		auto& bias_data = light.get_shadow_bias(0);
		float depth_bias, slope_depth_bias;
		depth_bias = bias_data.max_depth_bias / (far_clip - near_clip);
		depth_bias *= (far_clip - near_clip) / directional_shadow_map_count;
		depth_bias *= 0.5f;
		depth_bias *= bias_data.depth_bias;
		slope_depth_bias	= bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
		shadow_map_constant.shadow_bias[0].x = std::max(0.0f, depth_bias);
		shadow_map_constant.shadow_bias[0].y = std::max(0.0f, depth_bias * slope_depth_bias);
		shadow_map_constant.shadow_bias[0].z = bias_data.max_depth_bias;
		shadow_map_constant.shadow_bias[0].w = bias_data.sigma_factor; // 分散値
	}

	shadow_map_constant.shadow_mask = 0x00000001;
	DirectX::XMStoreFloat4x4(&shadow_map_constant.light_view_projection_transform[0], ViewMatrix * ProjectionMatrix);

	//	モデル類描画
	collision::frustum_culling frustum_culling;
	{
		frustum_culling.create_planes(shadow_map_constant.light_view_projection_transform[0]);
		frustum_culling.create_frustum_corners(DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&context.scene_constant.view_projection_transform)));
	}

	job_executor.execute([this, shadow_map_constant, frustum_culling](graphics_context& context)
	{
		shadow_map_pass.activate(context.device_context);

		context.shadow_map_constant = shadow_map_constant;

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &shadow_map_pass_viewport);

		context.is_shadow_rendering = true;
		model_render(context, context.render_data->mesh_container.shadow_datas, frustum_culling, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		shadow_map_pass.deactivate(context.device_context);
	}, "render_spot_shadow_map_pass", context, get_render_basis_sort_order() + register_index++);
	context.scene_constant = reserve_scene_constant;
}

void deffered_lighting_renderer::resize_buffer(int width, int height)
{
	if (lighting_pass_viewport.Width == static_cast<float>(width) && lighting_pass_viewport.Height == static_cast<float>(height))
		return;

	//	ライティングパス生成
	{
		lighting_pass.render_targets.clear();
		lighting_pass.render_targets.emplace_back(render_targets->scene_color_texture);
		lighting_pass.render_targets.back().clear_color = false;
		lighting_pass.render_targets.back().clear_color_value[0] = 0.0f;
		lighting_pass.render_targets.back().clear_color_value[1] = 0.0f;
		lighting_pass.render_targets.back().clear_color_value[2] = 0.0f;
		lighting_pass.render_targets.back().clear_color_value[3] = 1.0f;

		// ビューポートの設定
		lighting_pass_viewport.TopLeftX = 0.0f;
		lighting_pass_viewport.TopLeftY = 0.0f;
		lighting_pass_viewport.Width = static_cast<float>(width);
		lighting_pass_viewport.Height = static_cast<float>(height);
		lighting_pass_viewport.MinDepth = 0.0f;
		lighting_pass_viewport.MaxDepth = 1.0f;
	}
}

void deffered_lighting_renderer::resize_shadow_buffer(int width, int height)
{
	if (shadow_map_pass_viewport.Width == static_cast<float>(width) && shadow_map_pass_viewport.Height == static_cast<float>(height))
		return;

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	render_targets->shadowmap_depth_texture->get_shader_resource_view()->GetDesc(&srv_desc);
	//	シャドウマップパス生成
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	depth_stencil_view_desc.Texture2DArray.ArraySize = srv_desc.Texture2DArray.ArraySize;
	depth_stencil_view_desc.Texture2DArray.FirstArraySlice = 0;
	depth_stencil_view_desc.Texture2DArray.MipSlice = 0;
	shadow_map_pass.depth_stencil = depth_stencil_view(render_targets->shadowmap_depth_texture, &depth_stencil_view_desc);

	// ビューポートの設定
	shadow_map_pass_viewport.TopLeftX = 0.0f;
	shadow_map_pass_viewport.TopLeftY = 0.0f;
	shadow_map_pass_viewport.Width = static_cast<float>(width);
	shadow_map_pass_viewport.Height = static_cast<float>(height);
	shadow_map_pass_viewport.MinDepth = 0.0f;
	shadow_map_pass_viewport.MaxDepth = 1.0f;
}

#if defined(USE_IMGUI)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
debug_draw_pass_renderer::debug_draw_pass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	resize_buffer(0, 0);

	static constexpr std::string_view pso_names[] =
	{
		"debug_draw_line_0",
		"debug_draw_line_1",
	};
	//	パイプラインステート生成
	if (auto psm = pipeline_state_manager::instance())
	{
		int index;
		index = static_cast<int>(debug_draw_primitive_container::line_buffer_id::normal);
		psm->generate_pipeline_state<debug_draw_line_pipeline_state>(pso_names[index], &debug_draw_line_pipeline_states[index]);
		debug_draw_line_pipeline_states[index]->set_domain(pipeline_state_base::domains::system);

		index = static_cast<int>(debug_draw_primitive_container::line_buffer_id::disable_depth_test);
		psm->generate_pipeline_state<debug_draw_line_disable_depth_test_pipeline_state>(pso_names[index], &debug_draw_line_pipeline_states[index]);
		debug_draw_line_pipeline_states[index]->set_domain(pipeline_state_base::domains::system);
	}
}

debug_draw_pass_renderer::~debug_draw_pass_renderer()
{
}

void debug_draw_pass_renderer::render(renderer_context& context)
{
	job_executor.execute([this](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	ライン描画
		for (UINT index = 0; index < static_cast<int>(debug_draw_primitive_container::line_buffer_id::max); ++index)
		{
			debug_draw_line_pipeline_states[index]->begin(context);
			{
				auto& mesh = context.render_data->debug_draw_container.line_buffer_datas[index].color_meshs[0];
				auto it = mesh.vertex_buffer_views->begin();
				UINT stride = static_cast<UINT>(it->second.stride_in_bytes);
				UINT offset = static_cast<UINT>(it->second.offset());
				context.device_context->IASetVertexBuffers(0, 1, it->second.buffer.GetAddressOf(), &stride, &offset);

				context.device_context->Draw(static_cast<UINT>(it->second.count()), 0);
			}
			debug_draw_line_pipeline_states[index]->end(context);
		}
		rendering_pass.deactivate(context.device_context);
	}, "debug_draw_pass", context, get_render_basis_sort_order());
}

void debug_draw_pass_renderer::resize_buffer(int width, int height)
{
	rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());
	rendering_pass.render_targets.back().clear_color = false;
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

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
#endif // defined(USE_IMGUI)
