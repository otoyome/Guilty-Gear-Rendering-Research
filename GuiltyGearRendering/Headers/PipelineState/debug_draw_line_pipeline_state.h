#pragma once

#include "common_pipeline_states.h"

//	デバッグプリミティブ描画用(深度テスト有り)
class debug_draw_line_pipeline_state : public graphics_pipeline_state
{
protected:
	debug_draw_line_pipeline_state(bool depth_test);

public:
	debug_draw_line_pipeline_state();
	~debug_draw_line_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void update_parameters(const graphics_context& context, const material& material, const buffer_datas* buffer_data) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
};

//	デバッグプリミティブ描画用(深度テスト無し)
class debug_draw_line_disable_depth_test_pipeline_state : public debug_draw_line_pipeline_state
{
public:
	debug_draw_line_disable_depth_test_pipeline_state();
	~debug_draw_line_disable_depth_test_pipeline_state() override;
};
