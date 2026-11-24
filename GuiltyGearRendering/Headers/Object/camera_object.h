#pragma once

#include "camera.h"
#include "object/game_object.h"

//	カメラ制御用オブジェクト
class camera_object : public game_object
{
	GAME_OBJECT_DECRLEAR(camera_object, game_object)

public:
	camera_ptr get_camera() const {return camera;}

	const math::rotator& get_rotation() const;
	void set_rotation(const math::rotator& rotator);

	const DirectX::XMFLOAT3& get_position() const;
	void set_position(const DirectX::XMFLOAT3& position);

	const DirectX::XMFLOAT3& get_front() const;
	const DirectX::XMFLOAT3& get_right() const;
	const DirectX::XMFLOAT3& get_up() const;

public:
	camera_object(std::string_view name);
	~camera_object() override;

	void post_update(float elapsed_time) override;
	void post_render() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	camera_ptr camera;

	math::rotator rotation;
	DirectX::XMFLOAT3 position;
};
