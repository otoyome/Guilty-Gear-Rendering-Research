Texture2D<float3> source_texture : register(t0);
RWTexture2D<float3> destination_texture : register(u0);

static const int3 threads = int3(32, 32, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	float2 source_texture_size, destination_texture_size;
	source_texture.GetDimensions(source_texture_size.x, source_texture_size.y);
	destination_texture.GetDimensions(destination_texture_size.x, destination_texture_size.y);

	if (dispatch_thread_id.x >= destination_texture_size.x || dispatch_thread_id.y >= destination_texture_size.y)
		return;

	float2 rate = source_texture_size / destination_texture_size;

	float2 start = rate * (dispatch_thread_id.xy + 0);// + float2(0.5f, 0.5f);
	float2 end = rate * (dispatch_thread_id.xy + 1);// + float2(0.5f, 0.5f);
	float4 color = 0;
	for (int x = start.x; x < end.x; ++x)
	{
		for (int y = start.y; y < end.y; ++y)
		{
			color.xyz += source_texture[int2(x, y)];
			color.w += 1.0f;
		}
	}
	destination_texture[dispatch_thread_id.xy] = color.xyz / color.w;
}
