#include "material.h"
#include "texture.h"
#include "render_data.h"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/unordered_map.hpp"

namespace
{
	using variable_kind = pipeline_state_base::variable_kinds;
	using variable_type = pipeline_state_base::variable_types;

	bool split_array_element(std::string& inout_text, UINT& out_element)
	{
		auto head_pos = inout_text.find("[");
		auto tail_pos = inout_text.find("]");
		if (head_pos != std::string::npos &&
			tail_pos != std::string::npos)
		{
			//	変数名と配列数取得
			std::string st = inout_text.substr(head_pos + 1, tail_pos - head_pos - 1);
			inout_text = inout_text.substr(0, head_pos);
			out_element = atoi(st.data());
			return true;
		}
		return false;
	}
}

// CEREALバージョン定義
CEREAL_CLASS_VERSION(material::pipeline_state, 1)
CEREAL_CLASS_VERSION(material::constant_buffer::subset, 1)
CEREAL_CLASS_VERSION(material::constant_buffer, 1)
CEREAL_CLASS_VERSION(material::shader_resource, 1)
CEREAL_CLASS_VERSION(material::shader_resource::resource, 1)
CEREAL_CLASS_VERSION(material::physical_datas, 1)
CEREAL_CLASS_VERSION(material, 1)

template<class Archive>
void material::pipeline_state::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name));
}

template<class Archive>
void material::constant_buffer::subset::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(start_offset),
			CEREAL_NVP(size),
			CEREAL_NVP(stride),
			CEREAL_NVP(kind),
			CEREAL_NVP(type),
			CEREAL_NVP(rows),
			CEREAL_NVP(columns));
}

template<class Archive>
void material::constant_buffer::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(data_buffer), CEREAL_NVP(subsets), CEREAL_NVP(identifieres));
}

template<class Archive>
void material::shader_resource::resource::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(is_file_path_or_key), CEREAL_NVP(path));
}

template<class Archive>
void material::shader_resource::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(resources));
}

template<class Archive>
void material::physical_datas::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(friction));
}

material::material()
: name()
, domain(domains::unknown)
, blend_mode(blend_modes::opaque)
, blendable_location(blendable_locations::after_dof_translucency)
, translucency_path(translucency_paths::after_dof)
, shading_model(shading_models::unlit)
, pipeline_states()
, system_flags(0)
, alpha_clip_threshold(0.5f)
, physical_data()
, shader_resources()
, constant_buffers()
{
	set_flag(flags::shadow_caster, true);
	set_flag(flags::shadow_receiver, true);
#if	defined(USE_IMGUI)
	//	エラー表示用シェーダー読み込み
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		//	surface用
		failed_surface_psos[static_cast<UINT>(surface_paths::depth_write_pass)] = instance->search("3d_failed_depth");
		failed_surface_psos[static_cast<UINT>(surface_paths::shadow_write_pass)] = instance->search("3d_failed_shadowmap");
		failed_surface_psos[static_cast<UINT>(surface_paths::g_buffer_pass)] = instance->search("3d_failed_gbuffer");
		failed_surface_psos[static_cast<UINT>(surface_paths::forward_pass)] = instance->search("3d_failed_forward");
		//	デカール用
		failed_deferred_decal_pso = instance->search("3d_failed_forward");
		//	ポストプロセス用
		failed_post_process_pso = instance->search("postprocess_failed");
		//	UI用
		failed_user_interface_pso = instance->search("default_ui");
	}
#endif	//	defined(USE_IMGUI)
}

material::material(std::string_view name)
: material()
{
	this->name = name;
}

material::material(const material& material)
{
	material.duplicate(*this);
}

material::material(material&& material)
{
	material.duplicate(*this);
}

material& material::operator =(const material& material)
{
	material.duplicate(*this);
	return *this;
}

material::~material()
{
#if defined(USE_IMGUI)
	//	パイプラインステート側への検知関数登録解除
	for (auto& pipeline_state : pipeline_states)
	{
		if (pipeline_state.value)
			pipeline_state.value->unregister_reload_callback(pipeline_state.reload_callback_id);
	}
#endif	//	defined(USE_IMGUI)
}

