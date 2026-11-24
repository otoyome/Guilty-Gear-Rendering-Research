#pragma once

#include "renderer.h"
#include "render_pass.h"
#include "scene_render_targets.h"
#include "geometric_primitive.h"

#include "collision/frustum_culling.h"

#include "graphics/fast_fourier_transform.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ポストプロセスベース
class post_process_renderer_base : public renderer
{
public:
	virtual material::blendable_locations get_material_blendable_locations() const = 0;

public:
	post_process_renderer_base(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~post_process_renderer_base() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass work_buffer_rendering_pass[2];
	D3D11_VIEWPORT work_buffer_viewport;
	render_pass scene_color_rendering_pass;
	D3D11_VIEWPORT scene_color_viewport;

	graphics_pipeline_state_ptr write_back;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	スクリーンスペースリフレクションパス
class screen_space_reflection_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::screen_space_reflection_pass; }

public:
	screen_space_reflection_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~screen_space_reflection_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	void render_reflection(graphics_context& context, post_process_volume_ptr inner_volume);
	void render_denoise(graphics_context& context, post_process_volume_ptr inner_volume);
	void render_composite(graphics_context& context, post_process_volume_ptr inner_volume);

private:
	std::shared_ptr<scene_render_targets> render_targets;

	//	生成用情報
	render_pass ssr_uv_pass;
	render_pass ssr_color_pass;
	render_pass ssr_composite_pass;
	D3D11_VIEWPORT ssr_uv_pass_viewport;
	D3D11_VIEWPORT ssr_color_pass_viewport;
	D3D11_VIEWPORT ssr_composite_pass_viewport;
	graphics_pipeline_state_ptr ssr_uv_pipeline_state;
	graphics_pipeline_state_ptr ssr_color_pipeline_state;

	//	デノイズパス
	std::shared_ptr<compute_shader> denoise;
	std::shared_ptr<compute_shader> temporal_filter;
	Microsoft::WRL::ComPtr<ID3D11Buffer> denoise_constant_buffer;
	bool accumlate_flip_flag = false;

	//	合成パス
	render_pass scene_color_rendering_pass;
	D3D11_VIEWPORT scene_color_viewport;
	graphics_pipeline_state_ptr write_back;
	graphics_pipeline_state_ptr ssr_composite_pipeline_state;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度前ポストプロセスパス
class before_dof_post_process_renderer : public post_process_renderer_base
{
public:
	renderer_domain get_domain() const override { return renderer_domain::before_dof_post_process_pass; }
	material::blendable_locations get_material_blendable_locations() const override { return material::blendable_locations::before_dof; }

public:
	before_dof_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
	: post_process_renderer_base(job_executor, render_targets) {}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度パス
class depth_of_field_post_process_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::depth_of_field_pass; }

public:
	depth_of_field_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~depth_of_field_post_process_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	void update_constant_buffer(graphics_context& context, const depth_of_field_data& dof_data, UINT buffer_width, UINT buffer_height);

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass scene_color_rendering_pass;
	D3D11_VIEWPORT scene_color_viewport;

	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	std::shared_ptr<compute_shader> dof_filter_setup;
	std::shared_ptr<compute_shader> dof_vertical_integrate;
	std::shared_ptr<compute_shader> dof_apply;

	UINT filter_buffer_width;
	UINT filter_buffer_height;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> filter_buffer_uav;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> filter_buffer_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> filter_buffer_transpose_uav;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度後ポストプロセスパス
class after_dof_post_process_renderer : public post_process_renderer_base
{
public:
	renderer_domain get_domain() const override { return renderer_domain::after_dof_post_process_pass; }
	material::blendable_locations get_material_blendable_locations() const override { return material::blendable_locations::after_dof; }

public:
	after_dof_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
	: post_process_renderer_base(job_executor, render_targets) {}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度後半透明描画後ポストプロセスパス
class after_translucency_post_process_renderer : public post_process_renderer_base
{
public:
	renderer_domain get_domain() const override { return renderer_domain::after_translucency_post_process_pass; }
	material::blendable_locations get_material_blendable_locations() const override { return material::blendable_locations::after_dof_translucency; }

public:
	after_translucency_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
	: post_process_renderer_base(job_executor, render_targets) {}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	モーションブラーパス
class motion_blur_post_process_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::motion_blur_pass; }

public:
	motion_blur_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~motion_blur_post_process_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass scene_color_rendering_pass;
	D3D11_VIEWPORT scene_color_viewport;

	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	std::shared_ptr<compute_shader> reconstract_velocity;
	std::shared_ptr<compute_shader> motion_blur;

