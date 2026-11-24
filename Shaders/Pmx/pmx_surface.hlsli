#ifndef __PMX_SURFACE_HLSLI__
#define __PMX_SURFACE_HLSLI__

#include "pmx_rendering.hlsli"
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"
#include "forward_light_data.hlsli"

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

    data.roughness = roughness;
    data.metalness = metalness;

    bool is_outline_pixel = pin.w_normal.w > 0.5f;
    if (is_outline_pixel)
    {
        data.base_color.rgb = edge_color.rgb;
        data.roughness = 1.0f;
    }
    else
    {
        //  ベースカラー
        data.emissive_color = base_color_factor;
        if (HAS_RESOURCE(BaseMap))
        {
            data.emissive_color *= TEXTURE2D_SAMPLE(BaseMap, linear_sampler_state, pin.texcoord[BaseTexcoord]);
        }

        //  平行光源を一つだけ処理してライティング
        if (1 >= forward_only_light_counts.x)
        {
            directional_lights directional_light = forward_only_directional_light_data[0];
            float3 L = normalize(directional_light.direction.xyz);
            float2 ramp_texcoord = float2(0.5f, saturate(dot(L, data.w_normal.xyz) * 0.5f + threshold_offset));
            float3 ramp_color = 1.0f;
            if (HAS_RESOURCE(RampMap))
            {
                ramp_color = TEXTURE2D_SAMPLE(RampMap, linear_sampler_state, ramp_texcoord).rgb;
            }
            data.emissive_color *= ramp_color;
        }
    }
    data.shading_model = material_shading_model;
    return data;
}

#endif	//	__PMX_SURFACE_HLSLI__