void material::update_parameters(const graphics_context& context)
{
#if defined(USE_IMGUI)
	if (is_error())
	{
		return;
	}
#endif	//	defined(USE_IMGUI)

	//	定数バッファ設定
	for (auto& cb : constant_buffers)
	{
		if (cb.second.dirty)
		{
			context.device_context->UpdateSubresource(cb.second.buffer.Get(), 0, 0, cb.second.data_buffer.data(), 0, 0);
			cb.second.dirty = false;
		}
	}
}

template<class Archive>
void material::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(domain),
			CEREAL_NVP(blend_mode),
			CEREAL_NVP(shading_model),
			CEREAL_NVP(blendable_location),
			CEREAL_NVP(translucency_path),
			CEREAL_NVP(system_flags),
			CEREAL_NVP(alpha_clip_threshold),

			CEREAL_NVP(physical_data),
			CEREAL_NVP(pipeline_states),
			CEREAL_NVP(shader_resources),
			CEREAL_NVP(constant_buffers));
}

template<>
void material::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(domain),
			CEREAL_NVP(blend_mode),
			CEREAL_NVP(shading_model),
			CEREAL_NVP(blendable_location),
			CEREAL_NVP(translucency_path),
			CEREAL_NVP(system_flags),
			CEREAL_NVP(alpha_clip_threshold),

			CEREAL_NVP(physical_data),
			CEREAL_NVP(pipeline_states),
			CEREAL_NVP(shader_resources),
			CEREAL_NVP(constant_buffers));
}

template<>
void material::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(domain),
			CEREAL_NVP(blend_mode),
			CEREAL_NVP(shading_model),
			CEREAL_NVP(blendable_location),
			CEREAL_NVP(translucency_path),
			CEREAL_NVP(system_flags),
			CEREAL_NVP(alpha_clip_threshold),

			CEREAL_NVP(physical_data),
			CEREAL_NVP(pipeline_states),
			CEREAL_NVP(shader_resources),
			CEREAL_NVP(constant_buffers));
}

void material::duplicate(material& material) const
{
	std::stringstream iostream;
	serialize_stream(iostream);
	material.deserialize_stream(iostream);
	material.build();
}

void material::build()
{
	//	パイプラインステート登録
	for (auto& pipeline_state : pipeline_states)
	{
		if (auto state = pipeline_state_manager::instance()->search(pipeline_state.name))
		{
#if defined(USE_IMGUI)
			if (pipeline_state.value)
				pipeline_state.value->unregister_reload_callback(pipeline_state.reload_callback_id);
#endif	//	defined(USE_IMGUI)

			pipeline_state.value = state;

#if defined(USE_IMGUI)
			if (pipeline_state.value)
				pipeline_state.reload_callback_id = pipeline_state.value->register_reload_callback(
				[this](const pipeline_state_base* ps) { on_updates(ps); });
#endif	//	defined(USE_IMGUI)
		}
	}

	//	マテリアル設定
	build_parameters();
}

