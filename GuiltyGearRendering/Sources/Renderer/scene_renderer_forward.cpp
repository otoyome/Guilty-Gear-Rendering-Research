#include "Renderer/scene_renderer_forward.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/deferred_lighting_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"

#include "PipelineState/post_process_pipeline_state.h"

namespace
{
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
			float lod_rate = (mesh_length - lod_range_min) / (lod_range_max - lod_range_min);
			auto lod_mesh_index = math::lerp(0u, lod_level - 1, math::saturate(lod_rate));
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

	void model_render(graphics_context& context, const std::vector<std::size_t>& mesh_indices, const std::vector<float>& mesh_lengths)
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

			if (auto pipeline_state = material_data->material->get_pipeline_state(material::surface_paths::forward_pass))
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
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
before_dof_forward_renderer::before_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
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
}

before_dof_forward_renderer::~before_dof_forward_renderer()
{
}

void before_dof_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::before_dof);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass : before_dof", renderer_context, get_render_basis_sort_order());
}

void before_dof_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
after_dof_forward_renderer::after_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
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
}

after_dof_forward_renderer::~after_dof_forward_renderer()
{
}

void after_dof_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::after_dof);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass : after_dof", renderer_context, get_render_basis_sort_order());
}

void after_dof_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
after_motion_blur_forward_renderer::after_motion_blur_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
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
}

after_motion_blur_forward_renderer::~after_motion_blur_forward_renderer()
{
}

void after_motion_blur_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::after_motion_blur);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass: after_motion_blur", renderer_context, get_render_basis_sort_order());
}

void after_motion_blur_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}
