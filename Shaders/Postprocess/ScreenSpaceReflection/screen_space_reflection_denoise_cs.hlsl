#include "screen_space_reflection.hlsli"

#include "screen_space_reflection_denoise.hlsli"

#define	THREAD_X	(16)
#define	THREAD_Y	(16)

static	const	int2	denoise_offset[25]	=
{
	int2( -2, -2 ), int2( -1, -2 ), int2(  0, -2 ), int2(  1, -2 ), int2(  2, -2 ),
	int2( -2, -1 ), int2( -1, -1 ), int2(  0, -1 ), int2(  1, -1 ), int2(  2, -1 ),
	int2( -2,  0 ), int2( -1,  0 ), int2(  0,  0 ), int2(  1,  0 ), int2(  2,  0 ),
	int2( -2,  1 ), int2( -1,  1 ), int2(  0,  1 ), int2(  1,  1 ), int2(  2,  1 ),
	int2( -2,  2 ), int2( -1,  2 ), int2(  0,  2 ), int2(  1,  2 ), int2(  2,  2 )
};

static	const	float	denoise_kernel[25]	=
{
#if 01   //  σ = 0.5
    6.96247895e-08f, 2.80886488e-05f, 0.000207548583f, 2.80886488e-05f, 6.96247895e-08f,
    2.80886488e-05f,   0.0113317696f,   0.0837310776f,   0.0113317696f, 2.80886488e-05f,
    0.000207548583f,   0.0837310776f,    0.618693590f,   0.0837310776f, 0.000207548583f,
    2.80886488e-05f, 0.0113317696f, 0.0837310776f, 0.0113317696f, 2.80886488e-05f,
    6.96247895e-08f, 2.80886488e-05f, 0.000207548583f, 2.80886488e-05f, 6.96247895e-08f,
#else
    1.0f / 256.0f, 1.0f /  64.0f, 3.0f / 128.0f, 1.0f /  64.0f, 1.0f / 256.0f,
	1.0f /  64.0f, 1.0f /  16.0f, 3.0f /  32.0f, 1.0f /  16.0f, 1.0f /  64.0f,
	3.0f / 128.0f, 3.0f /  32.0f, 9.0f /  64.0f, 3.0f /  32.0f, 3.0f / 128.0f,
	1.0f /  64.0f, 1.0f /  16.0f, 3.0f /  32.0f, 1.0f /  16.0f, 1.0f /  64.0f,
	1.0f / 256.0f, 1.0f /  64.0f, 3.0f / 128.0f, 1.0f /  64.0f, 1.0f / 256.0f
#endif
};

static	const	int	adjust_range = 8;
groupshared float4 block_colors[THREAD_Y + adjust_range * 2][THREAD_X + adjust_range * 2];
groupshared float4 block_normal_depths[THREAD_Y + adjust_range * 2][THREAD_X + adjust_range * 2];

static	const	int2 block_left_top = int2(0, 0);
static	const	int2 block_right_bottom = int2(THREAD_X + adjust_range * 2 - 1, THREAD_Y + adjust_range * 2 - 1);

Texture2D<float4> source_color_map : register(t0);
Texture2D<float4> source_normal_depth_map : register(t1);
RWTexture2D<float4> destination_color_map : register(u0);

float rand(float seed)
{
    return frac(sin(seed) * 43758.5453123f);
}

[numthreads(THREAD_X, THREAD_Y, 1)]
void main(int3 group_id : SV_GroupID, int3 group_thread_id : SV_GroupThreadID, int3 dispatch_thread_id : SV_DispatchThreadID, uint group_index : SV_GROUPINDEX)
{
	int2 left_top_position = (int2)0;
	int2 right_bottom_position = texture_size - 1;
    
    int offset_count = min(2 * pow(2, iteration) + 0.1f, adjust_range);
    float denoise_step_scale = pow(2.0f, +iteration);
    float denoise_sigma_scale = pow(2.0f, -iteration);

	//	色情報キャッシュ
	{
        int2 x_range = (group_thread_id.x == 0) ? int2(-offset_count, 1) : (group_thread_id.x == THREAD_X - 1) ? int2(THREAD_X - 1, THREAD_X + offset_count) : int2(group_thread_id.x, group_thread_id.x + 1);
        int2 y_range = (group_thread_id.y == 0) ? int2(-offset_count, 1) : (group_thread_id.y == THREAD_Y - 1) ? int2(THREAD_Y - 1, THREAD_Y + offset_count) : int2(group_thread_id.y, group_thread_id.y + 1);
        
		for (int x = x_range.x; x < x_range.y; ++x)
		{
			for (int y = y_range.x; y < y_range.y; ++y)
            {
                //  ブロック座標を算出
                int x_position = x + offset_count;
                int y_position = y + offset_count;
                int2 block_position = clamp(int2(x_position, y_position), block_left_top, block_right_bottom);

                //  テクスチャ座標を算出
                int2 position = group_id.xy * int2(THREAD_X, THREAD_Y) + int2(x, y);
				position = clamp(position, left_top_position, right_bottom_position);
				int3 texture_position = uint3(position.xy, 0);

                //  情報キャッシュ
				block_colors[block_position.y][block_position.x] = source_color_map.Load(texture_position);
                block_normal_depths[block_position.y][block_position.x] = source_normal_depth_map.Load(texture_position);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    int3 center_texture_position = int3(group_thread_id.xy + int2(offset_count, offset_count), 0);
    float4 final_color;
	{
	    //	中心色取得
        float4 center_color = block_colors[center_texture_position.y][center_texture_position.x];
        float4 center_normal_depth = block_normal_depths[center_texture_position.y][center_texture_position.x];
        float center_luminance = convert_rgb_to_luminance(center_color.rgb);

        float ang = 2.0f * 3.1415926535f * rand(251.12860182f * group_thread_id.x + 729.9126812 * group_thread_id.y + 5.1839513 * time_data.x);
        float2x2 rotation_matrix = float2x2(cos(ang), sin(ang), -sin(ang), cos(ang));
    
        //  フィルタリング
        float4 sum_color = (float4) 0.0f;
        float sum_weight = 0.0f;
        for (int ii = 0; ii < 25; ++ii)
        {
            float2 offset = (float2) denoise_offset[ii];
            int2 texture_position = center_texture_position.xy + mul(offset, rotation_matrix) * denoise_step_scale + (float2) 0.1f;

            float4 color = block_colors[texture_position.y][texture_position.x];
            float4 normal_depth = block_normal_depths[texture_position.y][texture_position.x];

            //  輝度の重み
            float luminance = convert_rgb_to_luminance(color.rgb);
            float luminance_dist = center_luminance - luminance;
            float weight_color = min(exp(-luminance_dist / (denoise_color_sigma * denoise_sigma_scale)), 1.0f);

            //  法線の重み
            float normal_dist = max(0.0f, dot(normal_depth.xyz, center_normal_depth.xyz));
            float weight_normal = pow(normal_dist, denoise_normal_sigma);

            //  深度の重み
            float delta_depth = center_normal_depth.w - normal_depth.w;
            float depth_dist = dot(delta_depth, delta_depth);
            float weight_depth = min(exp(-depth_dist / (denoise_depth_sigma * denoise_sigma_scale)), 1.0f);

            float total_weight = weight_color * weight_normal * weight_depth;
            sum_color += color * total_weight * denoise_kernel[ii];
            sum_weight += total_weight * denoise_kernel[ii];
        }
        final_color = sum_color / sum_weight;
    }

    destination_color_map[dispatch_thread_id.xy] = final_color;
}
