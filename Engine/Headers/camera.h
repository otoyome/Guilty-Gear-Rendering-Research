#pragma once

#include "collision/frustum_culling.h"

class camera
{
public:
	enum class flags
	{
		active,

		dirty,
		update_previous_matrix,

		hybrid_projection,
	};

public:
	bool is_flag(flags flag) const;
	void set_flag(flags flag, bool value);

	const DirectX::XMFLOAT3& get_position() const;
	void set_position(const DirectX::XMFLOAT3& position);

	const math::rotator& get_rotation() const;
	void set_rotation(const math::rotator& rotation);

	float get_fov_y() const;
	void set_fov_y(float fov_y);

	float get_hybrid_rate() const;
	void set_hybrid_rate(float hybrid_rate);

	const DirectX::XMFLOAT4& get_screen_rect() const;
	void set_screen_rect(DirectX::XMFLOAT4 screen_rect);
	const DirectX::XMFLOAT2& get_clip_distance() const;
	void set_clip_distance(DirectX::XMFLOAT2 clip_distance);

	const DirectX::XMFLOAT3& get_right();
	const DirectX::XMFLOAT3& get_up();
	const DirectX::XMFLOAT3& get_front();
	const DirectX::XMFLOAT4X4& get_view_matrix();
	const DirectX::XMFLOAT4X4& get_projection_matrix();
	const DirectX::XMFLOAT4X4& get_view_projection_matrix();
	const DirectX::XMFLOAT4X4& get_previous_view_projection_matrix();

	const collision::frustum_culling& get_frustum_culling();

	void cache_previous_view_projection_matrix();

public:
	camera();
	~camera();

private:
	void update_parameters();

private:
	UINT system_flags;
	DirectX::XMFLOAT3 position;
	math::rotator rotation;
	DirectX::XMFLOAT3 right;
	DirectX::XMFLOAT3 up;
	DirectX::XMFLOAT3 front;
	DirectX::XMFLOAT4X4 view_matrix;
	DirectX::XMFLOAT4X4 projection_matrix;
	DirectX::XMFLOAT4X4 view_projection_matrix;
	DirectX::XMFLOAT4X4 previous_view_projection_matrix;

	float fov_y;
	DirectX::XMFLOAT4 screen_rect;
	DirectX::XMFLOAT2 clip_distance;
	float hybrid_rate;

	collision::frustum_culling frustum_culling;
};

using camera_ptr = std::shared_ptr<camera>;

//	カメラコンテナ
struct camera_container
{
	struct camera_data
	{
		camera_ptr camera;
		Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	};
	std::vector<camera_data> cameras;
};

//	カメラマネージャー
class camera_manager
{
	SINGLETON(camera_manager)

private:
	camera_manager();
	~camera_manager();

	void generate_constant_bufferes();

public:
	bool generate_camera(camera_ptr& out_camera);
	void get_container(camera_container& container);

private:
	std::vector<std::weak_ptr<camera>> cameras;
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constant_bufferes;
};
