#include "object/camera_object.h"

const math::rotator& camera_object::get_rotation() const
{
	return rotation;
}

void camera_object::set_rotation(const math::rotator& rotator)
{
	this->rotation = rotator;
}

const DirectX::XMFLOAT3& camera_object::get_position() const
{
	return position;
}

void camera_object::set_position(const DirectX::XMFLOAT3& position)
{
	this->position = position;
}

const DirectX::XMFLOAT3& camera_object::get_front() const
{
	return camera->get_front();
}

const DirectX::XMFLOAT3& camera_object::get_right() const
{
	return camera->get_right();
}

const DirectX::XMFLOAT3& camera_object::get_up() const
{
	return camera->get_up();
}

camera_object::camera_object(std::string_view name)
: game_object(name)
, rotation(0, DirectX::XM_PI, 0)
, position(0, 0, 0)
{
	camera_manager::instance()->generate_camera(camera);
	camera->set_rotation(rotation);
	camera->set_position(position);
	camera->set_screen_rect({0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT});
}

camera_object::~camera_object()
{
}

void camera_object::post_update(float elapsed_time)
{
	math::rotator new_rotation = rotation;
	DirectX::XMFLOAT3 new_position = position;
	camera->set_rotation(new_rotation);
	camera->set_position(new_position);
}

void camera_object::post_render()
{
	camera->cache_previous_view_projection_matrix();
}

#if defined(USE_IMGUI)
void camera_object::imgui_render_core()
{
	auto rotation = camera->get_rotation();
	ImGui::DragFloat3("rotation", &rotation.x, 0.01f);
	camera->set_rotation(rotation);

	auto position = camera->get_position();
	ImGui::DragFloat3("position", &position.x, 0.01f);
	camera->set_position(position);

	auto is_hybrid_projection = camera->is_flag(camera::flags::hybrid_projection);
	if (ImGui::Checkbox("hybrid_projection", &is_hybrid_projection))
	{
		camera->set_flag(camera::flags::hybrid_projection, is_hybrid_projection);
	}

	if (is_hybrid_projection)
	{
		auto hybrid_rate = camera->get_hybrid_rate();
		ImGui::SliderFloat("hybrid_rate", &hybrid_rate, 0, 1);
		camera->set_hybrid_rate(hybrid_rate);
	}

	auto fov_y = DirectX::XMConvertToDegrees(camera->get_fov_y());
	ImGui::DragFloat("fov_y", &fov_y, 0.01f, 0.01f);
	camera->set_fov_y(DirectX::XMConvertToRadians(fov_y));

	auto clip_distance = camera->get_clip_distance();
	ImGui::DragFloat2("clip_distance", &clip_distance.x, 0.01f);
	camera->set_clip_distance(clip_distance);
}
#endif	//	defined(USE_IMGUI)