	texture_ptr reconstract_velocity_map;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ブルーム前ポストプロセスパス
class before_bloom_post_process_renderer : public post_process_renderer_base
{
public:
	renderer_domain get_domain() const override { return renderer_domain::before_bloom_post_process_pass; }
	material::blendable_locations get_material_blendable_locations() const override { return material::blendable_locations::before_bloom; }

public:
	before_bloom_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
	: post_process_renderer_base(job_executor, render_targets) {}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ブルームとトーンマッピングパス
class bloom_and_tone_mapping_post_process_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::bloom_and_tone_mapping_pass; }

public:
	bloom_and_tone_mapping_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~bloom_and_tone_mapping_post_process_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	void bloom_pass(graphics_context& context, post_process_volume_ptr inner_volume, int average_buffer_index);
	void lens_ghost_pass(graphics_context& context, post_process_volume_ptr inner_volume, int average_buffer_index);

private:
	std::shared_ptr<scene_render_targets> render_targets;

	//	平均輝度算出
	std::shared_ptr<compute_shader> correct_luminance;
	int average_luminance_buffer_index;
	texture_ptr average_luminance_buffer[2];

	//	汎用シェーダー
	std::shared_ptr<compute_shader> copy_texture;
	std::shared_ptr<compute_shader> scene_additive;

	//	FFT関係
	std::unique_ptr<fast_fourier_transform> scene_fft;
	Microsoft::WRL::ComPtr<ID3D11Buffer> fft_gaussian_constant_buffer;
	std::shared_ptr<compute_shader> fft_gaussian_filter;
	std::shared_ptr<compute_shader> fft_multiply;

	//	輝度抽出
	Microsoft::WRL::ComPtr<ID3D11Buffer> luminance_extract_constant_buffer;
	std::shared_ptr<compute_shader> luminance_extract;
	texture_ptr lens_ghost_luminance_map;

	//	ブルーム用
	std::unique_ptr<fast_fourier_transform> bloom_kernel_fft;
	texture_ptr bloom_image_kernel_texture;

	//	ゴースト用
	Microsoft::WRL::ComPtr<ID3D11Buffer> lens_ghost_constant_buffer;
	std::shared_ptr<compute_shader> generate_ghost;
	std::unique_ptr<fast_fourier_transform> lens_ghost_kernel_fft;
	texture_ptr lens_ghost_image_kernel_texture;

	//	トーンマッピング用
	Microsoft::WRL::ComPtr<ID3D11Buffer> tone_mapping_constant_buffer;
	std::shared_ptr<compute_shader> tone_mapping;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	トーンマッピング後ポストプロセスパス
class after_tone_mapping_post_process_renderer : public post_process_renderer_base
{
public:
	renderer_domain get_domain() const override { return renderer_domain::after_tone_mapping_post_process_pass; }
	material::blendable_locations get_material_blendable_locations() const override { return material::blendable_locations::after_tone_mapping; }

public:
	after_tone_mapping_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
		: post_process_renderer_base(job_executor, render_targets) {}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	アンチエイリアスパス
class anti_alias_post_process_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::anti_alias_post_process_pass; }

public:
	anti_alias_post_process_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~anti_alias_post_process_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass scene_color_rendering_pass;
	render_pass work_color_rendering_pass;
	D3D11_VIEWPORT scene_color_viewport;
	D3D11_VIEWPORT work_color_viewport;
	graphics_pipeline_state_ptr write_back;

	//	SMAA用
	texture_ptr smaa_edge_buffer;
	texture_ptr smaa_blend_buffer;
	render_pass smaa_edge_buffer_rendering_pass;
	render_pass smaa_blend_buffer_rendering_pass;
	D3D11_VIEWPORT smaa_edge_buffer_viewport;
	D3D11_VIEWPORT smaa_blend_buffer_viewport;
	graphics_pipeline_state_ptr fxaa;
	graphics_pipeline_state_ptr smaa_edge_detection;
	graphics_pipeline_state_ptr smaa_blending_weight_calculation;
	graphics_pipeline_state_ptr smaa_neighborhood_blending;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ファイナルパス
class finalpass_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::final_pass; }

public:
	finalpass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~finalpass_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	graphics_pipeline_state_ptr convert_linear_to_srgb_pipeline_states;
};
