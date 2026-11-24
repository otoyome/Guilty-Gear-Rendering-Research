#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"

#include "collision/box.h"


debug_primitive_manager::debug_primitive_manager()
{
	HRESULT hr;
	auto device = render_data::instance()->get_device();

	//	頂点バッファ
	for (auto& vertex_buffer_view : vertex_buffer_views)
	{
		buffer_view vertex_buffer;

		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(VertexCount * sizeof(vertex));
		buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
		buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = device->CreateBuffer(&buffer_desc, nullptr, vertex_buffer.buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		vertex_buffer.format = DXGI_FORMAT_UNKNOWN;
		vertex_buffer.stride_in_bytes = sizeof(vertex);
		vertex_buffer.size_in_bytes = 0;
		vertex_buffer_view.insert_or_assign(semantic("VERTEX", 0), vertex_buffer);
	}
}

debug_primitive_manager::~debug_primitive_manager()
{
}

void debug_primitive_manager::get_container(debug_draw_primitive_container& container)
{
	//	描画情報登録
	HRESULT hr{ S_OK };
	for (UINT index = 0; index < static_cast<int>(debug_draw_primitive_container::line_buffer_id::max); ++index)
	{
		auto& view = vertex_buffer_views[index].begin()->second;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
		hr = render_data::instance()->get_immediate_context()->Map(view.buffer.Get(),
			0,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&mapped_subresource);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (SUCCEEDED(hr))
		{
			vertex* map_vertices = reinterpret_cast<vertex*>(mapped_subresource.pData);
			memcpy_s(map_vertices,
				VertexCount * sizeof(vertex),
				vertices[index].data(),
				std::min(VertexCount, vertices[index].size()) * sizeof(vertex));

			//	アンマップ
			render_data::instance()->get_immediate_context()->Unmap(view.buffer.Get(), 0);
		}

		//	頂点数の設定
		view.size_in_bytes = view.stride_in_bytes * vertices[index].size();

		//	コンテナに情報登録
		auto& main_mesh = container.line_buffer_datas[index].color_meshs.emplace_back();
		main_mesh.vertex_buffer_views = &vertex_buffer_views[index];
		vertices[index].clear();
	}
}

void debug_primitive_manager::push_line(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color, bool depth_write)
{
	int index = depth_write ? 0 : 1;
	if (vertices[index].size() + 2 >= VertexCount)
		return;

	vertex vertex;
	vertex.position = start;
	vertex.color = color;
	vertices[index].emplace_back(vertex);
	vertex.position = end;
	vertex.color = color;
	vertices[index].emplace_back(vertex);
}

void debug_primitive_manager::push_sphere(const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT4& color, bool depth_write)
{
	//	球の形状で登録
	std::vector<DirectX::XMFLOAT3> vertices;
	vertices.emplace_back(center.x, center.y + radius, center.z);

	static constexpr uint32_t stacks = 8;
	static constexpr uint32_t slices = 8;
	float phi_step{ DirectX::XM_PI / stacks };
	float theta_step{ 2.0f * DirectX::XM_PI / slices };
	for (uint32_t i = 1; i <= stacks - 1; ++i)
	{
		float phi{ i * phi_step };
		for (uint32_t j = 0; j <= slices; ++j)
		{
			float theta{ j * theta_step };
			DirectX::XMFLOAT3 position;
			position.x = center.x + radius * sinf(phi) * cosf(theta);
			position.y = center.y + radius * cosf(phi);
			position.z = center.z + radius * sinf(phi) * sinf(theta);
			vertices.emplace_back(position);
		}
	}
	vertices.emplace_back(center.x, center.y - radius, center.z);

	for (uint32_t i = 1; i <= slices; ++i)
	{
		push_line(vertices[0], vertices[i], color, depth_write);
		push_line(vertices[i], vertices[i + 1], color, depth_write);
	}

	uint32_t base_index{ 1 };
	uint32_t ring_vertex_count{ slices + 1 };
	for (uint32_t i = 0; i < stacks - 2; ++i)
	{
		for (uint32_t j = 0; j < slices; ++j)
		{
			uint32_t index[6] =
			{
				{base_index + i * ring_vertex_count + j},
				{base_index + (i + 1) * ring_vertex_count + j},
				{base_index + i * ring_vertex_count + j + 1},

				{base_index + (i + 1) * ring_vertex_count + j},
				{base_index + (i + 1) * ring_vertex_count + j + 1},
				{base_index + i * ring_vertex_count + j + 1},
			};
			push_line(vertices[index[0]], vertices[index[1]], color, depth_write);
			push_line(vertices[index[2]], vertices[index[0]], color, depth_write);
			push_line(vertices[index[3]], vertices[index[4]], color, depth_write);
			push_line(vertices[index[4]], vertices[index[5]], color, depth_write);
		}
	}
	uint32_t south_pole_index{ static_cast<uint32_t>(vertices.size() - 1) };
	base_index = south_pole_index - ring_vertex_count;
	for (uint32_t i = 0; i < slices; ++i)
	{
		push_line(vertices[base_index + i + 1], vertices[base_index + i], color, depth_write);
		push_line(vertices[south_pole_index], vertices[base_index + i], color, depth_write);
	}
}

void debug_primitive_manager::push_capsule(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT4& rotation, float radius, float length, const DirectX::XMFLOAT4& color, bool depth_write)
{
	//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
	math::rotator rotator(rotation);
	DirectX::XMVECTOR Center = DirectX::XMLoadFloat3(&center);
	DirectX::XMFLOAT3 work = rotator.get_up();
	DirectX::XMVECTOR Up = DirectX::XMLoadFloat3(&work);
	work = rotator.get_right();
	DirectX::XMVECTOR Right = DirectX::XMLoadFloat3(&work);
	work = rotator.get_front();
	DirectX::XMVECTOR Front = DirectX::XMLoadFloat3(&work);

	DirectX::XMVECTOR Botom = DirectX::XMVectorAdd(Center, DirectX::XMVectorScale(Up, -length / 2));
	DirectX::XMVECTOR Top = DirectX::XMVectorAdd(Center, DirectX::XMVectorScale(Up, +length / 2));

	DirectX::XMFLOAT3 position, position2;
	DirectX::XMStoreFloat3(&position, Botom);
	push_sphere(position, radius, color, depth_write);
	DirectX::XMStoreFloat3(&position, Top);
	push_sphere(position, radius, color, depth_write);

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Right, radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Right, radius)));
	push_line(position, position2, color, depth_write);

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Right, -radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Right, -radius)));
	push_line(position, position2, color, depth_write);

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Front, radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Front, radius)));
	push_line(position, position2, color, depth_write);

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Front, -radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Front, -radius)));
	push_line(position, position2, color, depth_write);

	DirectX::XMVECTOR Direction1 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(Right, Front));
	DirectX::XMVECTOR Direction2 = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(Right, Front));

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Direction1, radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Direction1, radius)));
	push_line(position, position2, color, depth_write);

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Direction1, -radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Direction1, -radius)));
	push_line(position, position2, color, depth_write);

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Direction2, radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Direction2, radius)));
	push_line(position, position2, color, depth_write);

	DirectX::XMStoreFloat3(&position, DirectX::XMVectorAdd(Botom, DirectX::XMVectorScale(Direction2, -radius)));
	DirectX::XMStoreFloat3(&position2, DirectX::XMVectorAdd(Top, DirectX::XMVectorScale(Direction2, -radius)));
	push_line(position, position2, color, depth_write);
}

void debug_primitive_manager::push_box(const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color, bool depth_write)
{
	collision::bounding_box box(scale, rotation, position);
	DirectX::XMFLOAT3 vertices[8];
	box.calculate_vertex(vertices, 8);
	push_line(vertices[0], vertices[1], color, depth_write);
	push_line(vertices[1], vertices[2], color, depth_write);
	push_line(vertices[2], vertices[3], color, depth_write);
	push_line(vertices[3], vertices[0], color, depth_write);

	push_line(vertices[4], vertices[5], color, depth_write);
	push_line(vertices[5], vertices[6], color, depth_write);
	push_line(vertices[6], vertices[7], color, depth_write);
	push_line(vertices[7], vertices[4], color, depth_write);

	push_line(vertices[0], vertices[4], color, depth_write);
	push_line(vertices[1], vertices[5], color, depth_write);
	push_line(vertices[2], vertices[6], color, depth_write);
	push_line(vertices[3], vertices[7], color, depth_write);
}

#endif	//	defined(USE_IMGUI)
