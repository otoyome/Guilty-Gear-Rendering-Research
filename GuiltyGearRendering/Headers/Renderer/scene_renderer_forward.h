#pragma once

#include "renderer.h"
#include "render_pass.h"
#include "scene_render_targets.h"
#include "geometric_primitive.h"

#include "collision/frustum_culling.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	フォワードレンダリングベース
class forward_renderer_base : public renderer
{
public:
	virtual material::translucency_paths get_material_translucency_path() const = 0;

public:
	forward_renderer_base(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~forward_renderer_base() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度前フォワードレンダリング
class before_dof_forward_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::before_dof_translucency_pass; }

public:
	before_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~before_dof_forward_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度後フォワードレンダリング
class after_dof_forward_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::after_dof_translucency_pass; }

public:
	after_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~after_dof_forward_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	モーションブラー後フォワードレンダリング
class after_motion_blur_forward_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::after_motion_blur_forward_pass; }

public:
	after_motion_blur_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~after_motion_blur_forward_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};
