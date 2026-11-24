#ifndef __GGST_SURFACE_HLSLI__
#define __GGST_SURFACE_HLSLI__

#include "ggst_rendering.hlsli"
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;
    
    data.base_color = float4(0, 0, 0, 1);
    data.shade_color = 0;
    data.emissive_color = 0;
    data.occlusion_factor = 0;
    data.occlusion_strength = 0;
    data.unique_parameter = 0;
    data.w_normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.stencil = material_flags;

    //  ベースカラー
    data.base_color = base_color_factor;
    if (HAS_RESOURCE(BaseMap))
    {
        data.base_color *= TEXTURE2D_SAMPLE(BaseMap, linear_sampler_state, pin.texcoord[BaseTexcoord]);
    }

    data.roughness = roughness;
    data.metalness = metalness;

    data.shading_model = material_shading_model;
    return data;
}

#endif	//	__GGST_SURFACE_HLSLI__
