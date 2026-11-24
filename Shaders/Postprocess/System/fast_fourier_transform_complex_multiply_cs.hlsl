
Texture2D<float3> source_texture_real : register(t0);
Texture2D<float3> source_texture_imaginary : register(t1);
RWTexture2D<float3> destination_texture_real : register(u0);
RWTexture2D<float3> destination_texture_imaginary : register(u1);

static const int3 threads = int3(32, 32, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	float3 real0 = source_texture_real[dispatch_thread_id.xy].rgb;
	float3 imaginary0 = source_texture_imaginary[dispatch_thread_id.xy].rgb;
	float3 real1 = destination_texture_real[dispatch_thread_id.xy].rgb;
	float3 imaginary1 = destination_texture_imaginary[dispatch_thread_id.xy].rgb;

	float3 real = real0.rgb * real1.rgb - imaginary0.rgb * imaginary1.rgb;
	float3 imaginary = imaginary0.rgb * real1.rgb + real0.rgb * imaginary1.rgb;

	destination_texture_real[dispatch_thread_id.xy] = float3(real.x, real.y, real.z);
	destination_texture_imaginary[dispatch_thread_id.xy] = float3(imaginary.x, imaginary.y, imaginary.z);
}
