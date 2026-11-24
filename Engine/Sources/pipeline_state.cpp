#include "pipeline_state.h"

#include "render_data.h"

namespace
{
	using variable_kind = pipeline_state_base::variable_kinds;
	using variable_type = pipeline_state_base::variable_types;
	using variable_desc = pipeline_state_base::constant_buffer_view_desc::variable_desc;

	variable_kind convert_variable_kind_from_shader_type_desc(const D3D11_SHADER_TYPE_DESC& desc)
	{
		switch (desc.Class)
		{
			case D3D_SVC_SCALAR: return variable_kind::Scalar;
			case D3D_SVC_VECTOR: return variable_kind::Vector;
			case D3D_SVC_MATRIX_ROWS: return variable_kind::MatrixRows;
			case D3D_SVC_MATRIX_COLUMNS: return variable_kind::MatrixColumns;
			case D3D_SVC_STRUCT: return variable_kind::Struct;
		}
		return variable_kind::Unknown;
	}

	variable_type convert_variable_type_from_shader_type_desc(const D3D11_SHADER_TYPE_DESC& desc)
	{
		switch (desc.Type)
		{
			case D3D_SVT_BOOL: return variable_type::Bool;
			case D3D_SVT_INT: return variable_type::Int;
			case D3D_SVT_UINT: return variable_type::UInt;
			case D3D_SVT_FLOAT: return variable_type::Float;
		}
		return variable_type::Unknown;
	}

	UINT get_size_variable_type(const variable_type& type)
	{
		switch (type)
		{
			case variable_type::Bool: return 4;
			case variable_type::Int: return 4;
			case variable_type::UInt: return 4;
			case variable_type::Float: return 4;
		}
		return 0;
	}

	variable_type convert_variable_type_from_srv_simension(const D3D_SRV_DIMENSION& dimension, bool rw)
	{
		if (rw)
		{
			D3D_SVT_RWBUFFER;
			D3D_SVT_TEXTURE;
			switch (dimension)
			{
				case D3D_SRV_DIMENSION_BUFFER:			return variable_type::RWBuffer;
				case D3D_SRV_DIMENSION_BUFFEREX:		return variable_type::RWBufferEx;
				case D3D_SRV_DIMENSION_TEXTURE1D:		return variable_type::RWTexture1d;
				case D3D_SRV_DIMENSION_TEXTURE1DARRAY:	return variable_type::RWTexture1dArray;
				case D3D_SRV_DIMENSION_TEXTURE2D:		return variable_type::RWTexture2d;
				case D3D_SRV_DIMENSION_TEXTURE2DARRAY:	return variable_type::RWTexture2dArray;
				case D3D_SRV_DIMENSION_TEXTURE3D:		return variable_type::RWTexture3d;
			}
			return variable_type::Unknown;
		}
		switch (dimension)
		{
			case D3D_SRV_DIMENSION_BUFFER:				return variable_type::Buffer;
			case D3D_SRV_DIMENSION_BUFFEREX:			return variable_type::BufferEx;
			case D3D_SRV_DIMENSION_TEXTURE1D:			return variable_type::Texture1d;
			case D3D_SRV_DIMENSION_TEXTURE1DARRAY:		return variable_type::Texture1dArray;
			case D3D_SRV_DIMENSION_TEXTURE2D:			return variable_type::Texture2d;
			case D3D_SRV_DIMENSION_TEXTURE2DARRAY:		return variable_type::Texture2dArray;
			case D3D_SRV_DIMENSION_TEXTURE2DMS:			return variable_type::Texture2dMS;
			case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:	return variable_type::Texture2dMSArray;

			case D3D_SRV_DIMENSION_TEXTURE3D:			return variable_type::Texture3d;
			case D3D_SRV_DIMENSION_TEXTURECUBE:			return variable_type::TextureCube;
			case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:	return variable_type::TextureCubeArray;
		}
		return variable_type::Unknown;
	}

