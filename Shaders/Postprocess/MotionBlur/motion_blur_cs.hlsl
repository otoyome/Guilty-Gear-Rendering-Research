#include "motion_blur.hlsli"

Texture2D<float4> scene_map : register(t0);
Texture2D<float> depth_map : register(t1);
Texture2D<float2> velocity_map : register(t2);
Texture2D<float2> neighbor_max_velocity_map : register(t3);

RWTexture2D<float4> input_scene_map : register(u0);

#define FLT_EPS 1.19209e-07f

float rand(float t)
{
    return frac(sin(dot(float2(t, t), float2(12.9898f, 78.233f))) * 43758.5453123f);
}

float2 rand2(float2 s)
{
    s = float2(dot(s, float2(12.9898f, 78.233f)), dot(s, float2(269.5f, 183.3f)));
    return frac(sin(s) * float2(43758.5453123f, 43758.5453123f));
}

//  速度ベクトルをタイルサイズ分に拡大させる
float2 reconst_velocity(float2 velocity)
{
    velocity *= motion_blur_velocity_multiplyer;
    return velocity * tile_size;
}

float calc_soft_depth_compare(float za, float zb)
{
    static const float SoftDepthExtend = 0.1f;
    return saturate(1.0f - (za - zb) / SoftDepthExtend);
}

float calc_cone(float L, float2 V)
{
    float ret = 1.0f - (L / length(V));
    return saturate(ret);
}

float calc_cylinder(float L, float2 V)
{
    float lv = length(V);
    return 1.0f - smoothstep(0.95f * lv, 1.05f * lv, L);
}

static const int3 threads = int3(32, 32, 1);
static const float2 half_pixel = 0.5f;
static const float half_pixel_length = length(half_pixel);

[numthreads(32, 32, 1)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    int2 tex_size = (int2)0;
    scene_map.GetDimensions(tex_size.x, tex_size.y);
    if (dispatch_thread_id.x >= tex_size.x || dispatch_thread_id.y >= tex_size.y)
        return;

    float4 color = scene_map.Load(int3(dispatch_thread_id.xy, 0));

    int2 tile_position;
    {
        float2 texture_position = (float2) dispatch_thread_id.xy / (float2) tex_size;
        uint2 max_tile_size = uint2(tex_size.xy + tile_size.xx - int2(1, 1)) / uint2(tile_size.xx);
        tile_position = int2(texture_position * max_tile_size + 0.1f);
    }

    float2 Vn = reconst_velocity(neighbor_max_velocity_map.Load(int3(tile_position, 0)));

    //  速度が小さい場合は無視
    if (length(Vn) < half_pixel_length)
    {
        input_scene_map[dispatch_thread_id.xy] = color;
        return;
    }
    
    float depth = depth_map.Load(int3(dispatch_thread_id.xy, 0));

    float2 X = (float2) dispatch_thread_id.xy;
    int3 nX = int3((int) X.x, (int) X.y, 0);
    float2 Vx = reconst_velocity(velocity_map.Load(nX));
    float Zx = depth;

    float weight = 1.0f / max(length(Vx), half_pixel_length);
    color *= weight;

    float2 random_value = rand2(dispatch_thread_id.xy / (float2)tex_size);
    float jitter = ((random_value.x + random_value.y) * 2.0f - 1.0f) * motion_blur_jitter;

    //  速度ベクトル方向にブラーをかける
    int ignore = (motion_blur_iteration - 1) >> 1;
	[loop] for (int index = 0; index < motion_blur_iteration; ++index)
	{
        if (index == ignore)
            continue;

		//  代表ベクトルはVxとVnを交互に採用
        float2 Va = Vn;
        Va = lerp(Va, Vx, (index & 0x01) != 0);

        float t = lerp(-1.0f, 1.0f, ((float) index + jitter) / motion_blur_iteration);
        float2 Y = clamp(X + Va * t + half_pixel, 0.0f, tex_size);
        int3 nY = int3((int) Y.x, (int) Y.y, 0);

        float Zy = depth_map.Load(nY);
        float2 L = length(dispatch_thread_id.xy - nY.xy);

        float2 Vy = reconst_velocity(velocity_map.Load(nY));
        float ay = calc_soft_depth_compare(Zx, Zy) * calc_cone(L.x, Vy)
                 + calc_soft_depth_compare(Zy, Zx) * calc_cone(L.y, Vx)
                 + calc_cylinder(L.x, Vy) * calc_cylinder(L.y, Vx) * 2.0f;

		weight += ay;
        color += scene_map.Load(nY) * ay;
	}

	color /= weight;

    input_scene_map[dispatch_thread_id.xy] = float4(color.xyz, 1);
}