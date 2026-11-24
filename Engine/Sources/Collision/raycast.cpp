// RAYCAST
#include "Collision/raycast.h"

using namespace DirectX;

int intersect_ray_triangles
(
	const float* positions,
	const uint32_t offset,
	const uint32_t stride,
	const uint32_t* indices,
	const size_t index_count,
	const XMFLOAT4& ray_position,
	const XMFLOAT4& ray_direction,
	XMFLOAT4& intersection_point,
	XMFLOAT3& intersection_normal,
	float& distance,
	const bool RHS
)
{
	const bool CCW{ RHS };
	const int C0{ 0 };
	const int C1{ CCW ? 1 : 2 };
	const int C2{ CCW ? 2 : 1 };

	int intersection_count{ 0 };
	int intersected_triangle_index{ -1 };

	const float ray_length_limit{ distance };
	float closest_distance{ FLT_MAX };

	XMVECTOR XP{}, XN{};
	const XMVECTOR P{ XMVectorSet(ray_position.x, ray_position.y, ray_position.z, 1) };
	const XMVECTOR D{ XMVector3Normalize(XMVectorSet(ray_direction.x, ray_direction.y, ray_direction.z, 0)) };

	using byte = uint8_t;
	const byte* p{ reinterpret_cast<const byte*>(positions) + offset };
	const size_t triangle_count{ index_count / 3 };
	for (size_t triangle_index = 0; triangle_index < triangle_count; triangle_index++)
	{
		const XMVECTOR A{ XMVectorSet(
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C0] * stride))[0],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C0] * stride))[1],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C0] * stride))[2],
			1.0f
		) };
		const XMVECTOR B{ XMVectorSet(
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C1] * stride))[0],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C1] * stride))[1],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C1] * stride))[2],
			1.0f
		) };
		const XMVECTOR C{ XMVectorSet(
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C2] * stride))[0],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C2] * stride))[1],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C2] * stride))[2],
			1.0f
		) };

		XMVECTOR Q{};

		const XMVECTOR N{ XMVector3Normalize(XMVector3Cross(XMVectorSubtract(B, A), XMVectorSubtract(C, A))) };
		const float d{ XMVectorGetByIndex(XMVector3Dot(N, A), 0) };

		const float denominator{ XMVectorGetByIndex(XMVector3Dot(N, D), 0) };
		if (denominator < 0)
		{
			const float numerator{ d - XMVectorGetByIndex(XMVector3Dot(N, P), 0) };
			const float t{ numerator / denominator };

			if (t > 0 && t < ray_length_limit)
			{
				Q = XMVectorAdd(P, XMVectorScale(D, t));

				const XMVECTOR QA{ XMVectorSubtract(A, Q) };
				const XMVECTOR QB{ XMVectorSubtract(B, Q) };
				const XMVECTOR QC{ XMVectorSubtract(C, Q) };

				XMVECTOR U{ XMVector3Cross(QB, QC) };
				XMVECTOR V{ XMVector3Cross(QC, QA) };
				if (XMVectorGetByIndex(XMVector3Dot(U, V), 0) < 0)
				{
					continue;
				}

				XMVECTOR W{ XMVector3Cross(QA, QB) };
				if (XMVectorGetByIndex(XMVector3Dot(U, W), 0) < 0)
				{
					continue;
				}
				if (XMVectorGetByIndex(XMVector3Dot(V, W), 0) < 0)
				{
					continue;
				}

				if (t < closest_distance)
				{
					closest_distance = t;
					intersected_triangle_index = static_cast<int>(triangle_index);
					XP = Q;
					XN = N;
				}
				intersection_count++;
			}
		}
	}
	if (intersection_count > 0)
	{
		XMStoreFloat4(&intersection_point, XP);
		XMStoreFloat3(&intersection_normal, XN);
		distance = closest_distance;
	}
	return intersected_triangle_index;
}
