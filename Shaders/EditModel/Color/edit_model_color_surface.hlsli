#ifndef __EDIT_MODEL_COLOR_SURFACE_HLSLI__
#define __EDIT_MODEL_COLOR_SURFACE_HLSLI__

#include "edit_model_color_rendering.hlsli"
#include "gbuffer.hlsli"

//  ñ èÓïÒ
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;
    data.base_color = float4(0, 0, 0, 1);
    data.emissive_color = pin.color[0].rgb;
    data.w_normal = normalize(pin.w_normal.xyz);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.shading_model = material_shading_model;
    data.stencil = material_flags;
    return data;
}

#endif	//	__EDIT_MODEL_COLOR_SURFACE_HLSLI__
