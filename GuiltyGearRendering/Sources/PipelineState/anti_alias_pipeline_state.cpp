#include "PipelineState/anti_alias_pipeline_state.h"

#include "subpixel_morphological_anti_aliasing/textures/AreaTex.h"
#include "subpixel_morphological_anti_aliasing/textures/SearchTex.h"

namespace smaa
{
	//	プリセット設定にしているので必要であればこれを作成して対応する事
	struct constants
	{
		float smaa_threshold = 0.05f;	//	エッジ検出閾値指定
		UINT smaa_max_search_steps = 32;	//	水平/垂直パターン検索で実行される最大ステップ数
		UINT smaa_max_search_steps_diagonal = 16;	//	対角パターン検索の最大ステップ数
		UINT smaa_corner_rounding = 25;	//	角の丸みを調整
		float smaa_local_contrast_adaptation_factor = 2.0f;	//	不要な交差エッジを除去するための係数
		DirectX::XMUINT2 smaa_dummy;
		bool smaa_draw_edge = false;	//	デバッグ用の適応エッジ表示判定
	};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
fxaa_pipeline_state::fxaa_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/fast_approximate_anti_aliasing_ps.cso";
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

fxaa_pipeline_state::~fxaa_pipeline_state()
{
}

void fxaa_pipeline_state::begin(const graphics_context& context)
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

	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetShaderResources(0, 1, context.scene_color_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());
}

void fxaa_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
smaa_edge_detection_pipeline_state::smaa_edge_detection_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/smaa_edge_detection_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/smaa_edge_detection_ps.cso";
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

smaa_edge_detection_pipeline_state::~smaa_edge_detection_pipeline_state()
{
}

void smaa_edge_detection_pipeline_state::begin(const graphics_context& context)
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

	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetShaderResources(0, 1, context.scene_color_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
	context.device_context->PSSetSamplers(1, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());
}

void smaa_edge_detection_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
smaa_blending_weight_calculation_pipeline_state::smaa_blending_weight_calculation_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/smaa_blending_weight_calculation_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/smaa_blending_weight_calculation_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	//	パラメーターテクスチャ生成
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> dummy_resource;
		//	Area
		{
			texture2d_desc.Width = AREATEX_WIDTH;
			texture2d_desc.Height = AREATEX_HEIGHT;
			texture2d_desc.Format = DXGI_FORMAT_R8G8_UNORM;

			D3D11_SUBRESOURCE_DATA initialize_data = {};
			initialize_data.pSysMem = areaTexBytes;
			initialize_data.SysMemPitch = AREATEX_PITCH;
			texture_manager::instance()->generate_texture2d(area_texture, "AreaTex", texture2d_desc, initialize_data);
		}
		//	Search
		{
			texture2d_desc.Width = SEARCHTEX_WIDTH;
			texture2d_desc.Height = SEARCHTEX_HEIGHT;
			texture2d_desc.Format = DXGI_FORMAT_R8_UNORM;

			D3D11_SUBRESOURCE_DATA initialize_data = {};
			initialize_data.pSysMem = searchTexBytes;
			initialize_data.SysMemPitch = SEARCHTEX_PITCH;
			texture_manager::instance()->generate_texture2d(search_texture, "SearchTex", texture2d_desc, initialize_data);
		}
	}

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

smaa_blending_weight_calculation_pipeline_state::~smaa_blending_weight_calculation_pipeline_state()
{
}

void smaa_blending_weight_calculation_pipeline_state::begin(const graphics_context& context)
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

	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetShaderResources(0, 1, context.scene_color_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetShaderResources(1, 1, area_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetShaderResources(2, 1, search_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
	context.device_context->PSSetSamplers(1, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());
}

void smaa_blending_weight_calculation_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
smaa_neighborhood_blending_pipeline_state::smaa_neighborhood_blending_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/smaa_neighborhood_blending_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/smaa_neighborhood_blending_ps.cso";
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

smaa_neighborhood_blending_pipeline_state::~smaa_neighborhood_blending_pipeline_state()
{
}

void smaa_neighborhood_blending_pipeline_state::begin(const graphics_context& context)
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

	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetShaderResources(0, 1, context.scene_color_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetShaderResources(1, 1, context.work_texture[0]->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetShaderResources(2, 1, context.work_texture[1]->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT).GetAddressOf());
	context.device_context->PSSetSamplers(1, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());
}

void smaa_neighborhood_blending_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}
