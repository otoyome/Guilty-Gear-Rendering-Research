#pragma once

#include "texture.h"

class material;

//	ブルーム用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct bloom_data
{
	enum class modes
	{
		standard,
		convolution,
	};

private:
	bool use = true;

public:
	float threshold = 1.2f;
	float intensity = 0.1f;
	modes mode = modes::convolution;

	//	gaussian
	float deviation = 50.0f;

	//	convolution
	texture_ptr image_kernel;

public:
	bloom_data();

	bool is_active() const;
	void set_active(bool flag);
};

//	レンズゴースト用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct lens_ghosts_data
{
private:
	bool use = true;

public:
	float threshold = 1.2f;
	float intensity = 0.1f;
	float dispersion_scale = 0.8f;
	float dispersion_offset = 0;

	float ghost_mask_radius = 320;
	float ghost_mask_fade = 0.5f;
	int iteration = 4;
	float ghost_bokeh = 1.5f;

	texture_ptr image_kernel;
	texture_ptr ghost_color_lut;

public:
	lens_ghosts_data();

	bool is_active() const;
	void set_active(bool flag);
};

//	被写界深度用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct depth_of_field_data
{
private:
	bool use = true;

public:
	float focal_length = 30.0f;
	float focus_distance = 1.0f;
	float circle_of_confusion = 0.03f;
	float apeture = 1.5f;

	bool display_dof_area = false;
	int blur_size = 57;
	DirectX::XMFLOAT2 adjust_length = { 0.0f, 0.0f };

	int precision_scale_factor = 26; //	精度調整係数
									 //	被写界深度で色がおかしくなっている時はこれを調整してみてください。

public:
	float calculate_f_number() const;
	bool is_active() const;
	void set_active(bool flag);
};

//	スクリーン空間反射用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct screen_space_reflection_data
{
private:
	bool use = true;

public:
	float ray_max_distance = 1000.0f;
	float ray_resolution = 0.5f;
	DirectX::XMFLOAT2 ray_tickness = {0.5f, 100.0f};

	int ray_substep_iteration_max = 15;
	int ray_flags = 0;

	bool accumalate_reset = true;
	float velocity_reset_threshold = 1.0f;

public:
	screen_space_reflection_data();
	bool is_flag(UINT ssr_flag) const;
	void set_flag(UINT ssr_flag, bool value);
	bool is_active() const;
	void set_active(bool flag);
};

//	モーションブラー用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct motion_blur_data
{
private:
	bool use = true;

public:
	int iteration = 10;
	float jitter_noise_value = 0.5f;
	float velocity_multiplyer = 10.0f;

public:
	bool is_active() const;
	void set_active(bool flag);
};

//	露出補正用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct exposure_compensation_data
{
private:
	bool use = true;

public:
	float adapt_rate = 0.05f;
	DirectX::XMFLOAT2 luminance_range = {2.0f, 1000.0f};

public:
	bool is_active() const;
	void set_active(bool flag);
};

//	トーンマッピング用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct tone_mapping_data
{
	enum class tone_mapping_type
	{
		none,
		reinhard,
		aces_krzysztof_narkowicz,
		aces_stephen_hill,
		gran_turismo,
	};

private:
	tone_mapping_type tone_map_type = tone_mapping_type::none;

public:
	float tone_mapping_variables[7];

public:
	tone_mapping_data();
	tone_mapping_type get_tone_map_type() const;
	void set_tone_map_type(tone_mapping_type type);
};

//	ポストプロセスボリューム情報
class post_process_volume
{
public:
	enum class flags
	{
		active,		//	アクティブ状態かどうか
		unbind,		//	範囲制限をかけるかどうか(かけない場合は全画面)
		debug_draw,	//	デバッグ描画
	};

public:
	post_process_volume();
	~post_process_volume();

	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value){ system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	DirectX::XMFLOAT3 get_position() const;
	void set_position(const DirectX::XMFLOAT3& position);
	DirectX::XMFLOAT3 get_inner_range() const;
	void set_inner_range(const DirectX::XMFLOAT3& inner_range);
	DirectX::XMFLOAT3 get_outer_range() const;
	void set_outer_range(const DirectX::XMFLOAT3& outer_range);
	const DirectX::XMFLOAT4& get_color() const;
	void set_color(const DirectX::XMFLOAT4& color);

	const bloom_data& get_bloom_constant() const;
	void set_bloom_constant(const bloom_data& data);

	const lens_ghosts_data& get_lens_ghosts_constant() const;
	void set_lens_ghosts_constant(const lens_ghosts_data& data);

	const depth_of_field_data& get_depth_of_field_constant() const;
	void set_depth_of_field_constant(const depth_of_field_data& data);

	const screen_space_reflection_data& get_screen_space_reflection_constant() const;
	void set_screen_space_reflection_constant(const screen_space_reflection_data& data);

	const motion_blur_data& get_motion_blur_constant() const;
	void set_motion_blur_constant(const motion_blur_data& data);

	const exposure_compensation_data& get_exposure_compensation_constant() const;
	void set_exposure_compensation_constant(const exposure_compensation_data& data);

	const tone_mapping_data& get_tone_mapping_constant() const;
	void set_tone_mapping_constant(const tone_mapping_data& data);

	const std::vector<std::shared_ptr<material>>& get_materials() const;
	bool register_postprocess_materials(std::shared_ptr<material> material_ptr);

private:
	UINT system_flags;
	bloom_data bloom_constant;
	lens_ghosts_data lens_ghosts_constant;
	depth_of_field_data depth_of_field_constant;
	screen_space_reflection_data screen_space_reflection_constant;
	motion_blur_data motion_blur_constant;
	exposure_compensation_data exposure_compensation_constant;
	tone_mapping_data tone_mapping_constant;
	std::vector<std::shared_ptr<material>> postprocess_materials;

	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 inner_range;
	DirectX::XMFLOAT3 outer_range;
	DirectX::XMFLOAT4 color;
};
ENUM_ATTR_BITFLAG(post_process_volume::flags);

using post_process_volume_ptr = std::shared_ptr<post_process_volume>;

//	ポストプロセスボリュームコンテナ
struct post_process_volume_container
{
	std::vector<post_process_volume_ptr> volumes;
};

//	ポストプロセスボリューム管理クラス
class post_process_volume_manager
{
	SINGLETON(post_process_volume_manager);

private:
	post_process_volume_manager();
	~post_process_volume_manager();

public:
	bool generate_post_process_volume(post_process_volume_ptr& out_post_process_volume);
	void get_container(post_process_volume_container& container);

private:
	std::vector<std::weak_ptr<post_process_volume>> post_process_volumes;
};