	variable_type convert_variable_type_from_shader_input_bind_desc(const D3D11_SHADER_INPUT_BIND_DESC& desc)
	{
		//	基本的なものだけ対応
		switch (desc.Type)
		{
			//	構造化バッファー
			case D3D_SIT_STRUCTURED:
			{
				if (desc.Dimension == D3D_SRV_DIMENSION_BUFFER)
					return variable_type::Buffer;
				if (desc.Dimension == D3D_SRV_DIMENSION_BUFFEREX)
					return variable_type::BufferEx;
				return variable_type::StructuredBuffer;
			}
			case D3D_SIT_UAV_RWSTRUCTURED:
			{
				if (desc.Dimension == D3D_SRV_DIMENSION_BUFFER)
					return variable_type::RWBuffer;
				if (desc.Dimension == D3D_SRV_DIMENSION_BUFFEREX)
					return variable_type::RWBufferEx;
				return variable_type::RWStructuredBuffer;
			}
			case D3D_SIT_UAV_APPEND_STRUCTURED: return variable_type::AppendStructuredBuffer;
			case D3D_SIT_UAV_CONSUME_STRUCTURED: return variable_type::ConsumeStructuredBuffer;
			//	バイトアドレスバッファー
			case D3D_SIT_BYTEADDRESS: return variable_type::ByteAddressBuffer;
			case D3D_SIT_UAV_RWBYTEADDRESS: return variable_type::RWByteAddressBuffer;
			//	テクスチャ系列
			case D3D_SIT_TEXTURE:
			case D3D_SIT_UAV_RWTYPED:
			{
				bool rw = false;
				if (desc.Type == D3D_SIT_UAV_RWTYPED)
					rw = true;
				return convert_variable_type_from_srv_simension(desc.Dimension, rw);
			}
			case D3D_SIT_SAMPLER:
			{
				return variable_type::Sampler;
			}
		}

		return variable_type::Unknown;
	}

	variable_desc generate_variable_desc(ID3D11ShaderReflectionType* srt)
	{
		D3D11_SHADER_TYPE_DESC shader_type_desc;
		HRESULT hr = srt->GetDesc(&shader_type_desc);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		variable_desc v_desc;
		v_desc.name = shader_type_desc.Name;
		v_desc.start_offset = shader_type_desc.Offset;
		v_desc.kind = convert_variable_kind_from_shader_type_desc(shader_type_desc);
		v_desc.type = convert_variable_type_from_shader_type_desc(shader_type_desc);
		v_desc.rows = shader_type_desc.Rows;
		v_desc.columns = shader_type_desc.Columns;
		v_desc.size = get_size_variable_type(v_desc.type) * v_desc.rows * v_desc.columns;

		//	構造体の場合、別途メンバ変数などを登録していく
		if (v_desc.kind == variable_kind::Struct)
		{
			for (UINT member_index = 0; member_index < shader_type_desc.Members; ++member_index)
			{
				auto member = srt->GetMemberTypeByIndex(member_index);
				v_desc.members.emplace_back(generate_variable_desc(member));
				//	メンバ名は取得できない時があるので、ここで取得しておく
				v_desc.members.back().name = srt->GetMemberTypeName(member_index);
			}
		}
		return v_desc;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CEREALバージョン定義
CEREAL_CLASS_VERSION(pipeline_state_base::constant_buffer_view_desc::variable_desc, 1)
CEREAL_CLASS_VERSION(pipeline_state_base::constant_buffer_view_desc, 1)
CEREAL_CLASS_VERSION(pipeline_state_base::resource_view_desc, 1)
CEREAL_CLASS_VERSION(pipeline_state_base::input_resource_desc, 1)

template<class Archive>
void pipeline_state_base::constant_buffer_view_desc::variable_desc::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(start_offset),
			CEREAL_NVP(size),
			CEREAL_NVP(kind),
			CEREAL_NVP(type),
			CEREAL_NVP(rows),
			CEREAL_NVP(columns),
			CEREAL_NVP(members));
}

