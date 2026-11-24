Texture2D<float3> source_texture : register(t0);
RWTexture2D<float3> destination_texture : register(u0);

static const int3 threads = int3(32, 32, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID)
{
	float2	source_texture_size, destination_texture_size;
	source_texture.GetDimensions(source_texture_size.x, source_texture_size.y);
	destination_texture.GetDimensions(destination_texture_size.x, destination_texture_size.y);

	int2 hds = destination_texture_size / 2;
	int2 hss = source_texture_size / 2;
	int2 value = step(abs(dispatch_thread_id.xy - hds), hss);

	int2 source_position = clamp(dispatch_thread_id.xy - hds + hss, 0, destination_texture_size);
	destination_texture[dispatch_thread_id.xy] = source_texture[dispatch_thread_id.xy - hds + hss] * (value.x * value.y);
}