void material::build_parameters()
{
#if defined(USE_IMGUI)
	if (is_error())
	{
		//	エラーならビルドしない
		return;
	}
#endif	//	defined(USE_IMGUI)

	//	退避
	auto temp_shader_resources = shader_resources;
	auto temp_constant_buffers = constant_buffers;
	shader_resources.clear();
	constant_buffers.clear();

	//	パイプラインステートから入力リソース情報を取得
	input_resource_desc resource_desc;
	for (auto& pipeline_state : pipeline_states)
	{
		if (!pipeline_state.is_valid())
			continue;

		std::vector<input_resource_desc> descs;
		pipeline_state.value->generate_input_resource_descs(descs);

		//	名前が一緒の物は除外するようにしておく
		for (auto& desc : descs)
		{
			//	定数バッファ
			for (auto& cbv : desc.cbvs)
			{
				bool find = false;
				for (auto exclude_names : common_pipeline_states::system_constant_buffer_names)
				{
					if (cbv.name == exclude_names)
					{
						find = true;
						break;
					}
				}
				if (find)
					continue;

				decltype(resource_desc.cbvs)::iterator it = std::find_if(
					resource_desc.cbvs.begin(),
					resource_desc.cbvs.end(),
					[&](const decltype(resource_desc.cbvs)::value_type& it)
					{
						return it.name == cbv.name;
					});
				if (it == resource_desc.cbvs.end())
				{
					resource_desc.cbvs.emplace_back(cbv);
				}
			}

			//	リソース類
			for (auto& rv : desc.rvs)
			{
				bool find = false;
				for (auto exclude_names : common_pipeline_states::system_shader_resource_names)
				{
					if (rv.name == exclude_names)
					{
						find = true;
						break;
					}
				}
				if (find)
					continue;

				decltype(resource_desc.rvs)::iterator it = std::find_if(
					resource_desc.rvs.begin(),
					resource_desc.rvs.end(),
					[&](const decltype(resource_desc.rvs)::value_type& it)
					{
						return it.name == rv.name;
					});
				if (it == resource_desc.rvs.end())
				{
					resource_desc.rvs.emplace_back(rv);
				}
			}
		}
	}

	//	テクスチャ情報を構築
	for (auto& rv : resource_desc.rvs)
	{
		if (!rv.is_valid())
			continue;

		if (rv.is_srvs())
		{
			//	登録済みであれば除外
			auto it = shader_resources.find(rv.name);
			if (it != shader_resources.end())
				continue;

			//	空のテクスチャを登録
			shader_resource sr = {};
			sr.resources.resize(rv.array_size);
			//	@remainder	ダミーで黒テクスチャを入れていたが、シェーダー側で未登録の場合は使用できないようにした
			//for (auto& resource : sr.resources)
			//{
			//	resource.is_file_path_or_key = false;
			//	resource.path = texture_manager::black_texture2d_name;
			//	resource.value = texture_manager::instance()->search(texture_manager::black_texture2d_name);
			//}
			shader_resources.insert(std::make_pair(rv.name, sr));
		}
	}

	//	定数バッファ情報を構築
	for (auto& cbv : resource_desc.cbvs)
	{
		constant_buffer cb;
		cb.data_buffer.resize(cbv.total_size);
		memset(cb.data_buffer.data(), 0, cb.data_buffer.size());

		//	識別子登録処理
		{
			std::function<UINT(const variable_desc&, std::string_view, UINT)> register_data_function;
			register_data_function = [&](const variable_desc& desc, std::string_view root_name, UINT start_offset) -> UINT
			{
				std::string variable_name = std::string(root_name);
				variable_name += desc.name;

				constant_buffer::subset subset;
				subset.start_offset = start_offset + desc.start_offset;
				subset.size = desc.size;
				subset.kind = desc.kind;
				subset.type = desc.type;
				subset.rows = desc.rows;
				subset.columns = desc.columns;

				if (desc.kind == pipeline_state_base::variable_kinds::Struct)
				{
					UINT stride = 0;
					for (auto member : desc.members)
					{
						UINT size = register_data_function(member, variable_name + ".", subset.start_offset);
						stride += member.size == 0 ? size : member.size;
					}
					subset.stride = stride;
				}
				else
				{
					subset.stride = pipeline_state_base::get_variable_type_size(desc.kind, desc.type, desc.rows, desc.columns);
				}
				cb.identifieres.insert(std::make_pair(variable_name, cb.subsets.size()));
				cb.subsets.emplace_back(subset);

				return math::alignment(subset.stride, 0x0Fu);	//	16バイトアライメント
			};

			for (auto& variable : cbv.variables)
			{
				register_data_function(variable, "", 0);
			}
		}

		//	定数バッファの生成
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;
			buffer_desc.ByteWidth = cbv.total_size;
			HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, cb.buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	定数バッファ登録
		constant_buffers.insert(std::make_pair(cbv.name, std::move(cb)));
	}

	//	前回情報を登録
	{
		for (auto& texture : temp_shader_resources)
		{
			auto it = shader_resources.find(texture.first);
			if (it == shader_resources.end())
				continue;

			for (UINT index = 0; index < texture.second.resources.size(); ++index)
			{
				auto& temp_resource = texture.second.resources[index];
				auto& resource = it->second.resources[index];
				if (temp_resource.is_valid())
				{
					resource = temp_resource;
				}
				else if (!temp_resource.path.empty())
				{
					resource.is_file_path_or_key = temp_resource.is_file_path_or_key;
					resource.path = temp_resource.path;
					if (resource.is_file_path_or_key)
					{
						texture_manager::instance()->load_texture_from_file(resource.value, temp_resource.path);
					}
					else
					{
						resource.value = texture_manager::instance()->search(temp_resource.path);
					}
				}
			}
		}
		for (auto& constant_buffer : temp_constant_buffers)
		{
			for (auto& identifier : constant_buffer.second.identifieres)
			{
				auto& subset = constant_buffer.second.subsets[identifier.second];
				set_data(identifier.first, constant_buffer.second.data_buffer.data() + subset.start_offset, subset.size);
			}
		}
	}
}

