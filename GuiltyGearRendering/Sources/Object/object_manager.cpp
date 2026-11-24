#include "Object/object_manager.h"

namespace
{
	static constexpr float fixed_per_second = system_units::convert_fps_to_sec(1.0f);
}

object_manager::object_manager()
: game_objects()
, game_object_add()
, game_object_removes()
, messenger()

, total_time(0)
, remain_time(0)
{
}

object_manager::~object_manager()
{
}

void object_manager::updates(float elapsed_time)
{
	//	追加
	add_objects();

#if defined(USE_IMGUI)
	if (pause)
	{
		if (step)
		{
			elapsed_time = fixed_per_second + 0.00001f;
			step = false;
		}
		else
		{
			return;
		}
	}
#endif // defined(USE_IMGUI)

	//	更新処理
	{
#if defined(USE_IMGUI)
		//	デバッグ更新
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;
			object->debug_start_of_frame();
		}
#endif	//	defined(USE_IMGUI)

		total_time += elapsed_time;
		float temporary_elapsed_time = elapsed_time + remain_time;
		int loop_count = static_cast<int>(temporary_elapsed_time / fixed_per_second);
#if defined(USE_IMGUI)
		fixed_update_count = loop_count;
#endif // defined(USE_IMGUI)
		int limit_loop_count = std::min(loop_count, static_cast<int>(1.0f / fixed_per_second + 0.0001f));
		temporary_elapsed_time -= (loop_count - limit_loop_count) * fixed_per_second;
		for (int count = 0; count < limit_loop_count; ++count)
		{
			add_objects();

			//	固定更新処理
			for (auto& object : game_objects)
			{
				if (!object->is_active())
					continue;
				object->fixed_update(fixed_per_second);
			}

			remove_objects();
			add_objects();

			temporary_elapsed_time -= fixed_per_second;
		}
		remain_time = temporary_elapsed_time;

		//	更新処理
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;
			object->update(elapsed_time);
		}

		remove_objects();
		add_objects();

		//	更新後処理
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;
			object->post_update(elapsed_time);
		}

		remove_objects();
		add_objects();

#if defined(USE_IMGUI)
		//	デバッグ更新
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;
			object->debug_pre_update(elapsed_time);
		}
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;
			object->debug_update(elapsed_time);
		}
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;
			object->debug_post_update(elapsed_time);
		}
#endif	//	defined(USE_IMGUI)
	}
}

void object_manager::pre_render()
{
	remove_objects();
	add_objects();

	//	描画前処理
	for (auto& object : game_objects)
	{
		if (!object->is_active())
			continue;

		object->pre_render();
	}

	remove_objects();
	add_objects();
}

void object_manager::post_render()
{
	remove_objects();
	add_objects();

	//	描画前処理
	for (auto& object : game_objects)
	{
		if (!object->is_active())
			continue;

		object->post_render();
	}

	remove_objects();
	add_objects();
}

void object_manager::finalize()
{
	for (auto& object : game_objects)
	{
		object->on_finalize();
	}
	system_utility::std_vector_swap_clear(game_objects);
	for (auto& object : game_object_add)
	{
		object->on_finalize();
	}
	system_utility::std_vector_swap_clear(game_object_add);
	for (auto& object : game_object_removes)
	{
		object->on_finalize();
	}
	system_utility::std_vector_swap_clear(game_object_removes);
}

#if defined(USE_IMGUI)
void object_manager::imgui_render()
{
	if (ImGui::Begin("objects"))
	{
		for (auto& object : game_objects)
		{
			object->imgui_render();
		}
	}
	ImGui::End();
}

bool object_manager::get_fixed_update_count() const
{
	return fixed_update_count;
}

bool object_manager::get_pause() const
{
	return pause;
}

void object_manager::set_pause(bool pause)
{
	this->pause = pause;
}

void object_manager::called_step()
{
	step = true;
}

#endif // defined(USE_IMGUI)

bool object_manager::register_object(game_object_ptr object)
{
	if (!object)
		return false;
	if (std::find(game_object_add.begin(), game_object_add.end(), object) != game_object_add.end())
		return false;
	if (std::find(game_objects.begin(), game_objects.end(), object) != game_objects.end())
		return false;

	auto it = std::find(game_object_removes.begin(), game_object_removes.end(), object);
	if (it != game_object_removes.end())
	{
		game_object_removes.erase(it);
	}
	game_object_add.emplace_back(object);
	return true;
}

bool object_manager::unregister_object(game_object_ptr object)
{
	if (!object)
		return false;
	if (std::find(game_object_removes.begin(), game_object_removes.end(), object) != game_object_removes.end())
		return false;
	if (std::find(game_objects.begin(), game_objects.end(), object) == game_objects.end())
		return false;

	auto it = std::find(game_object_add.begin(), game_object_add.end(), object);
	if (it != game_object_add.end())
	{
		game_object_add.erase(it);
	}
	game_object_removes.emplace_back(object);
	return true;
}

void object_manager::add_objects()
{
	for (auto& object : game_object_add)
	{
		game_objects.emplace_back(object->shared_from_this());
		game_objects.back()->on_initialize();
	}
	system_utility::std_vector_swap_clear(game_object_add);
}

void object_manager::remove_objects()
{
	for (auto& object : game_object_removes)
	{
		auto it = std::find(game_objects.begin(), game_objects.end(), object);
		if (it != game_objects.end())
		{
			(*it)->on_finalize();
			game_objects.erase(it);
		}
	}
	system_utility::std_vector_swap_clear(game_object_removes);
}
