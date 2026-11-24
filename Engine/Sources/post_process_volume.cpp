#include "post_process_volume.h"
#include "texture.h"
#include "material.h"
#include "collision/box.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(USE_IMGUI)

bloom_data::bloom_data()
{
	texture_manager::instance()->load_texture_from_file(image_kernel, "Assets/Resources/Textures/comvolution_bloom_default_kernel.png");
}

bool bloom_data::is_active() const
{
	return use;
}

void bloom_data::set_active(bool flag)
{
	if (use != flag)
	{
		use = flag;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
lens_ghosts_data::lens_ghosts_data()
{
	texture_manager::instance()->load_texture_from_file(image_kernel, "Assets/Resources/Textures/lens_ghost_default_kernel.png");
	texture_manager::instance()->load_texture_from_file(ghost_color_lut, "Assets/Resources/Textures/lens_ghost_color_lut.png");
}

bool lens_ghosts_data::is_active() const
{
	return use;
}

void lens_ghosts_data::set_active(bool flag)
{
	if (use != flag)
	{
		use = flag;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
float depth_of_field_data::calculate_f_number() const
{
	return focal_length / apeture;
}

bool depth_of_field_data::is_active() const
{
	return use;
}

void depth_of_field_data::set_active(bool flag)
{
	if (use != flag)
	{
		use = flag;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
screen_space_reflection_data::screen_space_reflection_data()
{
	set_flag(common_pipeline_states::screen_space_reflection_flags_consideration_uv_hole, false);
	set_flag(common_pipeline_states::screen_space_reflection_flags_reflection_dir_adjust, true);
	set_flag(common_pipeline_states::screen_space_reflection_flags_step_random, true);
	set_flag(common_pipeline_states::screen_space_reflection_flags_substep_check, true);
}

bool screen_space_reflection_data::is_flag(UINT ssr_flag) const
{
	return system_utility::is_bitflag(ray_flags, 1 << ssr_flag);
}

void screen_space_reflection_data::set_flag(UINT ssr_flag, bool value)
{
	system_utility::set_bitflag(ray_flags, 1 << ssr_flag, value);

	//	乱数を適応する場合はデノイズするので蓄積をリセットしておく
	if (ssr_flag == common_pipeline_states::screen_space_reflection_flags_step_random && is_flag(ssr_flag))
	{
		accumalate_reset = true;
	}
}
bool screen_space_reflection_data::is_active() const
{
	return use;
}

void screen_space_reflection_data::set_active(bool flag)
{
	if (use != flag)
	{
		use = flag;
		//	乱数を適応する場合はデノイズするので蓄積をリセットしておく
		if (is_flag(common_pipeline_states::screen_space_reflection_flags_step_random))
		{
			accumalate_reset = true;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool motion_blur_data::is_active() const
{
	return use;
}

void motion_blur_data::set_active(bool flag)
{
	if (use != flag)
	{
		use = flag;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool exposure_compensation_data::is_active() const
{
	return use;
}

void exposure_compensation_data::set_active(bool flag)
{
	if (use != flag)
	{
		use = flag;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace tone_mapping
{
	constexpr float aces_krzysztof_narkowicz[7] =
	{
		2.51f,
		0.03f,
		2.43f,
		0.59f,
		0.14f,
		0,
		0,
	};
	constexpr float aces_stephen_hill[7] =
	{
		0.0245786f,
		0.000090537f,
		0.983729f,
		0.4329510f,
		0.238081f,
		0,
		0,
	};
	constexpr float gran_turismo[7] =
	{
		1.0f,
		1.0f,
		0.22f,
		0.4f,
		1.33f,
		0.0f,
		0,
	};
}

tone_mapping_data::tone_mapping_data()
{
	set_tone_map_type(tone_mapping_type::none);
}

tone_mapping_data::tone_mapping_type tone_mapping_data::get_tone_map_type() const
{
	return tone_map_type;
}

void tone_mapping_data::set_tone_map_type(tone_mapping_type type)
{
	if (tone_map_type == type)
		return;

	tone_map_type = type;
	switch (tone_map_type)
	{
		case tone_mapping_type::aces_krzysztof_narkowicz:
		{
			memcpy_s(tone_mapping_variables, sizeof(tone_mapping_variables),
				tone_mapping::aces_krzysztof_narkowicz, sizeof(tone_mapping::aces_krzysztof_narkowicz));
			break;
		}
		case tone_mapping_type::aces_stephen_hill:
		{
			memcpy_s(tone_mapping_variables, sizeof(tone_mapping_variables),
				tone_mapping::aces_stephen_hill, sizeof(tone_mapping::aces_stephen_hill));
			break;
		}
		case tone_mapping_type::gran_turismo:
		{
			memcpy_s(tone_mapping_variables, sizeof(tone_mapping_variables),
				tone_mapping::gran_turismo, sizeof(tone_mapping::gran_turismo));
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
post_process_volume::post_process_volume()
: system_flags(0)

, position(0.0f, 0.0f, 0.0f)
, inner_range(50.0f, 50.0f, 50.0f)
, outer_range(100.0f, 100.0f, 100.0f)
, color(1, 1, 1, 1)
{
	set_flag(flags::active, true);
}

post_process_volume::~post_process_volume()
{
}

DirectX::XMFLOAT3 post_process_volume::get_position() const
{
	return position;
}

void post_process_volume::set_position(const DirectX::XMFLOAT3& position)
{
	this->position = position;
}

DirectX::XMFLOAT3 post_process_volume::get_inner_range() const
{
	return inner_range;
}

void post_process_volume::set_inner_range(const DirectX::XMFLOAT3& inner_range)
{
	this->inner_range = inner_range;
}

DirectX::XMFLOAT3 post_process_volume::get_outer_range() const
{
	return outer_range;
}

void post_process_volume::set_outer_range(const DirectX::XMFLOAT3& outer_range)
{
	this->outer_range = outer_range;
}

const DirectX::XMFLOAT4& post_process_volume::get_color() const
{
	return color;
}

void post_process_volume::set_color(const DirectX::XMFLOAT4& color)
{
	this->color = color;
}

const bloom_data& post_process_volume::get_bloom_constant() const
{
	return bloom_constant;
}

void post_process_volume::set_bloom_constant(const bloom_data& data)
{
	bloom_constant = data;
}

const lens_ghosts_data& post_process_volume::get_lens_ghosts_constant() const
{
	return lens_ghosts_constant;
}

void post_process_volume::set_lens_ghosts_constant(const lens_ghosts_data& data)
{
	lens_ghosts_constant = data;
}

const depth_of_field_data& post_process_volume::get_depth_of_field_constant() const
{
	return depth_of_field_constant;
}

void post_process_volume::set_depth_of_field_constant(const depth_of_field_data& data)
{
	depth_of_field_constant = data;
}

const screen_space_reflection_data& post_process_volume::get_screen_space_reflection_constant() const
{
	return screen_space_reflection_constant;
}

void post_process_volume::set_screen_space_reflection_constant(const screen_space_reflection_data& data)
{
	screen_space_reflection_constant = data;
}

const motion_blur_data& post_process_volume::get_motion_blur_constant() const
{
	return motion_blur_constant;
}

void post_process_volume::set_motion_blur_constant(const motion_blur_data& data)
{
	motion_blur_constant = data;
}

const exposure_compensation_data& post_process_volume::get_exposure_compensation_constant() const
{
	return exposure_compensation_constant;
}

void post_process_volume::set_exposure_compensation_constant(const exposure_compensation_data& data)
{
	exposure_compensation_constant = data;
}

const tone_mapping_data& post_process_volume::get_tone_mapping_constant() const
{
	return tone_mapping_constant;
}

void post_process_volume::set_tone_mapping_constant(const tone_mapping_data& data)
{
	tone_mapping_constant = data;
}

const std::vector<std::shared_ptr<material>>& post_process_volume::get_materials() const
{
	return postprocess_materials;
}

bool post_process_volume::register_postprocess_materials(std::shared_ptr<material> material_ptr)
{
	assert(material_ptr->get_domain() == material::domains::post_process);
	if (material_ptr->get_domain() != material::domains::post_process)
		return false;

	postprocess_materials.push_back(material_ptr);
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
post_process_volume_manager::post_process_volume_manager()
{
}

post_process_volume_manager::~post_process_volume_manager()
{
}

bool post_process_volume_manager::generate_post_process_volume(post_process_volume_ptr& out_post_process_volume)
{
	out_post_process_volume = std::make_shared<post_process_volume>();
	post_process_volumes.emplace_back(out_post_process_volume);
	return true;
}

void post_process_volume_manager::get_container(post_process_volume_container& container)
{
	//	ボリュームリストを生成
	{
		container.volumes.clear();
		auto it = post_process_volumes.begin();
		for (; it != post_process_volumes.end();)
		{
			auto instance = it->lock();
			if (!instance)
			{
				it = post_process_volumes.erase(it);
				continue;
			}

			if (!instance->is_flag(post_process_volume::flags::active))
			{
				++it;
				continue;
			}

			container.volumes.emplace_back(instance);
			++it;
		}
	}

#if defined(USE_IMGUI)
	//	範囲表示
	{
		auto it = post_process_volumes.begin();
		for (; it != post_process_volumes.end(); ++it)
		{
			auto instance = it->lock();

			if (instance->is_flag(post_process_volume::flags::unbind) ||
				(!instance->is_flag(post_process_volume::flags::active) &&
				 !instance->is_flag(post_process_volume::flags::debug_draw)))
			{
				continue;
			}

			DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
			DirectX::XMFLOAT4 color = { 0.0f, 1.0f, 0.0f, 1.0f };
			PUSH_DEBUG_PRIMITIVE_BOX(instance->get_outer_range(), rotation, instance->get_position(), color, true);
			PUSH_DEBUG_PRIMITIVE_BOX(instance->get_inner_range(), rotation, instance->get_position(), color, true);
		}
	}
#endif	//	defined(USE_IMGUI)
}