void material::set_name(std::string_view name)
{
	this->name = std::string(name);
}

std::string_view material::get_name() const
{
	return name;
}

material::domains material::get_domain() const
{
	return domain;
}

void material::set_domain(material::domains domain)
{
	if (this->domain == domain)
		return;

	this->domain = domain;
	pipeline_states.clear();
	std::size_t size = 0; 
	switch (domain)
	{
		case material::domains::surface: size = magic_enum::enum_count<surface_paths>() + 1; break;
		case material::domains::post_process: size = 1; break;
		case material::domains::deferred_decal: size = 1; break;
		case material::domains::user_interface: size = 1; break;

		case material::domains::system: size = magic_enum::enum_count<system_paths>() + 1; break;
		default: assert(!L"Not Compatible."); break;
	}

	pipeline_states.resize(size);
}

material::blend_modes material::get_blend_mode() const
{
	return blend_mode;
}

void material::set_blend_mode(blend_modes blend_mode)
{
	this->blend_mode = blend_mode;
}

material::shading_models material::get_shading_model() const
{
	return shading_model;
}

void material::set_shading_model(shading_models shading_model)
{
	this->shading_model = shading_model;
}

material::blendable_locations material::get_blendable_location() const
{
	return domain == domains::post_process ? blendable_location : blendable_locations::unknown;
}

void material::set_blendable_location(blendable_locations location)
{
	if (domain == domains::post_process) blendable_location = location;
}

material::translucency_paths material::get_translucency_path() const
{
	if (domain == domains::surface && blend_mode == blend_modes::translucent)
		return translucency_path;

	return translucency_paths::unknown;
}

void material::set_translucency_path(translucency_paths pass)
{
	if (domain == domains::surface && blend_mode == blend_modes::translucent)
		translucency_path = pass;
}

const material::constant_buffer_map& material::get_constant_buffers() const
{
	return constant_buffers;
}

const material::shader_resource_map& material::get_shader_resources() const
{
	return shader_resources;
}

#if defined(USE_IMGUI)
material::constant_buffer_map& material::get_constant_buffers_for_editor()
{
	return constant_buffers;
}

material::shader_resource_map& material::get_shader_resources_for_editor()
{
	return shader_resources;
}
#endif	//	defined(USE_IMGUI)

bool material::set_pipeline_state(const set_surface_pipeline_states_desc* descs, UINT size)
{
	//	ドメインが違えば登録させない
	if (this->domain != domains::surface)
		return false;

	struct set_surface_pipeline_states_desc
	{
		surface_paths type;
		std::string_view pipeline_state_name;
	};
	bool registered = false;
	for (UINT desc_index = 0; desc_index < size; ++desc_index)
	{
		UINT index = static_cast<int>(descs[desc_index].type);
		std::string name = std::string(descs[desc_index].pipeline_state_name);

		if (pipeline_states.size() >= index)
		{
			pipeline_states.resize(index + 1);
		}

		if (pipeline_states[index].name != name)
		{
			if (auto pipeline_state = pipeline_state_manager::instance()->search(name))
			{
				pipeline_states[index].name = name;
#if defined(USE_IMGUI)
				if (pipeline_states[index].value)
					pipeline_states[index].value->unregister_reload_callback(pipeline_states[index].reload_callback_id);
#endif	//	defined(USE_IMGUI)

				pipeline_states[index].value = pipeline_state;

#if defined(USE_IMGUI)
				if (pipeline_states[index].value)
					pipeline_states[index].reload_callback_id = pipeline_states[index].value->register_reload_callback([this](const pipeline_state_base* ps) { this->on_updates(ps); });
#endif	//	defined(USE_IMGUI)
				registered = true;
			}
		}
	}

	//	マテリアルパラメーターの更新
	if (registered)
		build_parameters();

	return registered;
}