template<class Archive>
void pipeline_state_base::constant_buffer_view_desc::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(flags),
			CEREAL_NVP(cbv_flags),
			CEREAL_NVP(register_index),
			CEREAL_NVP(total_size),
			CEREAL_NVP(variables));
}

template<class Archive>
void pipeline_state_base::resource_view_desc::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(flags),
			CEREAL_NVP(register_index),
			CEREAL_NVP(array_size),
			CEREAL_NVP(type));
}

template<class Archive>
void pipeline_state_base::input_resource_desc::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(cbvs), CEREAL_NVP(rvs));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
UINT pipeline_state_base::get_variable_type_size(variable_kinds kind, variable_types type, UINT rows, UINT columns)
{
	UINT base_size = 0;
	switch (type)
	{
		case variable_type::Bool:
		case variable_type::Int:
		case variable_type::UInt:
		case variable_type::Float:
		{
			base_size = 4;
			break;
		}
	}

	switch (kind)
	{
		case variable_kind::MatrixRows:		return rows * columns * base_size;
		case variable_kind::MatrixColumns:	return columns * rows * base_size;
		case variable_kind::Vector:			return columns * base_size;
		case variable_kind::Scalar:			return base_size;
	}
	return 0;
}

pipeline_state_base::pipeline_state_base()
: system_flags(0)
, domain(domains::unknown)
{
}

pipeline_state_base::~pipeline_state_base()
{
}

bool pipeline_state_base::is_flag(flags flag) const
{
	return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag));
}

void pipeline_state_base::set_flag(flags flag, bool value)
{
	system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value);
}

void pipeline_state_base::set_name(std::string_view name)
{
	this->name = std::string(name);
}

std::string_view pipeline_state_base::get_name() const
{
	return name;
}

void pipeline_state_base::set_domain(domains domain)
{
	this->domain = domain;
}

pipeline_state_base::domains pipeline_state_base::get_domain() const
{
	return domain;
}

