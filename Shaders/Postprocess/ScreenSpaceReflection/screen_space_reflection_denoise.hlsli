#ifndef __SCREEN_SPACE_DENOISE_HLSLI__
#define __SCREEN_SPACE_DENOISE_HLSLI__

#include "filter_functions.hlsli"

//  G-BufferにマテリアルID、インスタンスID等があればここをONにする
//#define USE_MATERIAL_AND_INSTANCE_SLOT

cbuffer DENOISE_CONSTANT_BUFFER : register(b3)
{
    uint2 texture_size;
    float reset_flag;
    float iteration;

    float denoise_velocity_max;
    float denoise_color_sigma;
    float denoise_normal_sigma;
    float denoise_depth_sigma;

    float denoise_temporal_blend_factor;
    float denoise_counter_max;
    uint2 denoise_dummy;
};

#endif  //  __SCREEN_SPACE_DENOISE_HLSLI__