bool material::set_pipeline_state(domains domain, UINT index, std::string_view pipeline_state_name)
{
	//	ドメインが違えば登録させない
	if(this->domain != domain)
		return false;

	std::string name = std::string(pipeline_state_name);
	if (pipeline_states[index].name != name)
	{
		if (auto pipeline_state = pipeline_state_manager::instance()->search(name))
		{
			pipeline_states[index].name = name;
#if defined(USE_IMGUI)
			if (pipeline_states[index].value)
				pipeline_states[index].value->unregister_reload_callback(pipeline_states[index].reload_callback_id);
#endif	//	defined(USE_IMGUI)

			pipeline_states[index].value = pipeline_state;

#if defined(USE_IMGUI)
			if (pipeline_states[index].value)
				pipeline_states[index].reload_callback_id = pipeline_states[index].value->register_reload_callback([this](const pipeline_state_base* ps) { on_updates(ps); });
#endif	//	defined(USE_IMGUI)

			//	マテリアルパラメーターの更新
			build_parameters();
			return true;
		}
		else
		{
			//	見つからない場合は解除する
			pipeline_states[index].name = name;
#if defined(USE_IMGUI)
			if (pipeline_states[index].value)
				pipeline_states[index].value->unregister_reload_callback(pipeline_states[index].reload_callback_id);
#endif	//	defined(USE_IMGUI)

			pipeline_states[index].value = pipeline_state;

			//	マテリアルパラメーターの更新
			build_parameters();
		}
	}
	return false;
}

bool material::set_pipeline_state(surface_paths type, std::string_view pipeline_state_name)
{
	return set_pipeline_state(domains::surface, static_cast<UINT>(type), pipeline_state_name);
}

bool material::set_pipeline_state(system_paths type, std::string_view pipeline_state_name)
{
	return set_pipeline_state(domains::system, static_cast<UINT>(type), pipeline_state_name);
}

bool material::set_pipeline_state(std::string_view pipeline_state_name)
{
	assert(domain != domains::surface || domain != domains::system);
	return set_pipeline_state(domain, 0, pipeline_state_name);
}

std::string_view material::get_pipeline_state_name(surface_paths type) const
{
	return get_pipeline_state_name(domains::surface, static_cast<UINT>(type));
}

std::string_view material::get_pipeline_state_name(system_paths type) const
{
	return get_pipeline_state_name(domains::system, static_cast<UINT>(type));
}

std::string_view material::get_pipeline_state_name() const
{
	assert(domain == domains::surface || domain == domains::system);
	return get_pipeline_state_name(domain, 0);
}

graphics_pipeline_state_ptr material::get_pipeline_state(surface_paths value) const
{
	return get_pipeline_state(domains::surface, static_cast<UINT>(value));
}

graphics_pipeline_state_ptr material::get_pipeline_state(system_paths value) const
{
	return get_pipeline_state(domains::system, static_cast<UINT>(value));
}

graphics_pipeline_state_ptr material::get_pipeline_state() const
{
	assert(domain != domains::surface || domain != domains::system);
	return get_pipeline_state(domain, 0);
}

std::string_view material::get_pipeline_state_name(domains domain, UINT index) const
{
	if(this->domain != domain)
		return "";
	return pipeline_states[index].name;
}

graphics_pipeline_state_ptr material::get_pipeline_state(domains domain, UINT index) const
{
	if (this->domain != domain)
		return nullptr;

#if defined(USE_IMGUI)
	//	エラーチェック
	if (is_error())
	{
		switch (this->domain)
		{
			case domains::surface: return failed_surface_psos[index];
			case domains::deferred_decal: return failed_deferred_decal_pso;
			case domains::post_process: return failed_post_process_pso;
			case domains::user_interface: return failed_user_interface_pso;
		}
	}
#endif	//	defined(USE_IMGUI)

	return pipeline_states[index].value;
}

bool material::set_data(std::string_view key_name, void* data, std::size_t data_size)
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = constant_buffers.begin();
	while (it != constant_buffers.end())
	{
		auto& cb = it->second;
		auto it_data = cb.identifieres.find(key);
		if (it_data != cb.identifieres.end())
		{
			auto& subset = cb.subsets[it_data->second];

			std::size_t start_offset = subset.start_offset + subset.stride * array_element;

			std::size_t buffer_size = cb.data_buffer.size() - start_offset;
			void* buffer = cb.data_buffer.data() + start_offset;
			memcpy_s(buffer, buffer_size, data, std::min(buffer_size, data_size));
			cb.dirty = true;
			return true;
		}
		++it;
	}
	return false;
}

