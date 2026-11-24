#include "bloom.hlsli"

RWTexture2D<float3> scene_texture : register(u0);
Texture2D<float3> luminance_extract_texture : register(t0);

static const int3 threads = int3(32, 32, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	float2 source_texture_size, destination_texture_size;
	luminance_extract_texture.GetDimensions(source_texture_size.x, source_texture_size.y);
	scene_texture.GetDimensions(destination_texture_size.x, destination_texture_size.y);

	if (dispatch_thread_id.x >= destination_texture_size.x || dispatch_thread_id.y >= destination_texture_size.y)
		return;

	int2 quater_size = source_texture_size / 4;
	int2 half_size = source_texture_size / 2;

	float2 rate = source_texture_size / destination_texture_size;
	int2 position = (int2)(dispatch_thread_id.xy * rate) / 2 + quater_size;
	position = clamp(position, quater_size, quater_size * 3 - 1);
	scene_texture[dispatch_thread_id.xy] += luminance_extract_texture[position];
}
