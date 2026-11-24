#include "scene_constant_buffer.hlsli"

SamplerState point_sampler : register(s0);
SamplerState linear_sampler : register(s1);

//  SMAA用の変数定義
#define SMAA_HLSL_4_1

#if 01
//  プリセット情報
#define SMAA_PRESET_ULTRA
#else
cbuffer SMAA_CONSTANT_BUFER : register(b2)
{
	float smaa_threshold;
    uint smaa_max_search_steps;
    uint smaa_max_search_steps_diagonal;
    uint smaa_corner_rounding;

	float smaa_local_contrast_adaptation_factor;
    uint2 smaa_dummy;
    bool smaa_draw_edge;
};
#define SMAA_THRESHOLD smaa_threshold
#define SMAA_MAX_SEARCH_STEPS smaa_max_search_steps
#define SMAA_MAX_SEARCH_STEPS_DIAG smaa_max_search_steps_diagonal
#define SMAA_CORNER_ROUNDING smaa_corner_rounding
#define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR smaa_local_contrast_adaptation_factor
#endif

#define SMAA_LINEAR_SAMPLER linear_sampler
#define SMAA_POINT_SAMPLER point_sampler
#define SMAA_RT_METRICS float4(get_viewport_size().zwxy)
#include "subpixel_morphological_anti_aliasing/SMAA.hlsli"

struct EDGE_DETECTION_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 offset[3] : TEXCOORD1;
};

struct BLENDING_WEIGHT_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
    float4 offset[3] : TEXCOORD2;
};

struct NEIGHBORHOOD_BLENDING_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 offset : TEXCOORD1;
};

// VS用の頂点計算
void GetPositionUV(out float4 position, out float2 uv, in uint index)
{
    const float2 vId = float2(index / 2u, index % 2u);
    float2 screenPosition = float2(2.0f, 2.0f) * vId.xy * get_viewport_size().xy;
    screenPosition = float2(2.0f, -2.0f) * screenPosition + float2(-1.0f, 1.0f);

    position = float4(screenPosition.xy, 0.1f, 1.0f);
    uv = float2(2.0f, 2.0f) * vId.xy * get_viewport_size().xy;
}
