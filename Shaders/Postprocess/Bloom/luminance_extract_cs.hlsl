#include "filter_functions.hlsli"

cbuffer LUMINANCE_EXTRACT_CONSTANT_BUFFER : register(b0)
{
    float bloom_threshold;
    float bloom_intensity;
    float lens_ghost_threshold;
    float lens_ghost_intensity;

    bool exposure_adjust;
    float3 dummy;
};

Texture2D<float3> source_texture : register(t0);
Texture2D<float4> average_luminance_buffer : register(t1);

//	同じサイズであること
RWTexture2D<float3> bloom_texture : register(u0);
RWTexture2D<float3> lens_ghost_texture : register(u1);

static const int3 threads = int3(32, 32, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID)
{
	float2	source_texture_size, destination_texture_size;
	source_texture.GetDimensions(source_texture_size.x, source_texture_size.y);
	bloom_texture.GetDimensions(destination_texture_size.x, destination_texture_size.y);
	int2 max_source_texture_size = source_texture_size - int2(1, 1);

	int2 hds = destination_texture_size / 2;
	int2 qds = (float2)hds / 2;
	int2 value = step(abs(dispatch_thread_id.xy - hds), qds);
	if (value.x * value.y >= 0.00001f)
	{
		float2 rate = source_texture_size / hds;
		int2 uv = (dispatch_thread_id.xy - qds) * rate;
		int2 uv00 = min(uv + int2(0, 0), max_source_texture_size);
		int2 uv10 = min(uv + int2(1, 0), max_source_texture_size);
		int2 uv01 = min(uv + int2(0, 1), max_source_texture_size);
		int2 uv11 = min(uv + int2(1, 1), max_source_texture_size);

		float3 color = (source_texture[uv00] + source_texture[uv10] + source_texture[uv01] + source_texture[uv11]) * 0.25f;

		float3 normalize_color = color;
		if (exposure_adjust)
		{
			float average_luminance = average_luminance_buffer[int2(0, 0)].r;
			normalize_color = adjust_exposure_to_middle_gray(color, average_luminance);
		}
		float scene_luminance = convert_rgb_to_luminance(normalize_color);

		//	ブルーム
		if (bloom_intensity >= 0.0f)
		{
			float luminance = step(bloom_threshold, scene_luminance);
			bloom_texture[dispatch_thread_id.xy] = color * luminance * bloom_intensity;
		}
		//	レンズゴースト
		if (lens_ghost_intensity >= 0.0f)
		{
			float luminance = step(lens_ghost_threshold, scene_luminance);
			lens_ghost_texture[dispatch_thread_id.xy] = color * luminance * lens_ghost_intensity;
		}
	}
}
