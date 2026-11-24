#include "motion_blur.hlsli"

Texture2D<float2> velocity_map : register(t0);
Texture2D<float4> scene_color_map : register(t1);
RWTexture2D<float2> neighbor_max_velocity_map : register(u0);
RWTexture2D<float4> copy_scene_color_map : register(u1);

static const int3 threads = int3(8, 8, 1);
static const int2 tile_buffer_size = threads.xy + int2(2, 2);

groupshared float3 tile_max_velocity_buffer[tile_buffer_size.y][tile_buffer_size.x];

float3 calculate_tile_velocity(int2 position)
{
    int2 tex_size = (int2) 0;
    velocity_map.GetDimensions(tex_size.x, tex_size.y);

    //  UV範囲算出
    int4 uv_range = clamp((position.xyxy + int4(0, 0, 1, 1)) * tile_size.xxxx, 0, tex_size.xyxy);

    //  タイルの最大速度算出
    float3 max_velocity = (float3) 0.0f;
    for (int y = uv_range.y; y < uv_range.w; ++y)
    {
        for (int x = uv_range.x; x < uv_range.z; ++x)
        {
            float3 temp_velocity;
            temp_velocity.xy = velocity_map.Load(int3(x, y, 0)).xy;
            temp_velocity.z = dot(temp_velocity.xy, temp_velocity.xy);
            max_velocity = lerp(max_velocity, temp_velocity, step(max_velocity.z, temp_velocity.z));
        }
    }
    return max_velocity;
}

[numthreads(threads.x, threads.y, threads.z)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID, int3 group_thread_id : SV_GroupThreadID, int3 group_id : SV_GroupID)
{
    int2 tex_size = (int2)0;
    velocity_map.GetDimensions(tex_size.x, tex_size.y);

    //  タイル化
    {
        //  UV範囲算出
        int4 uv_range = clamp((dispatch_thread_id.xyxy + int4(0, 0, 1, 1)) * tile_size.xxxx, 0, tex_size.xyxy);

        float3 max_velocity = (float3) 0.0f;
	    for (int y = uv_range.y; y < uv_range.w; ++y)
        {
		    for (int x = uv_range.x; x < uv_range.z; ++x)
            {
                float3 temp_velocity;
                temp_velocity.xy  = velocity_map.Load(int3(x, y, 0)).xy;
                temp_velocity.z = dot(temp_velocity.xy, temp_velocity.xy);
                max_velocity = lerp(max_velocity, temp_velocity, step(max_velocity.z, temp_velocity.z));
            
                //  色コピー
                copy_scene_color_map[int2(x, y)] = scene_color_map.Load(int3(x, y, 0));
            }
        }
        tile_max_velocity_buffer[group_thread_id.y + 1][group_thread_id.x + 1] = max_velocity;

        //  雑に縁の対応
        if (group_thread_id.x == 0)
        {
            int2 position = max(dispatch_thread_id.xy - int2(1, 0), 0);
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[group_thread_id.y + 1][0] = max_velocity;
        }
        if (group_thread_id.y == 0)
        {
            int2 position = max(dispatch_thread_id.xy - int2(0, 1), 0);
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[0][group_thread_id.x + 1] = max_velocity;
        }
        if (group_thread_id.x == threads.x - 1)
        {
            neighbor_max_velocity_map.GetDimensions(tex_size.x, tex_size.y);
            int2 position = min(dispatch_thread_id.xy + int2(1, 0), tex_size.xy - int2(1, 1));
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[group_thread_id.y + 1][tile_buffer_size.x - 1] = max_velocity;
        }
        if (group_thread_id.y == threads.y - 1)
        {
            neighbor_max_velocity_map.GetDimensions(tex_size.x, tex_size.y);
            int2 position = min(dispatch_thread_id.xy + int2(0, 1), tex_size.xy - int2(1, 1));
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[tile_buffer_size.y - 1][group_thread_id.x + 1] = max_velocity;
        }
        if (group_thread_id.x == 0 && group_thread_id.y == 0)
        {
            int2 position = max(dispatch_thread_id.xy - int2(1, 1), 0);
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[0][0] = max_velocity;
        }
        if (group_thread_id.x == 0 && group_thread_id.y == threads.y - 1)
        {
            int2 position = max(dispatch_thread_id.xy + int2(-1, 1), 0);
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[0][tile_buffer_size.x - 1] = max_velocity;
        }
        if (group_thread_id.x == threads.x - 1 && group_thread_id.y == 0)
        {
            int2 position = max(dispatch_thread_id.xy + int2(1, -1), 0);
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[tile_buffer_size.x - 1][0] = max_velocity;
        }
        if (group_thread_id.x == threads.x - 1 && group_thread_id.y == threads.y - 1)
        {
            neighbor_max_velocity_map.GetDimensions(tex_size.x, tex_size.y);
            int2 position = min(dispatch_thread_id.xy + int2(1, 1), tex_size.xy - int2(1, 1));
            float3 max_velocity = calculate_tile_velocity(position);
        
            tile_max_velocity_buffer[tile_buffer_size.y - 1][tile_buffer_size.x - 1] = max_velocity;
        }

        GroupMemoryBarrierWithGroupSync();
    }

    //  近傍タイルを確認して最も早いものを利用する
    float3 max_velocity = tile_max_velocity_buffer[group_thread_id.y + 1][group_thread_id.x + 1];
    for (int index = 0; index < 8; ++index)
    {
        int2 uv = group_thread_id.xy + neigbor_max_offsets[index];

        float3 temp_velocity = tile_max_velocity_buffer[uv.y + 1][uv.x + 1];
        max_velocity = lerp(max_velocity, temp_velocity, step(max_velocity.z, temp_velocity.z));
    }

    neighbor_max_velocity_map[dispatch_thread_id.xy] = max_velocity.xy;
}
