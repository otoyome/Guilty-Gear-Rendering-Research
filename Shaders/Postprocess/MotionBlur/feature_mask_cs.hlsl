#include "motion_blur.hlsli"

Texture2D<float> scene_depth_map : register(t0);
RWTexture2D<float> feature_mask_map : register(u0);

static const int3 threads = int3(8, 8, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID, int3 group_id : SV_GroupID)
{
    int2 tex_size = (int2)0;
    scene_depth_map.GetDimensions(tex_size.x, tex_size.y);

    //  Sobelフィルター
    uint2 p = dispatch_thread_id.xy;
    float d00 = scene_depth_map.Load(int3(p + neigbor_max_offsets[0], 0));
    float d10 = scene_depth_map.Load(int3(p + neigbor_max_offsets[1], 0));
    float d20 = scene_depth_map.Load(int3(p + neigbor_max_offsets[2], 0));
    float d01 = scene_depth_map.Load(int3(p + neigbor_max_offsets[3], 0));
    float d21 = scene_depth_map.Load(int3(p + neigbor_max_offsets[4], 0));
    float d02 = scene_depth_map.Load(int3(p + neigbor_max_offsets[5], 0));
    float d12 = scene_depth_map.Load(int3(p + neigbor_max_offsets[6], 0));
    float d22 = scene_depth_map.Load(int3(p + neigbor_max_offsets[7], 0));

    float gx = (d20 + 2 * d21 + d22) - (d00 + 2 * d01 + d02);
    float gy = (d02 + 2 * d12 + d22) - (d00 + 2 * d10 + d20);
    // エッジほど 1.0 に近づくマスク
    feature_mask_map[p] = saturate(abs(gx) + abs(gy));
}
