#pragma once

#include <queue>

#include "renderer_domain.h"
#include "renderer_context.h"

#if defined(USE_IMGUI)
#include "Graphics/gpu_timer.h"

//	GPUタイムスタンプ情報
struct gpu_time_stamp_data
{
	std::string name;
	float elapsed_time;
	gpu_time_stamp_data(std::string name, float elapsed_time) : name(name), elapsed_time(elapsed_time) {}
};
#endif	//	defined(USE_IMGUI)

using render_job_function = void(graphics_context&);

//	描画用ジョブ実行者
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class render_job_executor
{
	struct render_command_data
	{
		std::string name;
		UINT sort_order;
		Microsoft::WRL::ComPtr<ID3D11CommandList> command_list;
		std::function<void()> task;
	};

public:
	render_job_executor(UINT num_threads);
	~render_job_executor();

	void execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context = false);

	void begin();
	void render();
	void end();

	bool is_single_thread_mode() const { return single_thread_mode; }
	void set_single_thread_mode(bool flag) { single_thread_mode = flag; }

	//	全描画コマンド実行後に実行する関数
	void register_post_rendering(std::function<void()> function)
	{
		post_render_function = function;
	}

#if defined(USE_IMGUI)
	//	スナップショット撮影
	void begin_gpu_snap_shot() {gpu_snap_shot = true;}

	//	スナップショット:タイムスタンプリスト取得
	void end_gpu_snap_shot(std::vector<gpu_time_stamp_data>& time_stamp_data)
	{
		time_stamp_data = snap_shot_time_stamps;
	}

	//	常駐でのGPU使用状況を取得
	inline float get_stationed_time_stamp() const { return stationed_time_stamp; }
#endif	//	defined(USE_IMGUI)

private:
	void run(UINT context_index);
	void register_command(render_command_data& command_data);

	void begin_snap_shot();
	void end_snap_shot(std::string_view command_name);

private:
	std::mutex mutex;
	std::mutex command_mutex;
	std::condition_variable condition;
	std::vector<std::thread> worker_threads;
	std::vector<Microsoft::WRL::ComPtr<ID3D11DeviceContext>> deferred_contexts;

	std::queue<std::function<void(ID3D11DeviceContext*)>> tasks;
	std::atomic_int running_threads;
	bool running;
	bool single_thread_mode;
	std::function<void()> post_render_function;

	std::vector<render_command_data> render_command_list;

#if defined(USE_IMGUI)
	bool gpu_snap_shot = false;
	gpu_timer stationed_gpu_timer;
	float stationed_time_stamp;

	gpu_timer snap_shot_gpu_timer;
	std::vector<gpu_time_stamp_data> snap_shot_time_stamps;
#endif	//	defined(USE_IMGUI)
};

class render_job_registrar
{
public:
	render_job_registrar(std::shared_ptr<render_job_executor> executor);

	void execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context = false);

private:
	std::shared_ptr<render_job_executor> executor;
};
