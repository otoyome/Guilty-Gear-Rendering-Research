#include "depth_of_field.hlsli"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RWTexture2D<float4> destination_texture : register(u0);
StructuredBuffer<int4> source_buffer : register(t0);

int4 read_from_buffer(int2 location)
{
    const uint offset = calaculate_buffer_offset(location);
    return source_buffer[offset];
}

float4 read_result(int2 location)
{
    float4 color = read_from_buffer(location + max_blur_radius);
    color /= max(color.a, 0.000001f);
    color = decompress_rgba_to_log(color);
    return color;
}

[numthreads(8, 8, 1)]
void main(uint3 position : SV_DispatchThreadID)
{
    float4 color = read_result(position.xy);
    destination_texture[position.xy] = float4(color.rgb, 1.0f);
}

