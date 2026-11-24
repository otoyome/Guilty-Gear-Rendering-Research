#ifndef __MOTION_BLUR_HLSLI__
#define __MOTION_BLUR_HLSLI__

#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"

//  速度バッファをタイル化する目的で使用するブロックサイズ
//  ここはCPU側と同期しておく必要がある
static const int tile_size = 20;

cbuffer MOTION_BLUR_CONSTANT_BUFFER : register(b1)
{
    int motion_blur_iteration;
    float motion_blur_jitter;
    float motion_blur_velocity_multiplyer;
    float motion_blur_dummy;
};

//  近傍最大速度バッファオフセット
static const int2 neigbor_max_offsets[] =
{
    int2(-1, -1),
    int2(+0, -1),
    int2(+1, -1),

    int2(-1, +0),
    int2(+1, +0),

    int2(-1, +1),
    int2(+0, +1),
    int2(+1, +1),
};

#endif  //  __MOTION_BLUR_HLSLI__
