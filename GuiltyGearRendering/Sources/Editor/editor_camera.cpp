#include "Editor/editor_camera.h"

#if	defined(USE_IMGUI)

editor_camera::editor_camera(std::shared_ptr<camera_object> camera)
: game_object("editor_camera")
, camera(camera)

, execute(false)

, rotation(0, 0, 0)
, position(0, 0, 0)
{
}

editor_camera::~editor_camera()
{
}

void editor_camera::fixed_update(float elapsed_time)
{
	camera->set_rotation(rotation);
	camera->set_position(position);
}

#if defined(USE_IMGUI)
void editor_camera::debug_update(float elapsed_time)
{
	auto mouse_state = mouse::instance()->get_state();
	auto keyboard_state = keyboard::instance()->get_state();

	//	キーボード操作
	if (mouse_state.is_key_state(mouse::key_states::right) == input::state::hold)
	{
		int x = 0, y = 0, z = 0;
		if (keyboard_state.is_key_state(keyboard::key_states::w) == input::state::hold)
		{
			++z;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::s) == input::state::hold)
		{
			--z;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::d) == input::state::hold)
		{
			++x;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::a) == input::state::hold)
		{
			--x;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::e) == input::state::hold)
		{
			++y;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::q) == input::state::hold)
		{
			--y;
		}

		int wheel = mouse::instance()->get_wheel_value();
		if (wheel != 0)
		{
			move_speed += move_speed * 0.001f * wheel;
		}

		float speed = move_speed * system_units::convert_sec_to_fps(elapsed_time);
		DirectX::XMFLOAT3 front = rotation.get_front();
		DirectX::XMFLOAT3 right = rotation.get_right();
		DirectX::XMFLOAT3 up = rotation.get_up();
		position.x += +front.x * speed * z + right.x * speed * x - up.x * speed * y;
		position.y += +front.y * speed * z + right.y * speed * x - up.y * speed * y;
		position.z += +front.z * speed * z + right.z * speed * x - up.z * speed * y;
	}

	// マウス操作
#if defined(USE_IMGUI)
	if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
#endif // defined(USE_IMGUI)
	{
		DirectX::XMVECTOR Up{ DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };

		DirectX::XMFLOAT2 move;
		move.x = static_cast<float>(mouse::instance()->get_move_cursol().x);
		move.y = static_cast<float>(mouse::instance()->get_move_cursol().y);
		int wheel = mouse::instance()->get_wheel_value();
		move.x *= math::step(1.0f, fabsf(move.x));
		move.y *= math::step(1.0f, fabsf(move.y));
		if (mouse_state.is_key_state(mouse::key_states::right) == input::state::hold)
		{
			float speed = rotation_speed * system_units::convert_sec_to_fps(elapsed_time);
			rotation.x += move.y * speed;
			rotation.y += move.x * (rotation.get_up().y < 0 ? -speed : speed);
		}

		if (mouse_state.is_key_state(mouse::key_states::middle) == input::state::hold ||
			(mouse_state.is_key_state(mouse::key_states::right) != input::state::hold && wheel != 0))
		{
			float speed = system_units::convert_m_to_unit(0.02f) * system_units::convert_sec_to_fps(elapsed_time);
			DirectX::XMFLOAT3 front = rotation.get_front();
			DirectX::XMFLOAT3 right = rotation.get_right();
			DirectX::XMFLOAT3 up = rotation.get_up();
			position.x += +front.x * speed * wheel - right.x * speed * move.x + up.x * speed * move.y;
			position.y += +front.y * speed * wheel - right.y * speed * move.x + up.y * speed * move.y;
			position.z += +front.z * speed * wheel - right.z * speed * move.x + up.z * speed * move.y;
		}

		if (mouse_state.is_key_state(mouse::key_states::left) != input::state::none)
		{
			//	座標情報を送る
			mouse_position_data data;
			data.position.x = mouse_state.position.position.x;
			data.position.y = mouse_state.position.position.y;
			if (mouse_state.is_key_state(mouse::key_states::left) == input::state::down)
				object_manager::instance()->get_messenger().send_data(left_click_down_identifier, &data);
			else if (mouse_state.is_key_state(mouse::key_states::left) == input::state::hold)
				object_manager::instance()->get_messenger().send_data(left_click_hold_identifier, &data);
			else if (mouse_state.is_key_state(mouse::key_states::left) == input::state::release)
				object_manager::instance()->get_messenger().send_data(left_click_release_identifier, &data);
		}
	}

	if (is_auto_rotate)
	{
		rotation.y += DirectX::XMConvertToRadians(1.0f * elapsed_time);
	}
}
#endif	//	defined(USE_IMGUI)

void editor_camera::on_execute()
{
	rotation = camera->get_rotation();
	position = camera->get_position();
}

#endif	//	defined(USE_IMGUI)
