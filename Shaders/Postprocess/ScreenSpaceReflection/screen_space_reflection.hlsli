#ifndef __SCREEN_SPACE_REFLECTION_HLSLI__
#define __SCREEN_SPACE_REFLECTION_HLSLI__

#include "fullscreen_quad.hlsli"
#include "scene_constant_buffer.hlsli"
#include "gbuffer.hlsli"

cbuffer SCREEN_SPACE_REFLECTION_CONSTANT_BUFFER : register(b1)
{
    float ray_max_distance;         //  反射最大距離
    float ray_resolution;           //  レイのステップ解像度
    float2 ray_tickness;            //  深度判定を行う際の厚みの範囲

    int ray_substep_iteration_max;  //	ステップ深度チェック細分化回数
    int ray_flags;                  //  処理の設定フラグ
    float2 dummy;
    float4 viewport_size;           //  シーンのビューポートサイズ
};

static const int ray_flags_reflection_dir_adjust = 0;
static const int ray_flags_consideration_uv_hole = 1;
static const int ray_flags_substep_check = 2;
static const int ray_flags_step_random = 3;

bool is_flag(int bitflag)
{
    return ray_flags & (1 << bitflag);
}

#endif  //  __SCREEN_SPACE_REFLECTION_HLSLI__
