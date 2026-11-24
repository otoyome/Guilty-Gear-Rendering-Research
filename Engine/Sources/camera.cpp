#include "camera.h"

camera::camera()
: system_flags(0)
, position(0.0f, 0.0f, 0.0f)
, rotation(0.0f, 0.0f, 0.0f)

, right(1.0f, 0.0f, 0.0f)
, up(0.0f, 1.0f, 0.0f)
, front(0.0f, 0.0f, 1.0f)

, view_matrix()
, projection_matrix()

, fov_y(DirectX::XMConvertToRadians(45))
, screen_rect(0.0f, 0.0f, 1920.0f, 1080.0f)
, clip_distance(0.1f, 10000.0f)
, hybrid_rate(0.3f)
{
	set_flag(flags::active, true);
	set_flag(flags::dirty, true);
	set_flag(flags::update_previous_matrix, true);
}

camera::~camera()
{
}

bool camera::is_flag(flags flag) const
{
	return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag));
}

void camera::set_flag(flags flag, bool value)
{
	if (flag == flags::hybrid_projection && value != is_flag(flags::hybrid_projection))
	{
		set_flag(flags::dirty, true);
	}

	system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value);
}

const DirectX::XMFLOAT3& camera::get_position() const
{
	return position;
}

void camera::set_position(const DirectX::XMFLOAT3& position)
{
	set_flag(flags::dirty, true);
	this->position = position;
}

const math::rotator& camera::get_rotation() const
{
	return rotation;
}

void camera::set_rotation(const math::rotator& rotation)
{
	set_flag(flags::dirty, true);
	this->rotation = rotation;
}

float camera::get_fov_y() const
{
	return fov_y;
}

void camera::set_fov_y(float fov_y)
{
	set_flag(flags::dirty, true);
	this->fov_y = fov_y;
}

float camera::get_hybrid_rate() const
{
	return hybrid_rate;
}

void camera::set_hybrid_rate(float hybrid_rate)
{
	set_flag(flags::dirty, true);
	this->hybrid_rate = hybrid_rate;
}

const DirectX::XMFLOAT4& camera::get_screen_rect() const
{
	return screen_rect;
}

void camera::set_screen_rect(DirectX::XMFLOAT4 screen_rect)
{
	set_flag(flags::dirty, true);
	this->screen_rect = screen_rect;
}

const DirectX::XMFLOAT2& camera::get_clip_distance() const
{
	return clip_distance;
}

void camera::set_clip_distance(DirectX::XMFLOAT2 clip_distance)
{
	set_flag(flags::dirty, true);
	this->clip_distance = clip_distance;
}

const DirectX::XMFLOAT3& camera::get_right()
{
	update_parameters();
	return right;
}

const DirectX::XMFLOAT3& camera::get_up()
{
	update_parameters();
	return up;
}

const DirectX::XMFLOAT3& camera::get_front()
{
	update_parameters();
	return front;
}

const DirectX::XMFLOAT4X4& camera::get_view_matrix()
{
	update_parameters();
	return view_matrix;
}

const DirectX::XMFLOAT4X4& camera::get_projection_matrix()
{
	update_parameters();
	return projection_matrix;
}

const DirectX::XMFLOAT4X4& camera::get_view_projection_matrix()
{
	update_parameters();
	return view_projection_matrix;
}

const DirectX::XMFLOAT4X4& camera::get_previous_view_projection_matrix()
{
	update_parameters();
	return previous_view_projection_matrix;
}

void camera::cache_previous_view_projection_matrix()
{
	set_flag(flags::update_previous_matrix, false);
	previous_view_projection_matrix = view_projection_matrix;
}

const collision::frustum_culling& camera::get_frustum_culling()
{
	update_parameters();
	return frustum_culling;
}

