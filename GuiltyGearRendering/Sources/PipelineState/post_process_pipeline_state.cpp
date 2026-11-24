#include "PipelineState/post_process_pipeline_state.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
write_back_pipeline_state::write_back_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/fullscreen_quad_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = render_data::instance()->get_device()->CreateBlendState(&blend_desc, blend_states.GetAddressOf());
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;
		hr = render_data::instance()->get_device()->CreateRasterizerState(&rasterizer_desc, rasterizer_states.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

write_back_pipeline_state::~write_back_pipeline_state()
{
}

void write_back_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(vs->get_input_layout().Get());
	context.device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(nullptr, 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	context.device_context->PSSetShaderResources(0, 1, context.scene_color_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
}

void write_back_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
compress_rgb_to_log_pipeline_state::compress_rgb_to_log_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/compress_rgb_to_log_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = render_data::instance()->get_device()->CreateBlendState(&blend_desc, blend_states.GetAddressOf());
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;
		hr = render_data::instance()->get_device()->CreateRasterizerState(&rasterizer_desc, rasterizer_states.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

compress_rgb_to_log_pipeline_state::~compress_rgb_to_log_pipeline_state()
{
}

void compress_rgb_to_log_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(vs->get_input_layout().Get());
	context.device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(nullptr, 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	context.device_context->PSSetShaderResources(0, 1, context.scene_color_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
}

void compress_rgb_to_log_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
decompress_rgb_to_log_pipeline_state::decompress_rgb_to_log_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/decompress_rgb_to_log_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = render_data::instance()->get_device()->CreateBlendState(&blend_desc, blend_states.GetAddressOf());
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;
		hr = render_data::instance()->get_device()->CreateRasterizerState(&rasterizer_desc, rasterizer_states.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

decompress_rgb_to_log_pipeline_state::~decompress_rgb_to_log_pipeline_state()
{
}

void decompress_rgb_to_log_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(vs->get_input_layout().Get());
	context.device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(nullptr, 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	context.device_context->PSSetShaderResources(0, 1, context.scene_color_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
}

void decompress_rgb_to_log_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
namespace ssr
{
	constexpr UINT cbv_index = 1;
	constexpr UINT gbuffer_srv_start = 0;

	constexpr UINT ssr_uv_srv_index = 10;
	constexpr UINT linear_boarder_black_index = 11;

	constexpr UINT ssr_merge_color_srv_index = 10;
	constexpr UINT ssr_merge_bokeh_srv_index = 11;
	constexpr UINT ssr_merge_bokeh_sampler_index = 11;
	constexpr UINT ssr_merge_skymap_srv_index = 12;

	struct constant_buffer
	{
		float ray_max_distance;				//  反射最大距離
		float ray_resolution;				//  レイのステップ解像度
		DirectX::XMFLOAT2 ray_tickness;		//  深度判定を行う際の厚みの範囲

		int ray_substep_iteration_max;		//	ステップ深度チェック細分化回数
		int ray_flags;						//  処理の設定フラグ
		DirectX::XMFLOAT2 dummy;			//  深度判定を行う際の厚みの範囲
		DirectX::XMFLOAT4 viewport_size;	//  シーンのビューポートサイズ
	};
}

screen_space_reflection_uv_pipeline_state::screen_space_reflection_uv_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };
	auto device = render_data::instance()->get_device();

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/screen_space_reflection_uv_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&blend_desc, blend_states.GetAddressOf());
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	定数バッファ生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::screen_space_reflection_constants);
		hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

screen_space_reflection_uv_pipeline_state::~screen_space_reflection_uv_pipeline_state()
{
}

void screen_space_reflection_uv_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(vs->get_input_layout().Get());

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(nullptr, 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	ID3D11ShaderResourceView* g_buffer_srvs[static_cast<UINT>(common_pipeline_states::gbuffer_id::max)];
	for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
	{
		g_buffer_srvs[index] = context.g_buffer_textures[index]->get_shader_resource_view().Get();
	}
	context.device_context->PSSetShaderResources(ssr::gbuffer_srv_start, ARRAYSIZE(g_buffer_srvs), g_buffer_srvs);
	context.device_context->PSSetSamplers(ssr::gbuffer_srv_start, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());

	//	定数バッファ設定
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(ssr::cbv_index, 1, constant_buffer.GetAddressOf());

	ssr::constant_buffer cb;
	cb.ray_flags = context.screen_space_reflection_constant.ray_flags;
	cb.ray_max_distance = context.screen_space_reflection_constant.ray_max_distance;
	cb.ray_resolution = context.screen_space_reflection_constant.ray_resolution;
	cb.ray_substep_iteration_max = context.screen_space_reflection_constant.ray_substep_iteration_max;
	cb.ray_tickness = context.screen_space_reflection_constant.ray_tickness;
	cb.viewport_size = context.screen_space_reflection_constant.viewport_size;
	context.device_context->UpdateSubresource(constant_buffer.Get(), 0, nullptr, &cb, 0, 0);
}

void screen_space_reflection_uv_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
screen_space_reflection_color_pipeline_state::screen_space_reflection_color_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };
	auto device = render_data::instance()->get_device();

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/screen_space_reflection_color_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&blend_desc, blend_states.GetAddressOf());
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	定数バッファ生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::screen_space_reflection_constants);
		hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

