#include "custom_pipeline_state.h"

#include "render_data.h"

namespace
{
	//  マテリアルフラグの対応表
	constexpr std::pair<material::flags, UINT> convert_material_flags[] =
	{
		{material::flags::indirect_lighting_off, common_pipeline_states::material_attribute_indirect_lighting_off},
		{material::flags::shadow_receiver, common_pipeline_states::material_attribute_shadow_receiver},
	};

	//  テクスチャフラグの対応表
	constexpr std::pair<texture::flags, UINT> convert_texture_flags[] =
	{
		{texture::flags::srgb, common_pipeline_states::shader_resource_view_flags_srgb},
		{texture::flags::cube_map, common_pipeline_states::shader_resource_view_flags_cube_map},
	};

	//	近距離のライトを検索
	struct forward_light_data
	{
		common_pipeline_states::forward_light_constants light;

		//	IBL情報
		texture_ptr diffuse_ibl_texture;
		texture_ptr specluar_ibl_texture;
		texture_ptr lut_ggx_texture;
	};
	forward_light_data search_neighborhood_light(const graphics_context& context, const buffer_datas& buffer_data)
	{
		forward_light_data light_data = {};
		//	テキトーに近くのライトを詰める
		auto BoundsMin = DirectX::XMVectorSet(buffer_data.bounds_min.x, buffer_data.bounds_min.y, buffer_data.bounds_min.z, 0);
		auto BoundsMax = DirectX::XMVectorSet(buffer_data.bounds_max.x, buffer_data.bounds_max.y, buffer_data.bounds_max.z, 0);
		auto BoundsCenter = DirectX::XMVectorScale(DirectX::XMVectorSubtract(BoundsMax, BoundsMin), 0.5f);
		BoundsCenter = DirectX::XMVectorAdd(BoundsCenter, BoundsMin);

		for (auto& light : context.render_data->light_container.lights)
		{
			switch(light->get_light_type())
			{
				case light_type::directional:
				{
					if (light_data.light.light_counts.x >= common_pipeline_states::directional_light_count)
						continue;

					auto& data = light_data.light.directional_light_data[light_data.light.light_counts.x];
					data.color = light->get_color();
					auto direction = light->get_rotation().get_front();
					data.direction.x = direction.x;
					data.direction.y = direction.y;
					data.direction.z = direction.z;
					data.direction.w = 0.0f;
					++light_data.light.light_counts.x;
					break;
				}
				case light_type::point:
				{
					if (light_data.light.light_counts.y >= common_pipeline_states::point_light_count)
						continue;

					auto& data = light_data.light.point_light_data[light_data.light.light_counts.y];
					data.position.x = light->get_position().x;
					data.position.y = light->get_position().y;
					data.position.z = light->get_position().z;
					data.position.w = 1.0f;
					data.range = light->get_range();
					data.color = light->get_color();
					++light_data.light.light_counts.y;
					break;
				}
				case light_type::spot:
				{
					if (light_data.light.light_counts.z >= common_pipeline_states::spot_light_count)
						continue;

					auto& data = light_data.light.spot_light_data[light_data.light.light_counts.z];
					auto direction = light->get_rotation().get_front();
					data.position.x = light->get_position().x;
					data.position.y = light->get_position().y;
					data.position.z = light->get_position().z;
					data.position.w = 1.0f;
					data.direction.x = direction.x;
					data.direction.y = direction.y;
					data.direction.z = direction.z;
					data.direction.w = 0.0f;
					data.range = light->get_range();
					data.inner_corn = light->get_inner_cone();
					data.outer_corn = light->get_outer_cone();
					data.color = light->get_color();
					++light_data.light.light_counts.z;
					break;
				}
			}
		}

		//	IBL
		for (const auto& data : context.render_data->light_container.light_probes)
		{
			const light_probe* light_probe = data.get();
			if (!light_probe->is_flag(light_probe::flags::active) || !light_probe->is_flag(light_probe::flags::unbind))
			{
				continue;
			}

			//	IBLテクスチャ設定
			light_data.diffuse_ibl_texture = light_probe->get_diffuse_ibl_texture();
			light_data.specluar_ibl_texture = light_probe->get_specular_ibl_texture();
			light_data.lut_ggx_texture = light_probe->get_lut_ggx_texture();

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
			light_data.light.light_probe_data = probe;
			break;
		}
		return light_data;
	}
}