void pipeline_state_base::generate_input_resource_desc(input_resource_desc& out_data, shader_base* shader)
{
	if (!shader)
		return;

	HRESULT hr;
	auto reflection_ptr = std::static_pointer_cast<Microsoft::WRL::ComPtr<ID3D11ShaderReflection>>(shader->get_reflection());
	auto reflection = (*reflection_ptr);

	D3D11_SHADER_DESC shader_desc;
	reflection->GetDesc(&shader_desc);
	for (UINT resource_index = 0; resource_index < shader_desc.BoundResources; ++resource_index)
	{
		//	リソース情報取得
		D3D11_SHADER_INPUT_BIND_DESC input_bind_desc;
		reflection->GetResourceBindingDesc(resource_index, &input_bind_desc);

		switch (input_bind_desc.Type)
		{
			case D3D_SIT_CBUFFER:
			{
				//	同じ名前の物が登録済みであれば除外
				{
					decltype(out_data.cbvs)::iterator it = std::find_if(
						out_data.cbvs.begin(),
						out_data.cbvs.end(),
						[&](const constant_buffer_view_desc& it)
						{
							return it.name == input_bind_desc.Name;
						});
					if (it != out_data.cbvs.end())
					{
						continue;
					}
				}

				//	定数バッファの情報取得
				auto sr_cbv = reflection->GetConstantBufferByName(input_bind_desc.Name);
				D3D11_SHADER_BUFFER_DESC sb_desc;
				hr = sr_cbv->GetDesc(&sb_desc);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

				//	情報設定
				constant_buffer_view_desc cbv_desc = {};
				cbv_desc.name = input_bind_desc.Name;
				cbv_desc.register_index = input_bind_desc.BindPoint;
				cbv_desc.total_size = sb_desc.Size;

				//	フラグ類設定
				system_utility::set_bitflag(cbv_desc.flags, view_flags::packing, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_USERPACKED));
				system_utility::set_bitflag(cbv_desc.flags, view_flags::comparison_sampler, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_COMPARISON_SAMPLER));
				system_utility::set_bitflag(cbv_desc.flags, view_flags::texture_component_0, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_TEXTURE_COMPONENT_0));
				system_utility::set_bitflag(cbv_desc.flags, view_flags::texture_component_1, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_TEXTURE_COMPONENT_1));
				system_utility::set_bitflag(cbv_desc.flags, view_flags::texture_components, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_TEXTURE_COMPONENTS));

				system_utility::set_bitflag(cbv_desc.cbv_flags, cbv_flags::user_packed, system_utility::is_bitflag(sb_desc.uFlags, D3D_CBF_USERPACKED));

				//	メンバ変数取得
				for (UINT variable_index = 0; variable_index < sb_desc.Variables; ++variable_index)
				{
					auto srv = sr_cbv->GetVariableByIndex(variable_index);
					if (!srv)
						continue;

					//	シェーダー変数情報取得
					D3D11_SHADER_VARIABLE_DESC shader_variable_desc;
					hr = srv->GetDesc(&shader_variable_desc);
					_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

					constant_buffer_view_desc::variable_desc cbv_v_desc;
					cbv_v_desc.name = shader_variable_desc.Name;
					cbv_v_desc.start_offset = shader_variable_desc.StartOffset;
					cbv_v_desc.size = shader_variable_desc.Size;
					//shader_variable_desc.DefaultValue;	//	初期値が取れる

					//	型情報取得
					if (auto srt = srv->GetType())
					{
						D3D11_SHADER_TYPE_DESC shader_type_desc;
						hr = srt->GetDesc(&shader_type_desc);
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

						cbv_v_desc.kind = convert_variable_kind_from_shader_type_desc(shader_type_desc);
						cbv_v_desc.type = convert_variable_type_from_shader_type_desc(shader_type_desc);
						cbv_v_desc.rows = shader_type_desc.Rows;
						cbv_v_desc.columns = shader_type_desc.Columns;

						//	構造体の場合、別途メンバ変数などを登録していく
						if (cbv_v_desc.kind == variable_kind::Struct)
						{
							for (UINT member_index = 0; member_index < shader_type_desc.Members; ++member_index)
							{
								auto member = srt->GetMemberTypeByIndex(member_index);
								cbv_v_desc.members.emplace_back(generate_variable_desc(member));
								cbv_v_desc.members.back().name = srt->GetMemberTypeName(member_index);
							}
						}
					}

					cbv_desc.variables.push_back(cbv_v_desc);
				}
				out_data.cbvs.emplace_back(cbv_desc);
				break;
			}
			case D3D_SIT_TEXTURE:
			case D3D_SIT_SAMPLER:
			case D3D_SIT_STRUCTURED:
			case D3D_SIT_UAV_RWSTRUCTURED:
			case D3D_SIT_BYTEADDRESS:
			case D3D_SIT_UAV_RWBYTEADDRESS:
			case D3D_SIT_UAV_APPEND_STRUCTURED:
			case D3D_SIT_UAV_CONSUME_STRUCTURED:
			case D3D_SIT_UAV_RWTYPED:
			case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
			{
				std::string name = input_bind_desc.Name;
				//	配列か判断
				bool is_array = false;
				UINT array_size = 0;
				{
					auto head_pos = name.find("[");
					auto tail_pos = name.find("]");
					if (head_pos != std::string::npos &&
						tail_pos != std::string::npos)
					{
						//	変数名と配列数取得
						std::string size_text = name.substr(head_pos + 1, tail_pos - head_pos - 1);
						name = name.substr(0, head_pos);
						array_size = atoi(size_text.data());
						is_array = true;
					}
				}

				//	登録済みか判断
				decltype(out_data.rvs)::iterator registerd_iterator;
				{
					registerd_iterator = std::find_if(
						out_data.rvs.begin(),
						out_data.rvs.end(),
						[name](const resource_view_desc& it)
						{
							return it.name == name;
						});
				}

				if (registerd_iterator != out_data.rvs.end())
				{
					if (is_array)
					{
						//	配列数を増やす
						resource_view_desc& rv_desc = *registerd_iterator;
						if (rv_desc.array_size <= array_size)
							rv_desc.array_size = array_size;
					}
				}
				else
				{
					resource_view_desc rv_desc;
					rv_desc.name = name;
					rv_desc.register_index = input_bind_desc.BindPoint;
					rv_desc.array_size = input_bind_desc.BindCount;
					system_utility::set_bitflag(rv_desc.flags, view_flags::packing, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_USERPACKED));
					system_utility::set_bitflag(rv_desc.flags, view_flags::comparison_sampler, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_COMPARISON_SAMPLER));
					system_utility::set_bitflag(rv_desc.flags, view_flags::texture_component_0, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_TEXTURE_COMPONENT_0));
					system_utility::set_bitflag(rv_desc.flags, view_flags::texture_component_1, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_TEXTURE_COMPONENT_1));
					system_utility::set_bitflag(rv_desc.flags, view_flags::texture_components, system_utility::is_bitflag(input_bind_desc.uFlags, D3D_SIF_TEXTURE_COMPONENTS));
					rv_desc.type = convert_variable_type_from_shader_input_bind_desc(input_bind_desc);
					out_data.rvs.emplace_back(rv_desc);
				}

				break;
			}
		}
	}
}