void camera::update_parameters()
{
	if (!is_flag(flags::dirty))
		return;

	set_flag(flags::dirty, false);

	DirectX::XMVECTOR Position, Rotation;
	DirectX::XMMATRIX View, Projection;
	float aspect_ratio;

	Position = DirectX::XMLoadFloat3(&position);
	DirectX::XMFLOAT4 quaternion = rotation;
	Rotation = DirectX::XMLoadFloat4(&quaternion);

	DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationQuaternion(Rotation);
	DirectX::XMStoreFloat3(&right, RotationMatrix.r[0]);
	DirectX::XMStoreFloat3(&up, RotationMatrix.r[1]);
	DirectX::XMStoreFloat3(&front, RotationMatrix.r[2]);

	DirectX::XMMATRIX TranslationMatrix = DirectX::XMMatrixTranslationFromVector(Position);
	View = DirectX::XMMatrixInverse(nullptr, RotationMatrix * TranslationMatrix);
	DirectX::XMStoreFloat4x4(&view_matrix, View);

	aspect_ratio = static_cast<float>(screen_rect.z - screen_rect.x) / static_cast<float>(screen_rect.w - screen_rect.y);
	Projection = DirectX::XMMatrixPerspectiveFovLH(fov_y, aspect_ratio, clip_distance.x, clip_distance.y);
	if (is_flag(flags::hybrid_projection))
	{
		//	平行投影とのブレンド処理
		float p_r = math::saturate(hybrid_rate);
		float o_r = 1.0f - p_r;
		DirectX::XMVECTOR P = DirectX::XMVectorSet(p_r, p_r, p_r, p_r);
		DirectX::XMVECTOR O = DirectX::XMVectorSet(o_r, o_r, o_r, o_r);

		DirectX::XMMATRIX OrthoProjection;
		float ortho_height = tanf(0.5f * fov_y);
		float ortho_width = ortho_height * aspect_ratio;
		OrthoProjection = DirectX::XMMatrixOrthographicLH(ortho_width, ortho_height, clip_distance.x, clip_distance.y);

		OrthoProjection.r[0] = DirectX::XMVectorMultiply(OrthoProjection.r[0], O);
		OrthoProjection.r[1] = DirectX::XMVectorMultiply(OrthoProjection.r[1], O);
		OrthoProjection.r[2] = DirectX::XMVectorMultiply(OrthoProjection.r[2], O);
		OrthoProjection.r[3] = DirectX::XMVectorMultiply(OrthoProjection.r[3], O);

		Projection.r[0] = DirectX::XMVectorMultiplyAdd(Projection.r[0], P, OrthoProjection.r[0]);
		Projection.r[1] = DirectX::XMVectorMultiplyAdd(Projection.r[1], P, OrthoProjection.r[1]);
		Projection.r[2] = DirectX::XMVectorMultiplyAdd(Projection.r[2], P, OrthoProjection.r[2]);
		Projection.r[3] = DirectX::XMVectorMultiplyAdd(Projection.r[3], P, OrthoProjection.r[3]);
	}
	DirectX::XMStoreFloat4x4(&projection_matrix, Projection);

	DirectX::XMMATRIX ViewProjection = View * Projection;
	DirectX::XMStoreFloat4x4(&view_projection_matrix, ViewProjection);

	frustum_culling.create_planes(view_projection_matrix);
	DirectX::XMFLOAT4X4 inverse_view_projection;
	DirectX::XMStoreFloat4x4(&inverse_view_projection, DirectX::XMMatrixInverse(nullptr, ViewProjection));
	frustum_culling.create_frustum_corners(inverse_view_projection);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
camera_manager::camera_manager()
{
}

camera_manager::~camera_manager()
{
}

bool camera_manager::generate_camera(camera_ptr& out_camera)
{
	out_camera = std::make_shared<camera>();
	cameras.emplace_back(out_camera);
	return true;
}

void camera_manager::get_container(camera_container& container)
{
	std::size_t constant_buffer_count = 0;

	container.cameras.clear();
	auto it = cameras.begin();
	for (; it != cameras.end();)
	{
		if (it->expired())
		{
			it = cameras.erase(it);
			continue;
		}

		auto camera = it->lock();
		if (!camera->is_flag(camera::flags::active))
			continue;

		//	プリミティブ用定数バッファ登録
		if (constant_buffer_count + 1 >= constant_bufferes.size())
		{
			generate_constant_bufferes();
		}

		camera_container::camera_data data;
		data.camera = camera;
		data.constant_buffer = constant_bufferes[constant_buffer_count++];
		container.cameras.emplace_back(data);
		++it;
	}
}

void camera_manager::generate_constant_bufferes()
{
	auto render_data = render_data::instance();
	auto device = render_data->get_device();

	HRESULT hr;
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	constant_bufferes.emplace_back();
	auto& buffer = constant_bufferes.back();
	buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
	hr = device->CreateBuffer(&buffer_desc, nullptr, buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