typedef void(ID3D11DeviceContext::* set_constant_buffers)(
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	_In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers);

typedef void(ID3D11DeviceContext::* set_shader_resources)(
	_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	_In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews);

typedef void(ID3D11DeviceContext::* set_sampplers)(
	_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	_In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers);
///////////////////////////////////////////////////////////////////////////////////////////////////////
graphics_custom_pipeline_state::graphics_custom_pipeline_state()
: graphics_pipeline_state()
{
	HRESULT hr{ S_OK };

	auto render_data = render_data::instance();
	auto device = render_data->get_device();

	// 深度ステンシルステートの生成
	{
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_test_and_write)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_test_only)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		depth_stencil_desc.DepthEnable = FALSE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_write_only)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		depth_stencil_desc.DepthEnable = FALSE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_off)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

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
		hr = device->CreateBlendState(&blend_desc, blend_states[static_cast<size_t>(blend_state::opaque)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&blend_desc, blend_states[static_cast<size_t>(blend_state::translucent)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; //D3D11_BLEND_ONE D3D11_BLEND_SRC_ALPHA
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&blend_desc, blend_states[static_cast<size_t>(blend_state::addative)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO; //D3D11_BLEND_DEST_COLOR
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR; //D3D11_BLEND_SRC_COLOR
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&blend_desc, blend_states[static_cast<size_t>(blend_state::multiply)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;

		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[static_cast<int>(raster_state::solid)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[static_cast<int>(raster_state::two_sided)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[static_cast<int>(raster_state::wire)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[static_cast<int>(raster_state::wire_two_sided)].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

graphics_custom_pipeline_state::~graphics_custom_pipeline_state()
{
}

void graphics_custom_pipeline_state::begin(const graphics_context& context)
{
	//	各種データ類更新
	update_register_datas();

	//	シェーダー設定
	if (vs)
	{
		context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
		context.device_context->IASetInputLayout(vs->get_input_layout().Get());
	}
	else
	{
		context.device_context->VSSetShader(nullptr, nullptr, 0);
		context.device_context->IASetInputLayout(nullptr);
	}
	context.device_context->HSSetShader(hs ? hs->get_shader().Get() : nullptr, nullptr, 0);
	context.device_context->DSSetShader(ds ? ds->get_shader().Get() : nullptr, nullptr, 0);
	context.device_context->GSSetShader(gs ? gs->get_shader().Get() : nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps ? ps->get_shader().Get() : nullptr, nullptr, 0);

	//	フラグ類リセット
	for (auto& data : parameter_buffer_data) data.use = false;
	primitive_data.use = false;
	deferred_light_buffer_data.use = false;
	forward_light_buffer_data.use = false;
	shadow_map_buffer_data.use = false;
}

void graphics_custom_pipeline_state::update_parameters(const graphics_context& context, const material& material, const buffer_datas* buffer_data)
{
	//	ラスタライザーステート設定
	{
		bool is_wire = material.is_flag(material::flags::wire);
		bool is_two_sided = material.is_flag(material::flags::two_sided);
		raster_state rs;
		if (is_wire)
		{
			rs = is_two_sided ? raster_state::wire_two_sided : raster_state::wire;
		}
		else
		{
			rs = is_two_sided ? raster_state::two_sided : raster_state::solid;
		}
		context.device_context->RSSetState(rasterizer_states[static_cast<int>(rs)].Get());
	}

	//	ブレンドステート&深度ステンシルステート設定
	{
		auto depth_stencil_state = depth_stencil_states[static_cast<int>(depth_stencil_state::depth_test_and_write)];
		auto blend_state = blend_states[static_cast<int>(blend_state::opaque)];
		if (!context.is_shadow_rendering)	//	@remainder	この判定処理は汚らわしい
		{
			switch (material.get_blend_mode())
			{
				case material::blend_modes::opaque:
				case material::blend_modes::mask:
				{
					break;
				}
				case material::blend_modes::translucent:
				{
					blend_state = blend_states[static_cast<int>(blend_state::translucent)];
					depth_stencil_state = depth_stencil_states[static_cast<int>(depth_stencil_state::depth_test_only)];
					break;
				}
				case material::blend_modes::addative:
				{
					blend_state = blend_states[static_cast<int>(blend_state::addative)];
					depth_stencil_state = depth_stencil_states[static_cast<int>(depth_stencil_state::depth_test_only)];
					break;
				}
				//case material::blend_modes::modulate:
				//{
				//	blend_state = blend_states[static_cast<int>(blend_state::multiply)];
				//	break;
				//}
			}
		}
		context.device_context->OMSetDepthStencilState(depth_stencil_state.Get(), stencil_ref);
		context.device_context->OMSetBlendState(blend_state.Get(), nullptr, 0xFFFFFFFF);
	}

	//	フラグ類リセット
	for (auto& data : parameter_buffer_data) data.use = false;
	primitive_data.use = false;
	deferred_light_buffer_data.use = false;
	forward_light_buffer_data.use = false;
	shadow_map_buffer_data.use = false;

	set_constant_buffers set_constant_buffer_funcs[static_cast<int>(arrangement::max)];
	set_shader_resources set_shader_resource_funcs[static_cast<int>(arrangement::max)];
	set_sampplers set_samppler_funcs[static_cast<int>(arrangement::max)];
	{
		set_constant_buffer_funcs[static_cast<int>(arrangement::vertex)] = &ID3D11DeviceContext::VSSetConstantBuffers;
		set_constant_buffer_funcs[static_cast<int>(arrangement::hull)] = &ID3D11DeviceContext::HSSetConstantBuffers;
		set_constant_buffer_funcs[static_cast<int>(arrangement::domain)] = &ID3D11DeviceContext::DSSetConstantBuffers;
		set_constant_buffer_funcs[static_cast<int>(arrangement::geometry)] = &ID3D11DeviceContext::GSSetConstantBuffers;
		set_constant_buffer_funcs[static_cast<int>(arrangement::pixel)] = &ID3D11DeviceContext::PSSetConstantBuffers;

		set_shader_resource_funcs[static_cast<int>(arrangement::vertex)] = &ID3D11DeviceContext::VSSetShaderResources;
		set_shader_resource_funcs[static_cast<int>(arrangement::hull)] = &ID3D11DeviceContext::HSSetShaderResources;
		set_shader_resource_funcs[static_cast<int>(arrangement::domain)] = &ID3D11DeviceContext::DSSetShaderResources;
		set_shader_resource_funcs[static_cast<int>(arrangement::geometry)] = &ID3D11DeviceContext::GSSetShaderResources;
		set_shader_resource_funcs[static_cast<int>(arrangement::pixel)] = &ID3D11DeviceContext::PSSetShaderResources;

		set_samppler_funcs[static_cast<int>(arrangement::vertex)] = &ID3D11DeviceContext::VSSetSamplers;
		set_samppler_funcs[static_cast<int>(arrangement::hull)] = &ID3D11DeviceContext::HSSetSamplers;
		set_samppler_funcs[static_cast<int>(arrangement::domain)] = &ID3D11DeviceContext::DSSetSamplers;
		set_samppler_funcs[static_cast<int>(arrangement::geometry)] = &ID3D11DeviceContext::GSSetSamplers;
		set_samppler_funcs[static_cast<int>(arrangement::pixel)] = &ID3D11DeviceContext::PSSetSamplers;
	}

	//	パラメーター設定
	{
		//	定数バッファビュー登録
		const material::constant_buffer_map& constant_buffers = material.get_constant_buffers();
		forward_light_data forward_light_data;
		for (auto& cbv : index_map.cbv)
		{
			system_utility::hash_key cbv_key = system_utility::generate_hash(cbv.first);
			if (cbv_key == common_pipeline_states::parameter_constant_buffer_hash_key)
			{
				//	特定のアイテムはcontextから
				for (UINT index = 0; index < static_cast<int>(arrangement::max); ++index)
				{
					if (cbv.second.ids[index] == -1)
						continue;

					parameter_buffer_data[index].use = true;
					ID3D11Buffer* constant_buffer = parameter_buffer_data[index].buffer.Get();
					(context.device_context->*set_constant_buffer_funcs[index])(cbv.second.ids[index], 1, &constant_buffer);
				}
				continue;
			}

			ID3D11Buffer* constant_buffer = nullptr;
			//	特定のアイテムはcontextから
			if (cbv_key == common_pipeline_states::scene_constant_buffer_hash_key)
			{
				constant_buffer = context.scene_constant_buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::shadow_map_constant_buffer_hash_key)
			{
				shadow_map_buffer_data.use = true;
				constant_buffer = shadow_map_buffer_data.buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::deferred_light_constant_buffer_hash_key)
			{
				deferred_light_buffer_data.use = true;
				constant_buffer = deferred_light_buffer_data.buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::forward_light_constant_buffer_hash_key)
			{
				if (!forward_light_buffer_data.use)
				{
					forward_light_data = search_neighborhood_light(context, *buffer_data);
				}
				forward_light_buffer_data.use = true;
				constant_buffer = forward_light_buffer_data.buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::primitive_constant_buffer_hash_key)
			{
				primitive_data.use = true;
				for (UINT index = 0; index < static_cast<int>(arrangement::max); ++index)
				{
					primitive_data.cbv_indices[index] = cbv.second.ids[index];
				}
				continue;
			}
			else
			{
				auto it = constant_buffers.find(cbv.first);
				if (it != constant_buffers.end())
				{
					constant_buffer = it->second.buffer.Get();
				}
			}

			for (UINT index = 0; index < static_cast<int>(arrangement::max); ++index)
			{
				if (cbv.second.ids[index] == -1)
					continue;

				(context.device_context->*set_constant_buffer_funcs[index])(cbv.second.ids[index], 1, &constant_buffer);
			}
		}

		//	シェーダーリソースビュー登録
		common_pipeline_states::parameter_constants parameter_constant_buffer[static_cast<int>(arrangement::max)];
		{
			const material::shader_resource_map& shader_resources = material.get_shader_resources();

			for (auto& srv : index_map.srv)
			{
				system_utility::hash_key srv_key = system_utility::generate_hash(srv.first);
				//	登録用関数
				auto register_srv_function = [&](const texture_ptr& srv_ptr)
				{
					if (!srv_ptr)
						return;
					UINT srv_flag = 0;
					for (auto& flag : convert_texture_flags)
					{
						system_utility::set_bitflag(srv_flag, flag.second, srv_ptr->is_flag(flag.first));
					}

					ID3D11ShaderResourceView* shader_resource_view = srv_ptr ? srv_ptr->get_shader_resource_view().Get() : nullptr;

					for (UINT index = 0; index < static_cast<int>(arrangement::max); ++index)
					{
						if (srv.second.ids[index] == -1)
							continue;

						UINT register_index = srv.second.ids[index];
						(context.device_context->*set_shader_resource_funcs[index])(register_index, 1, &shader_resource_view);
						int* shader_resource_view_index = &parameter_constant_buffer[index].shader_resource_view_index[register_index / 4].x;
						UINT* shader_resource_view_flags = &parameter_constant_buffer[index].shader_resource_view_flags[register_index / 4].x;
						shader_resource_view_index[register_index % 4] = 1;
						shader_resource_view_flags[register_index % 4] = srv_flag;
					}
				};
				//	contextから設定系
				{
					bool is_updated = false;
					auto register_function = [&](const texture_ptr& srv_ptr)
					{
						register_srv_function(srv_ptr);
						is_updated = true;
					};

					for (UINT g_buffer_index = 0; g_buffer_index < ARRAYSIZE(common_pipeline_states::gbuffer_names); ++g_buffer_index)
					{
						if (srv_key == common_pipeline_states::gbuffer_hash_keys[g_buffer_index])
						{
							register_function(context.g_buffer_textures[g_buffer_index]);
							break;
						}
					}
					if (srv_key == common_pipeline_states::forward_only_diffuse_iem_hash_key)
					{
						register_function(forward_light_data.diffuse_ibl_texture);
					}
					else if (srv_key == common_pipeline_states::forward_only_specular_pmrem_hash_key)
					{
						register_function(forward_light_data.specluar_ibl_texture);
					}
					else if (srv_key == common_pipeline_states::forward_only_lut_ggx_hash_key)
					{
						register_function(forward_light_data.lut_ggx_texture);
					}
					else if (srv_key == common_pipeline_states::scene_color_map_hash_key)
					{
						register_function(context.scene_color_texture);
					}
					else if (srv_key == common_pipeline_states::scene_depth_map_hash_key)
					{
						register_function(context.scene_depth_texture);
					}
					else if (srv_key == common_pipeline_states::shadow_maps_hash_key)
					{
						register_function(context.shadow_map_texture);
					}

					if (is_updated)
						continue;
				}

				auto it = shader_resources.find(srv.first);
				if (it == shader_resources.end())
					continue;

				auto& shader_resources = it->second.resources;
				for (UINT srv_index = 0; srv_index < shader_resources.size(); ++srv_index)
				{
					register_srv_function(shader_resources[srv_index].value);
				}
			}
		}

		//	マテリアルのパラメーター系登録
		{
			UINT material_flag = 0;
			for (auto& flag : convert_material_flags)
			{
				system_utility::set_bitflag(material_flag, flag.second, material.is_flag(flag.first));
			}
			for (UINT index = 0; index < static_cast<int>(arrangement::max); ++index)
			{
				constant_buffer_data& data = parameter_buffer_data[index];
				if (!data.use)
					continue;

				//	定数バッファの設定
				auto& cb_data = parameter_constant_buffer[index];
				cb_data.material_shading_model = static_cast<UINT>(material.get_shading_model());
				cb_data.material_alpha_clip_threshold = material.get_alpha_clip_threshold();
				cb_data.material_flags = material_flag;
				cb_data.material_blend_mode = static_cast<UINT>(material.get_blend_mode());

				ID3D11Buffer* constant_buffer = data.buffer.Get();
				context.device_context->UpdateSubresource(constant_buffer, 0, 0, &cb_data, 0, 0);
			}
		}

		//	サンプラーステート設定
		//	一旦は既定の物を入れておく。
		//	必要であれば、マテリアルに持たせるなど、必要に応じて対応してください。
		//	テクスチャ側にサンプラーの情報を設定
		ID3D11SamplerState* sampler_state = render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).Get();
		for (auto& sampler : index_map.sampler)
		{
			for (UINT index = 0; index < static_cast<int>(arrangement::max); ++index)
			{
				if (sampler.second.ids[index] == -1)
					continue;

				UINT register_index = sampler.second.ids[index];
				(context.device_context->*set_samppler_funcs[index])(register_index, 1, &sampler_state);
			}
		}

		//	プリミティブ情報設定
		if (primitive_data.use && buffer_data)
		{
			for (UINT shader_index = 0; shader_index < static_cast<int>(arrangement::max); ++shader_index)
			{
				UINT cbv_index = primitive_data.cbv_indices[shader_index];
				if (cbv_index == -1)
					continue;

				(context.device_context->*set_constant_buffer_funcs[shader_index])(cbv_index, 1, buffer_data->primitive_buffer.GetAddressOf());
			}
		}
		//	シャドウマップ情報設定
		if (shadow_map_buffer_data.use)
		{
			context.device_context->UpdateSubresource(shadow_map_buffer_data.buffer.Get(), 0, 0, &context.shadow_map_constant, 0, 0);
		}
		//	ライト情報設定
		if (deferred_light_buffer_data.use)
		{
			context.device_context->UpdateSubresource(deferred_light_buffer_data.buffer.Get(), 0, 0, &context.deferred_light_constant, 0, 0);
		}
		if (forward_light_buffer_data.use)
		{
			context.device_context->UpdateSubresource(forward_light_buffer_data.buffer.Get(), 0, 0, &forward_light_data.light, 0, 0);
		}
	}
}

void graphics_custom_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<vertex_shader>& shader)
{
	if (vs == shader)
		return;

#if defined(USE_IMGUI)
	if (vs)
		vs->unregister_reload_callback(vs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	vs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (vs)
		vs_reload_callback_id = vs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<hull_shader>& shader)
{
	if (hs == shader)
		return;

#if defined(USE_IMGUI)
	if (hs)
		hs->unregister_reload_callback(hs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	hs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (hs)
		hs_reload_callback_id = hs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<domain_shader>& shader)
{
	if (ds == shader)
		return;

#if defined(USE_IMGUI)
	if (ds)
		ds->unregister_reload_callback(ds_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	ds = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (ds)
		ds_reload_callback_id = ds->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<geometry_shader>& shader)
{
	if (gs == shader)
		return;

#if defined(USE_IMGUI)
	if (gs)
		gs->unregister_reload_callback(gs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	gs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (gs)
		gs_reload_callback_id = gs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<pixel_shader>& shader)
{
	if (ps == shader)
		return;

#if defined(USE_IMGUI)
	if (ps)
		ps->unregister_reload_callback(ps_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	ps = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (ps)
		ps_reload_callback_id = ps->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::generate_input_resource_descs(std::vector<input_resource_desc>& out_data)
{
	if (dirty_input_resource_datas)
	{
		dirty_input_resource_datas = false;
		input_resource_desces.clear();
		input_resource_desc resource_descs[static_cast<int>(arrangement::max)];
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::vertex)], vs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::hull)], hs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::domain)], ds.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::geometry)], gs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::pixel)], ps.get());
		for (auto& resource_desc : resource_descs)
		{
			input_resource_desces.emplace_back(resource_desc);
		}
	}
	out_data = input_resource_desces;

	//	登録位置リスト更新
	update_register_datas();
}

void graphics_custom_pipeline_state::update_register_datas()
{
	HRESULT hr;
	auto device = render_data::instance()->get_device().Get();

	if (dirty_input_resource_datas)
	{
		dirty_input_resource_datas = false;
		input_resource_desces.clear();
		input_resource_desc resource_descs[static_cast<int>(arrangement::max)];
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::vertex)], vs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::hull)], hs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::domain)], ds.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::geometry)], gs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::pixel)], ps.get());
		for (auto& resource_desc : resource_descs)
		{
			input_resource_desces.emplace_back(resource_desc);
		}
	}

	if (dirty)
	{
		dirty = false;
		index_map.clear();
		shader_base* shaders[static_cast<int>(arrangement::max)] =
		{
			vs.get(),
			hs.get(),
			ds.get(),
			gs.get(),
			ps.get()
		};
		for (UINT index = 0; index < ARRAYSIZE(shaders); ++index)
		{
			if (!shaders[index])
				continue;

			for (auto& cbv : input_resource_desces[index].cbvs)
			{
				auto search_func = [&](const std::pair<std::string, index_map::indices>& data)
				{
					return data.first == cbv.name;
				};

				auto it = std::find_if(index_map.cbv.begin(), index_map.cbv.end(), search_func);
				if (it == index_map.cbv.end())
				{
					index_map.cbv.emplace_back(std::make_pair(cbv.name, index_map::indices()));
					it = std::find_if(index_map.cbv.begin(), index_map.cbv.end(), search_func);
				}

				it->second.ids[index] = cbv.register_index;
			}
			for (auto& rv : input_resource_desces[index].rvs)
			{
				auto search_func = [&](const std::pair<std::string, index_map::indices>& data)
				{
					return data.first == rv.name;
				};

				if (rv.is_srvs())
				{
					auto it = std::find_if(index_map.srv.begin(), index_map.srv.end(), search_func);
					if (it == index_map.srv.end())
					{
						index_map.srv.emplace_back(std::make_pair(rv.name, index_map::indices()));
						it = std::find_if(index_map.srv.begin(), index_map.srv.end(), search_func);
					}
					it->second.ids[index] = rv.register_index;
				}
				else if (rv.is_sampler())
				{
					auto it = std::find_if(index_map.sampler.begin(), index_map.sampler.end(), search_func);
					if (it == index_map.sampler.end())
					{
						index_map.sampler.emplace_back(std::make_pair(rv.name, index_map::indices()));
						it = std::find_if(index_map.sampler.begin(), index_map.sampler.end(), search_func);
					}
					it->second.ids[index] = rv.register_index;
				}
			}
		}

		semantic_list.clear();
		for (auto& element_desc : vs->get_input_element_desc())
		{
			semantic_list.emplace_back(std::make_pair(semantic(element_desc.SemanticName, element_desc.SemanticIndex), graphics_utility::get_byte_offset_from_format(element_desc.Format)));
		}

		//	必要な定数バッファを生成する
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;

			for (auto& cbv : index_map.cbv)
			{
				system_utility::hash_key cbv_key = system_utility::generate_hash(cbv.first);

				switch (cbv_key)
				{
					case	common_pipeline_states::parameter_constant_buffer_hash_key:
					{
						for (UINT index = 0; index < ARRAYSIZE(parameter_buffer_data); ++index)
						{
							if (parameter_buffer_data[index].buffer)
								continue;

							buffer_desc.ByteWidth = sizeof(common_pipeline_states::parameter_constants);
							hr = device->CreateBuffer(&buffer_desc, nullptr, parameter_buffer_data[index].buffer.ReleaseAndGetAddressOf());
							_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						}
						break;
					}
					case	common_pipeline_states::shadow_map_constant_buffer_hash_key:
					{
						if (shadow_map_buffer_data.buffer)
							break;

						buffer_desc.ByteWidth = sizeof(common_pipeline_states::shadow_map_constants);
						hr = device->CreateBuffer(&buffer_desc, nullptr, shadow_map_buffer_data.buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						break;
					}
					case	common_pipeline_states::deferred_light_constant_buffer_hash_key:
					{
						if (deferred_light_buffer_data.buffer)
							break;

						buffer_desc.ByteWidth = sizeof(common_pipeline_states::deferred_light_constants);
						hr = device->CreateBuffer(&buffer_desc, nullptr, deferred_light_buffer_data.buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						break;
					}
					case	common_pipeline_states::forward_light_constant_buffer_hash_key:
					{
						if (forward_light_buffer_data.buffer)
							break;

						buffer_desc.ByteWidth = sizeof(common_pipeline_states::forward_light_constants);
						hr = device->CreateBuffer(&buffer_desc, nullptr, forward_light_buffer_data.buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						break;
					}
				}
			}
		}
	}
}

#if defined(USE_IMGUI)
void graphics_custom_pipeline_state::on_update_core(const shader_base* shader_base)
{
	//	更新が入った時に各種再生成
	dirty = true;
	dirty_input_resource_datas = true;
}

bool graphics_custom_pipeline_state::is_failed()
{
	if ((vs && vs->is_failed()) ||
		(hs && hs->is_failed()) ||
		(ds && ds->is_failed()) ||
		(gs && gs->is_failed()) ||
		(ps && ps->is_failed()))
	{
		return true;
	}
	return false;
}
#endif	//	defined(USE_IMGUI)

