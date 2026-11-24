#ifndef __3D_FAILED_SURFACE_HLSLI__
#define __3D_FAILED_SURFACE_HLSLI__

#include "3d_failed_rendering.hlsli"
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

//  ñ èÓïÒ
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;
    
    data.base_color = float4(0.5f, 0, 0.5f, 1);
    data.roughness = 1;
    data.w_normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.stencil = material_flags;

    data.shading_model = shading_model_unlit;
    return data;
}

#endif	//	__3D_FAILED_SURFACE_HLSLI__
