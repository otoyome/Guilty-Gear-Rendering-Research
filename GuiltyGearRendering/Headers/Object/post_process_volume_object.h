#pragma once

#include "object/game_object.h"
#include "post_process_volume.h"

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"
#endif	//	defined(USE_IMGUI)

//	ポストプロセスボリューム制御用オブジェクト
//	真面目には対応していない
class post_process_volume_object : public game_object
{
	GAME_OBJECT_DECRLEAR(post_process_volume_object, game_object)

public:
	post_process_volume_ptr get_volume() const {return instance;}

	bool is_flag(post_process_volume::flags flag) const;
	void set_flag(post_process_volume::flags flag, bool value);

	DirectX::XMFLOAT3 get_position() const;
	void set_position(const DirectX::XMFLOAT3& position);
	DirectX::XMFLOAT3 get_inner_range() const;
	void set_inner_range(const DirectX::XMFLOAT3& inner_range);
	DirectX::XMFLOAT3 get_outer_range() const;
	void set_outer_range(const DirectX::XMFLOAT3& outer_range);
	const DirectX::XMFLOAT4& get_color() const;
	void set_color(const DirectX::XMFLOAT4& color);

public:
	post_process_volume_object(std::string_view name);
	~post_process_volume_object() override;

	void post_update(float elapsed_time) override;
	void post_render() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;

	void imgui_bloom();
	void imgui_lens_ghost();
	void imgui_depth_of_field();
	void imgui_screen_space_reflection();
	void imgui_motion_blur();
	void imgui_exposure_compensation();
	void imgui_tone_mapping();
#endif	//	defined(USE_IMGUI)

private:
	void on_active() override;
	void on_deactive() override;

private:
	post_process_volume_ptr instance;
#if defined(USE_IMGUI)
	std::shared_ptr<editor_items::context> editor_context;
#endif	//	defined(USE_IMGUI)
};