#if defined(USE_IMGUI)
void pipeline_state_base::on_updates(const shader_base* shader_base)
{
	on_update_core(shader_base);

	for (auto& data : reload_callbacks)
	{
		data.callback(this);
	}
}

UINT pipeline_state_base::register_reload_callback(std::function<void(const pipeline_state_base*)> callback)
{
	reload_callbacks.emplace_back(callback_id_counter++, callback);
	return reload_callbacks.back().id;
}

void pipeline_state_base::unregister_reload_callback(UINT id)
{
	auto it = std::find_if(
		reload_callbacks.begin(),
		reload_callbacks.end(),
		[id](const callback_data& data) { return data.id == id; }
	);
	if (it == reload_callbacks.end())
		return;
	reload_callbacks.erase(it);
}
#endif	//	defined(USE_IMGUI)

///////////////////////////////////////////////////////////////////////////////////////////////////////
bool pipeline_state_manager::register_pipeline_state(graphics_pipeline_state_ptr& pipeline_state)
{
	std::string key_name = std::string(pipeline_state->get_name());
	auto it = graphics_pipeline_state_map.find(key_name);
	if (it != graphics_pipeline_state_map.end())
	{
		return false;
	}

	graphics_pipeline_state_map.insert(std::make_pair(key_name, pipeline_state));
	return true;
}

graphics_pipeline_state_ptr	pipeline_state_manager::search(std::string_view key)
{
	std::string key_name = std::string(key);
	auto it = graphics_pipeline_state_map.find(key_name);
	if (it != graphics_pipeline_state_map.end())
	{
		return it->second;
	}

	return	nullptr;
}

std::vector<graphics_pipeline_state_ptr> pipeline_state_manager::get_domain_lists(pipeline_state_base::domains domain)
{
	std::vector<graphics_pipeline_state_ptr> lists;
	for (const auto& it : graphics_pipeline_state_map)
	{
		if (it.second->get_domain() != domain ||
			it.second->is_flag(pipeline_state_base::flags::hidden) ||
			it.second->get_name().empty())
			continue;

		lists.emplace_back(it.second);
	}

	// 名前ソート
	std::sort(lists.begin(), lists.end(),
		[&](graphics_pipeline_state_ptr& l, graphics_pipeline_state_ptr& r)
		{
			return l->get_name() < r->get_name();
		});

	return	lists;
}
