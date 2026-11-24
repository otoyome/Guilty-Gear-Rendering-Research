#pragma once

#if defined(USE_IMGUI)
#include "graphics_context.h"
#include "render_data.h"

//	デバッグ図形描画管理クラス
//	ﾃｷﾄｰ('ω')-ﾄｷﾃ
class debug_primitive_manager
{
	SINGLETON(debug_primitive_manager);

	static constexpr size_t VertexCount = 0x000FFFFF;

	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

private:
	debug_primitive_manager();
	~debug_primitive_manager();

public:
	void get_container(debug_draw_primitive_container& container);

	void push_line(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color, bool depth_write = true);
	void push_sphere(const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT4& color, bool depth_write = true);
	void push_capsule(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT4& rotation, float radius, float length, const DirectX::XMFLOAT4& color, bool depth_write = true);
	void push_box(const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color, bool depth_write = true);

private:
	std::vector<vertex> vertices[static_cast<int>(debug_draw_primitive_container::line_buffer_id::max)];
	std::unordered_map<semantic, buffer_view> vertex_buffer_views[static_cast<int>(debug_draw_primitive_container::line_buffer_id::max)];
};

#define PUSH_DEBUG_PRIMITIVE_LINE(START, END, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->push_line(START, END, COLOR, DEPTH_TEST)
#define PUSH_DEBUG_PRIMITIVE_SPHERE(CENTER, RADIUS, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->push_sphere(CENTER, RADIUS, COLOR, DEPTH_TEST)
#define PUSH_DEBUG_PRIMITIVE_CAPSULE(CENTER, ROTATION, RADIUS, LENGTH, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->push_capsule(CENTER, ROTATION, RADIUS, LENGTH, COLOR, DEPTH_TEST)
#define PUSH_DEBUG_PRIMITIVE_BOX(SCALE, ROTATION, POSITION, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->push_box(SCALE, ROTATION, POSITION, COLOR, DEPTH_TEST)

#else // defined(USE_IMGUI)

#define PUSH_DEBUG_PRIMITIVE_LINE(START, END, COLOR, DEPTH_TEST)
#define PUSH_DEBUG_PRIMITIVE_SPHERE(CENTER, RADIUS, COLOR, DEPTH_TEST)
#define PUSH_DEBUG_PRIMITIVE_CAPSULE(CENTER, RADIUS, LENGTH, COLOR, DEPTH_TEST)
#define PUSH_DEBUG_PRIMITIVE_BOX(SCALE, ROTATION, POSITION, COLOR, DEPTH_TEST)

#endif // defined(USE_IMGUI)
