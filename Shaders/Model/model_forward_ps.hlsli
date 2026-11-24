//  VS_OUT
//  surface関数
//  を作る事

#include "gbuffer.hlsli"
#include "shading_models.hlsli"
#include "forward_light_data.hlsli"

float4 main(VS_OUT pin, bool is_front_face : SV_IsFrontFace) : SV_TARGET
{
    SurfaceData surface_data = surface(pin, is_front_face);

    //  ライティング無しの場合はそのまま色を出力
    if (surface_data.shading_model == shading_model_unlit)
    {
        float4 color = surface_data.base_color;
        color.rgb += surface_data.emissive_color.rgb;
        return color;
    }
    
    GBufferData data = convert_g_buffer_data(surface_data);

    uint index;
    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    
    //  平行光源
    [unroll]
    for (index = 0; index < directional_light_count; ++index)
    {
        if (index >= forward_only_light_counts.x)
            break;

        directional_lights directional_light = forward_only_directional_light_data[index];
        float3 L = normalize(directional_light.direction.xyz);

        float shading_attenuation = 1.0f;
        float4 shadowing_attenuation = 1.0f;

        //  影処理
        if (shadow_map_count > 0 && IsGBufferFlags(data, material_attribute_shadow_receiver))
        {
            shadowing_attenuation = shadowing_process(data.w_position, view_transform);
        }

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = shadowing_attenuation;
        lighting_data.direction = L;
        lighting_data.color = directional_light.color.rgb * directional_light.color.a;
        lighting_data.camera_position = camera_position.xyz;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }

    //  点光源
    [unroll]
    for (index = 0; index < point_light_count; ++index)
    {
        if (index >= forward_only_light_counts.y)
            break;
        
        point_lights point_light = forward_only_point_light_data[index];
        float3 L = data.w_position.xyz - point_light.position.xyz;
        float len = length(L);
        if (len >= point_light.range)
            continue;

        L /= len;
            
        float attenuateLength = saturate(1.0f - len / point_light.range);
        float shading_attenuation = attenuateLength * attenuateLength;

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = 1;
        lighting_data.direction = L;
        lighting_data.color = point_light.color.rgb * point_light.color.a;
        lighting_data.camera_position = camera_position.xyz;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }

    //  スポットライト
    [unroll]
    for (index = 0; index < spot_light_count; ++index)
    {
        if (index >= forward_only_light_counts.z)
            break;

        spot_lights spot_light = forward_only_spot_light_data[index];
        float3 L = data.w_position.xyz - spot_light.position.xyz;
        float len = length(L);
        if (len >= spot_light.range)
            continue;
        
        float attenuateLength = saturate(1.0f - len / spot_light.range);
        float shading_attenuation = attenuateLength * attenuateLength;
        L /= len;
        float3 spotDirection = normalize(spot_light.direction.xyz);
        float innerCorn = cos(spot_light.innerCorn);
        float outerCorn = cos(spot_light.outerCorn);
        float angle = dot(spotDirection, L);
        float area = innerCorn - outerCorn;
        shading_attenuation *= saturate(1.0f - (innerCorn - angle) / area);

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = 1;
        lighting_data.direction = L;
        lighting_data.color = spot_light.color.rgb * spot_light.color.a;
        lighting_data.camera_position = camera_position.xyz;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }

    //  IBL
    if (!IsGBufferFlags(data, material_attribute_indirect_lighting_off))
    {
        float3 albedo = data.base_color.rgb;
        float3 N = normalize(data.w_normal);
        float3 V = normalize(data.w_position.xyz - camera_position.xyz);

	    //	入射光のうち拡散反射になる割合
        float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, data.metalness);

	    //	垂直反射時のフレネル反射率
        float3 F0 = lerp(0.0f, albedo.rgb, data.metalness);
        float3 color_adjust = forward_only_light_probe_data.color.rgb * forward_only_light_probe_data.color.a;

        float3 diffuse = 0, specular = 0;
        switch (data.shading_model)
        {
            case shading_model_default_lit:
            {
                diffuse += DiffuseIBL(N, V, data.roughness, diffuse_reflectance, F0, forward_only_diffuse_iem, forward_only_ibl_sampler_state) * color_adjust;
                specular += SpecularIBL(N, V, data.roughness, F0, forward_only_lut_ggx, forward_only_specular_pmrem, forward_only_ibl_sampler_state) * color_adjust;
                break;
             }
//	[エンジン改造:シェーディング]
            case shading_model_ggst_lit:
            case shading_model_toon_lit:
            {
                // ココでは何もしない
                break;
            }
//	[エンジン改造:シェーディング]
        }

	    //	自己遮蔽
	    total_diffuse += lerp(diffuse, diffuse * data.occlusion_factor, data.occlusion_strength);
	    total_specular += lerp(specular, specular * data.occlusion_factor, data.occlusion_strength);
    }

    //  色返却
    return float4(total_diffuse + total_specular, surface_data.base_color.a);
}