bool material::get_data(std::string_view key_name, void* data, std::size_t data_size) const
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = constant_buffers.begin();
	while (it != constant_buffers.end())
	{
		auto& cb = it->second;
		auto it_data = cb.identifieres.find(key);
		if (it_data != cb.identifieres.end())
		{
			if (data)
			{
				auto& subset = cb.subsets[it_data->second];

				std::size_t start_offset = subset.start_offset + subset.stride * array_element;

				std::size_t buffer_size = cb.data_buffer.size() - start_offset;
				const void* buffer = cb.data_buffer.data() + start_offset;
				memcpy_s(data, data_size, buffer, std::min(data_size, buffer_size));
			}
			return true;
		}
		++it;
	}
	return false;
}

bool material::set_texture(std::string_view key_name, texture_ptr value)
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = shader_resources.find(key);
	if (it != shader_resources.end())
	{
		//	要素数をoオーバーしていたら除外
		if (it->second.resources.size() <= array_element)
			return false;

		auto& resource = it->second.resources[array_element];
		resource.is_file_path_or_key = value->is_file_path_or_key();
		resource.path = std::string(value->get_file_path_or_key());
		resource.value = value;
		return true;
	}
	return false;
}

bool material::get_texture(std::string_view key_name, texture_ptr& value) const
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = shader_resources.find(key);
	if (it != shader_resources.end())
	{
		//	要素数をoオーバーしていたら除外
		if (it->second.resources.size() <= array_element)
			return false;

		auto& resource = it->second.resources[array_element];
		value = resource.value;
		return true;
	}
	return false;
}

void material::serialize_stream(std::stringstream& ostream) const
{
	cereal::JSONOutputArchive archive(ostream);

	try
	{
		archive(CEREAL_NVP(name),
				CEREAL_NVP(domain),
				CEREAL_NVP(blend_mode),
				CEREAL_NVP(shading_model),
				CEREAL_NVP(blendable_location),
				CEREAL_NVP(translucency_path),
				CEREAL_NVP(physical_data),
				CEREAL_NVP(pipeline_states),

				CEREAL_NVP(system_flags),
				CEREAL_NVP(alpha_clip_threshold),
				CEREAL_NVP(shader_resources),
				CEREAL_NVP(constant_buffers));
	}
	catch (...)
	{
		DEBUG_LOG_CATEGORY(logger::categorys::model, "material serialize_stream failed[%s]", name.data());
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::model, "material serialize_stream succeeded[%s]", name.data());
}

void material::deserialize_stream(std::stringstream& istream)
{
	cereal::JSONInputArchive archive(istream);

	try
	{
		archive(CEREAL_NVP(name),
				CEREAL_NVP(domain),
				CEREAL_NVP(blend_mode),
				CEREAL_NVP(shading_model),
				CEREAL_NVP(blendable_location),
				CEREAL_NVP(translucency_path),
				CEREAL_NVP(physical_data),
				CEREAL_NVP(pipeline_states),

				CEREAL_NVP(system_flags),
				CEREAL_NVP(alpha_clip_threshold),
				CEREAL_NVP(shader_resources),
				CEREAL_NVP(constant_buffers));
	}
	catch (...)
	{
		DEBUG_LOG_CATEGORY(logger::categorys::model, "material deserialize_stream failed[%s]", name.data());
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::model, "material deserialize_stream succeeded[%s]", name.data());
}

#if defined(USE_IMGUI)
bool material::is_error() const
{
	std::size_t size = 0;
	switch (domain)
	{
		case material::domains::surface: size = magic_enum::enum_count<surface_paths>(); break;
		case material::domains::post_process: size = 1; break;
		case material::domains::deferred_decal: size = 1; break;
		case material::domains::user_interface: size = 1; break;

		case material::domains::system: size = magic_enum::enum_count<system_paths>(); break;
		default: assert(!L"Not Compatible."); break;
	}
	for (UINT index = 0; index < size; ++index)
	{
		auto& pipeline_state = pipeline_states[index];

		if (!pipeline_state.value)
			return true;

		if (pipeline_state.value->is_failed())
			return true;
	}

	return false;
}
void material::on_updates(const pipeline_state_base* ps)
{
	//	パラメーター再生成
	build_parameters();
}

#endif	//	defined(USE_IMGUI)

