cbuffer FAST_FOURIE_TRANSFORM_CONSTANT_BUFFER : register(b0)
{
    float deviation;
    float3 dummy;
};

RWTexture2D<float3> texture_real : register(u0);
RWTexture2D<float3> texture_imaginary : register(u1);

static const int3 threads = int3(32, 32, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	float2 texture_size;
	texture_real.GetDimensions(texture_size.x, texture_size.y);

	float3 real_colors = texture_real[dispatch_thread_id.xy];
	float3 imaginary_colors = texture_imaginary[dispatch_thread_id.xy];

	float len = length(dispatch_thread_id.xy / texture_size - float2(0.5f, 0.5f));
	float weight = exp(-(len * len * deviation * deviation) / 2.0f);

	texture_real[dispatch_thread_id.xy]	= real_colors * weight;
	texture_imaginary[dispatch_thread_id.xy] = imaginary_colors * weight;
}

