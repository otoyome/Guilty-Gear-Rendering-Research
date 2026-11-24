#ifndef __SCENE_CONSTANT_BUFFER_HLSLI__
#define __SCENE_CONSTANT_BUFFER_HLSLI__

cbuffer SCENE_CONSTANT_BUFFER : register(b0)
{
	float4 time_data;	//	x : 現在時間, y : 前時間, zw : 空き
    float4 z_buffer_parameteres; //	非線形深度から線形深度へ変換するためのパラメーター
    float4 camera_position; //  xyz : 座標, w : fovY
    float4 camera_right; //  xyz : 方向, w : tan(fovY)
    float4 camera_up; //  xyz : 方向, w : 空き
    float4 camera_direction; //  xyz : 方向, w : 空き
    float4 camera_clip_distance;
    float4 viewport_rect;   //  xy : 左上, zw : 右下
    row_major float4x4 view_transform;
    row_major float4x4 projection_transform;
    row_major float4x4 view_projection_transform;
    row_major float4x4 inverse_view_transform;
    row_major float4x4 inverse_projection_transform;
    row_major float4x4 inverse_view_projection_transform;

    row_major float4x4 previous_view_projection_transform;
};

inline float4 get_viewport_size()
{
    float2 s = float2(viewport_rect.zw - viewport_rect.xy);
    return float4(s, 1.0f / s);
}

inline float get_elapsed_time()
{
    return time_data.x - time_data.y;
}

inline float get_fov_y()
{
    return camera_position.w;
}

inline float get_tan_fov_y()
{
    return camera_right.w;
}

#endif  //  __SCENE_CONSTANT_BUFFER_HLSLI__
