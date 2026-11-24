RWTexture2D<float3>	color_texture	: register( u0 );

static const uint3 threads = int3(32, 32, 1);

groupshared	float3	chached_total_weight[threads.y][threads.x];

[numthreads(threads.x, threads.y, threads.z)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	uint y, x;
	uint2 texture_size;
	color_texture.GetDimensions(texture_size.x, texture_size.y);
	texture_size.xy	/= threads.xy;

	//	重みをすべて加算
	float3 total_weight = 0;
	for (y = dispatch_thread_id.y * texture_size.y; y < (dispatch_thread_id.y + 1) * texture_size.y; ++y)
	{
		for (x = dispatch_thread_id.x * texture_size.x; x < (dispatch_thread_id.x + 1) * texture_size.x; ++x)
		{
			total_weight += color_texture[uint2(x, y)];
		}
	}
	chached_total_weight[dispatch_thread_id.y][dispatch_thread_id.x] = total_weight;
	GroupMemoryBarrierWithGroupSync();

	//	スケーリング値を算出
	total_weight = 0;
	for (y = 0; y < threads.y; ++y)
	{
		for (x = 0; x < threads.x; ++x)
		{
			total_weight += chached_total_weight[y][x];
		}
	}

	//	平均化
	for (y = dispatch_thread_id.y * texture_size.y; y < (dispatch_thread_id.y + 1) * texture_size.y; ++y)
	{
		for (x = dispatch_thread_id.x * texture_size.x; x < (dispatch_thread_id.x + 1) * texture_size.x; ++x)
		{
			color_texture[uint2(x, y)] /= total_weight;
		}
	}
}