screen_space_reflection_color_pipeline_state::~screen_space_reflection_color_pipeline_state()
{
}

void screen_space_reflection_color_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(vs->get_input_layout().Get());

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(nullptr, 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	ID3D11ShaderResourceView* g_buffer_srvs[static_cast<UINT>(common_pipeline_states::gbuffer_id::max)];
	for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
	{
		g_buffer_srvs[index] = context.g_buffer_textures[index]->get_shader_resource_view().Get();
	}
	context.device_context->PSSetShaderResources(ssr::gbuffer_srv_start, ARRAYSIZE(g_buffer_srvs), g_buffer_srvs);
	context.device_context->PSSetSamplers(ssr::gbuffer_srv_start, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
	
	context.device_context->PSSetShaderResources(ssr::ssr_uv_srv_index, 1, context.work_texture[0]->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(ssr::ssr_uv_srv_index, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());
	context.device_context->PSSetSamplers(ssr::linear_boarder_black_index, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR_BORDER_BLACK).GetAddressOf());

	//	定数バッファ設定
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(ssr::cbv_index, 1, constant_buffer.GetAddressOf());

	ssr::constant_buffer cb;
	cb.ray_flags = context.screen_space_reflection_constant.ray_flags;
	cb.ray_max_distance = context.screen_space_reflection_constant.ray_max_distance;
	cb.ray_resolution = context.screen_space_reflection_constant.ray_resolution;
	cb.ray_substep_iteration_max = context.screen_space_reflection_constant.ray_substep_iteration_max;
	cb.ray_tickness = context.screen_space_reflection_constant.ray_tickness;
	cb.viewport_size = context.screen_space_reflection_constant.viewport_size;
	context.device_context->UpdateSubresource(constant_buffer.Get(), 0, nullptr, &cb, 0, 0);
}

void screen_space_reflection_color_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
screen_space_reflection_composite_pipeline_state::screen_space_reflection_composite_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };
	auto device = render_data::instance()->get_device();

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/screen_space_reflection_composite_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&blend_desc, blend_states.GetAddressOf());
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

screen_space_reflection_composite_pipeline_state::~screen_space_reflection_composite_pipeline_state()
{
}

void screen_space_reflection_composite_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(vs->get_input_layout().Get());

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(nullptr, 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	ID3D11ShaderResourceView* g_buffer_srvs[static_cast<UINT>(common_pipeline_states::gbuffer_id::max)];
	for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
	{
		g_buffer_srvs[index] = context.g_buffer_textures[index]->get_shader_resource_view().Get();
	}
	context.device_context->PSSetShaderResources(ssr::gbuffer_srv_start, ARRAYSIZE(g_buffer_srvs), g_buffer_srvs);
	context.device_context->PSSetShaderResources(ssr::ssr_merge_color_srv_index, 1, context.work_texture[0]->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(ssr::gbuffer_srv_start, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
	context.device_context->PSSetSamplers(ssr::ssr_merge_bokeh_sampler_index, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR_CLAMP).GetAddressOf());

	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
}

void screen_space_reflection_composite_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}
