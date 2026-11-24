#pragma once

#include "renderer.h"
#include "render_pass.h"
#include "scene_render_targets.h"
#include "geometric_primitive.h"

#include "collision/frustum_culling.h"

#if defined(USE_IMGUI)
#define USE_CUSTOM_PIPELINE
#endif	//	defined(USE_IMGUI)

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	深度プリパス
class depth_prepass_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::depth_prepass; }

public:
	depth_prepass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~depth_prepass_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	GBuffer
class gbuffer_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::g_buffer_pass; }

public:
	gbuffer_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~gbuffer_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	graphics_pipeline_state_ptr skybox_pipeline_states;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ディファードライティング
class deffered_lighting_renderer : public renderer
{
	static constexpr UINT directional_shadow_map_count = 4;

public:
	renderer_domain get_domain() const override { return renderer_domain::deffered_lighting_pass; }

public:
	deffered_lighting_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~deffered_lighting_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	void render_indirect(renderer_context& context);
	void render_directional(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);
	void render_directional_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);
	void render_point(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);
	void render_point_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);
	void render_spot(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);
	void render_spot_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);

	void resize_shadow_buffer(int width, int height);

private:
	std::shared_ptr<scene_render_targets> render_targets;
	UINT register_index;
	render_pass lighting_pass;
	D3D11_VIEWPORT lighting_pass_viewport;

	render_pass shadow_map_pass;
	D3D11_VIEWPORT shadow_map_pass_viewport;

#if defined(USE_CUSTOM_PIPELINE)
	std::shared_ptr<material> deferred_lighting_material;
#else	//	defined(USE_CUSTOM_PIPELINE)
	graphics_pipeline_state_ptr indirect_lighting_pipeline_state;
	graphics_pipeline_state_ptr directional_lighting_pipeline_state;
	graphics_pipeline_state_ptr point_lighting_pipeline_state;
	graphics_pipeline_state_ptr spot_lighting_pipeline_state;
	graphics_pipeline_state_ptr shadow_light_pipeline_state;
#endif	//	defined(USE_CUSTOM_PIPELINE)
};

#if defined(USE_IMGUI)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	デバッグ描画パス
class debug_draw_pass_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::debug_draw; }

public:
	debug_draw_pass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~debug_draw_pass_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	graphics_pipeline_state_ptr debug_draw_line_pipeline_states[static_cast<int>(debug_draw_primitive_container::line_buffer_id::max)];
};
#endif // defined(USE_IMGUI)
