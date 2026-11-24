#include "tone_mapping.hlsli"

////////////////////////////////////////////////////////////////////////////////////////////////////////
Texture2D<float4> scene_color_map : register(t0);
Texture2D<float4> previous_average_luminance_buffer : register(t1);
RWTexture2D<float> average_luminance_buffer : register(u0);

////////////////////////////////////////////////////////////////////////////////////////////////////////
static const int3 threads = int3(32, 32, 1);

groupshared float block_luminance_value[threads.y][threads.x];

////////////////////////////////////////////////////////////////////////////////////////////////////////
uint round_up_to_multiple(uint n, uint m)
{
    return ((n + m - 1) / m) * m;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(threads.x, threads.y, threads.z)]
void main(uint3 group_thread_id : SV_GroupThreadID, uint group_index : SV_GroupIndex)
{
    int y, x;

    int2 tex_size = (int2) 0;
    scene_color_map.GetDimensions(tex_size.x, tex_size.y);

    //  •½‹Ï‹P“x
    {
        uint2 round_up_size = uint2(round_up_to_multiple(tex_size.x, threads.x), round_up_to_multiple(tex_size.y, threads.y));
        int2 loop_count = round_up_size / threads.xy;
        int2 loop_offset = group_thread_id.xy * loop_count;
        loop_count = min(tex_size - loop_offset, loop_count);

        float total_value = 0;
        for (y = 0; y < loop_count.y; ++y)
        {
            for (x = 0; x < loop_count.x; ++x)
            {
                float3 color = scene_color_map[loop_offset + int2(x, y)].rgb;
                float value = convert_rgb_to_luminance(color);
                value = clamp(value, exposure_luminance_range.x, exposure_luminance_range.y);
                total_value += value;
            }
        }
        block_luminance_value[group_thread_id.y][group_thread_id.x] = total_value / max(1, loop_count.x * loop_count.y);
    }

    GroupMemoryBarrierWithGroupSync();

    if (group_index == 0)
    {
        float average_luminance = 0.0f;
        for (y = 0; y < threads.y; ++y)
        {
            for (x = 0; x < threads.x; ++x)
            {
                average_luminance += block_luminance_value[y][x];
            }
        }

        average_luminance /= (threads.x * threads.y);
        
        float previous_luminance = previous_average_luminance_buffer[int2(0, 0)];
        average_luminance = lerp(previous_luminance, average_luminance, exposure_adapt_rate);
        if (!isnan(average_luminance))
            average_luminance_buffer[int2(0, 0)] = average_luminance;
    }
}
