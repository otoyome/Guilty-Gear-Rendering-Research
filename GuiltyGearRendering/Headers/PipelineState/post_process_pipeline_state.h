#pragma once

#include "common_pipeline_states.h"

//	•`‚«–ß‚µ
class write_back_pipeline_state : public graphics_pipeline_state
{
public:
	write_back_pipeline_state();
	~write_back_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
};

//	Fî•ñ‚ğ‘Î”ˆ³k
class compress_rgb_to_log_pipeline_state : public graphics_pipeline_state
{
public:
	compress_rgb_to_log_pipeline_state();
	~compress_rgb_to_log_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
};

//	‘Î”ˆ³k‚³‚ê‚½Fî•ñ‚ğ‰ğ“€
class decompress_rgb_to_log_pipeline_state : public graphics_pipeline_state
{
public:
	decompress_rgb_to_log_pipeline_state();
	~decompress_rgb_to_log_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
};

//	SSR:UV¶¬
class screen_space_reflection_uv_pipeline_state : public graphics_pipeline_state
{
public:
	screen_space_reflection_uv_pipeline_state();
	~screen_space_reflection_uv_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
};

//	SSR:Color¶¬
class screen_space_reflection_color_pipeline_state : public graphics_pipeline_state
{
public:
	screen_space_reflection_color_pipeline_state();
	~screen_space_reflection_color_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
};

//	SSR:‡¬
class screen_space_reflection_composite_pipeline_state : public graphics_pipeline_state
{
public:
	screen_space_reflection_composite_pipeline_state();
	~screen_space_reflection_composite_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
};
