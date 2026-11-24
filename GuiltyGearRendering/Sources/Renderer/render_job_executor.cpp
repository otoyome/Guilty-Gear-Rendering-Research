#include "Renderer/render_job_executor.h"
#include <thread>

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#endif // defined(USE_IMGUI)

render_job_executor::render_job_executor(UINT num_threads)
: running(true)
, single_thread_mode(false)
#if defined(USE_IMGUI)
, stationed_gpu_timer(render_data::instance()->get_immediate_context())
, snap_shot_gpu_timer(render_data::instance()->get_immediate_context())
, stationed_time_stamp(0)
#endif	//	defined(USE_IMGUI)
{
	HRESULT hr;
	assert(num_threads != -1);

	//	スレッド生成
	worker_threads.reserve(num_threads);
	deferred_contexts.reserve(num_threads);
	for (UINT index = 0; index < num_threads; ++index)
	{
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		hr = render_data::instance()->get_device()->CreateDeferredContext(0, context.GetAddressOf());
		deferred_contexts.emplace_back(context);
		worker_threads.emplace_back(std::thread(&render_job_executor::run, this, index));
	}
}

render_job_executor::~render_job_executor()
{
	//	スレッドを終了させる
	{
		std::lock_guard<std::mutex>	cLockGuard(mutex);
		if (!running)
			return;

		running = false;
	}

	//	積み込みスレッド破棄
	condition.notify_all();
	for (auto& it : worker_threads)
	{
		it.join();
	}
}

void render_job_executor::execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context)
{
	if (single_thread_mode)
	{
		graphics_context g_context;
		g_context.device_context = render_data::instance()->get_immediate_context().Get();
		g_context.render_data = context.render_data;
		g_context.scene_constant = context.scene_constant;
		g_context.scene_constant_buffer = context.scene_constant_buffer;

		begin_snap_shot();
		task(g_context);
		end_snap_shot(name);
		return;
	}

	std::string name_text = std::string(name);
	if (use_immediate_context)
	{
		render_command_data command_data;
		command_data.name = name_text;
		command_data.sort_order = sort_order;
		command_data.task = std::function<void()>(
			[task, name_text, context]()
			{
				//	描画コマンド積み込み
				graphics_context g_context;
				g_context.device_context = render_data::instance()->get_immediate_context().Get();
				g_context.render_data = context.render_data;
				g_context.scene_constant = context.scene_constant;
				g_context.scene_constant_buffer = context.scene_constant_buffer;
				g_context.debug_begin_event(system_utility::convert_shift_jis_to_utf16(name_text).data());
				task(g_context);
				g_context.debug_end_event();
			} );
		register_command(command_data);
	}
	else
	{
		std::lock_guard<std::mutex>	cLockGuard(mutex);
		auto task_function = std::make_shared<std::function<void(ID3D11DeviceContext*)>>(
			[task, name_text, context, sort_order, this](ID3D11DeviceContext* deferred_context)
			{
				//	描画コマンド積み込み
				graphics_context g_context;
				g_context.device_context = deferred_context;
				g_context.render_data = context.render_data;
				g_context.scene_constant = context.scene_constant;
				g_context.scene_constant_buffer = context.scene_constant_buffer;
				g_context.debug_begin_event(system_utility::convert_shift_jis_to_utf16(name_text).data());
				task(g_context);
				g_context.debug_end_event();

				//	コマンドリスト生成
				render_command_data command_data;
				command_data.name = name_text;
				command_data.sort_order = sort_order;
				deferred_context->FinishCommandList(false, command_data.command_list.GetAddressOf());
				this->register_command(command_data);
			} );

		tasks.push(std::function<void(ID3D11DeviceContext*)>(
			[task_function](ID3D11DeviceContext* context)
			{
				(*task_function)(context);
			}));
	}

	//	待機スレッドを起床
	condition.notify_one();
}

void render_job_executor::begin()
{
#if defined(USE_IMGUI)
	//	常駐タイマー開始
	{
		float time = stationed_gpu_timer.get_result(true);
		if (stationed_gpu_timer.is_finalized())
		{
			stationed_time_stamp = time;
			stationed_gpu_timer.begin();
		}
	}

	//	クリア
	if (gpu_snap_shot)
	{
		snap_shot_time_stamps.clear();
	}
#endif	//	defined(USE_IMGUI)
}

void render_job_executor::end()
{
#if defined(USE_IMGUI)
	//	常駐タイマー終了
	{
		stationed_gpu_timer.end();
	}

	gpu_snap_shot = false;
#endif	//	defined(USE_IMGUI)

	render_command_list.clear();
	post_render_function();
}

void render_job_executor::render()
{
	//	全描画タスクが終了するまで待機
	while (true)
	{
		std::lock_guard<std::mutex>	cUniqueLock(mutex);
		if (tasks.empty())
		{
			if (running_threads <= 0)
			{
				break;
			}
		}
	}

	//	登録されている描画コマンドをソート
	std::sort(render_command_list.begin(), render_command_list.end(),
		[](const render_command_data& l, const render_command_data& r)
		{
			return l.sort_order < r.sort_order;
		}
	);

	//	積み込まれたコマンドを実行
	ID3D11DeviceContext* immediate_context = render_data::instance()->get_immediate_context().Get();
	for (auto& command : render_command_list)
	{
		begin_snap_shot();

		if (command.command_list)
		{
			immediate_context->ExecuteCommandList(command.command_list.Get(), false);
		}
		else
		{
			command.task();
		}

		end_snap_shot(command.name);
	}
}

void render_job_executor::register_command(render_command_data& command_data)
{
	std::unique_lock<std::mutex> unique_lock(command_mutex);

	render_command_list.emplace_back(command_data);
}

void render_job_executor::run(UINT context_index)
{
	auto deferred_context = deferred_contexts[context_index];
	while (true)
	{
		std::function<void(ID3D11DeviceContext*)> task_function;

		{
			std::unique_lock<std::mutex> unique_lock(mutex);
			condition.wait(unique_lock, [&]{return !tasks.empty() || !running;});
			++running_threads;
			if (!running && tasks.empty())
			{
				--running_threads;
				break;
			}
			if (tasks.empty())
			{
				--running_threads;
				continue;;
			}

			task_function = std::move(tasks.front());
			tasks.pop();
		}

		task_function(deferred_context.Get());

		--running_threads;
	}
}

void render_job_executor::begin_snap_shot()
{
#if defined(USE_IMGUI)
	//	記録開始
	if (gpu_snap_shot)
		snap_shot_gpu_timer.begin();
#endif	//	defined(USE_IMGUI)
}

void render_job_executor::end_snap_shot(std::string_view command_name)
{
#if defined(USE_IMGUI)
	//	記録終了
	if (gpu_snap_shot)
	{
		snap_shot_gpu_timer.end();
		float result_time = snap_shot_gpu_timer.get_result();
		snap_shot_time_stamps.emplace_back(std::string(command_name), result_time);
	}
#endif	//	defined(USE_IMGUI)
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
render_job_registrar::render_job_registrar(std::shared_ptr<render_job_executor> executor)
: executor(executor)
{
}

void render_job_registrar::execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context)
{
	executor->execute(task, name, context, sort_order, use_immediate_context);
}

