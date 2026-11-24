#ifndef __SHADING_MODEL_HLSLI__
#define __SHADING_MODEL_HLSLI__

#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

struct DirectLighting
{
    float3 diffuse;
    float3 specular;
};

struct OcclusionResult
{
    float3 diffuse;
    float3 specular;
};

struct LightingData
{
    float3 direction;               //  ライト方向
    float shading_attenuation;      //  シェーディングによる減衰値
    float3 color;                   //  ライトカラー
    float3 shadowing_attenuation;   //  シャドウイングによる減衰値
    float3 camera_position;         //  視点
    float dummy;
};

DirectLighting integrate_bxdf(in GBufferData gbuffer_data, in LightingData lighting_data)
{
    DirectLighting result = (DirectLighting)0;
    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:
        {
            //  ライティングは行わないので何もしない
            break;
        }
        case shading_model_default_lit:
        {
            float3 albedo = gbuffer_data.base_color.rgb;
            float3 normal = normalize(gbuffer_data.w_normal);
            float3 view_vector = normalize(gbuffer_data.w_position.xyz - lighting_data.camera_position.xyz);
            float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, gbuffer_data.metalness);
            float3 fresnel0 = lerp(0.04f, albedo.rgb, gbuffer_data.metalness);
            DirectBDRF(diffuse_reflectance, fresnel0, normal, view_vector, lighting_data.direction,
                       lighting_data.color, gbuffer_data.roughness, result.diffuse, result.specular);
            result.diffuse *= lighting_data.shadowing_attenuation * lighting_data.shading_attenuation;
            result.specular *= lighting_data.shadowing_attenuation * lighting_data.shading_attenuation;

	        //	自己遮蔽
            result.diffuse = lerp(result.diffuse, result.diffuse * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            result.specular = lerp(result.specular, result.specular * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            break;
        }
//	[エンジン改造:シェーディング]
        case shading_model_ggst_lit:
        {
            //  固定値だけ残しておく
            float specular_power = 10.0f;
            float specular_size = 0.2f;

            result.diffuse  = gbuffer_data.base_color;

            break;
        }
        case shading_model_toon_lit:
        {
            float3 base_color = gbuffer_data.base_color;
            float3 shade_color = gbuffer_data.shade_color;
            float4 ilm_color = gbuffer_data.ilm_color;
            float3 normal = normalize(gbuffer_data.w_normal);
            float3 lighting_direction = -normalize(lighting_data.direction);
            if (IsGBufferFlags(gbuffer_data, material_attribute_lighting_face_adjustor))
            {
                //  ライトの方向をXZ平面に射影
                float3 face_right = UnpackNormal(gbuffer_data.unique_data.y);
                float3 face_front = UnpackNormal(gbuffer_data.unique_data.z);
                float x = dot(face_right, lighting_direction);
                float z = dot(face_front, lighting_direction);
                lighting_direction = normalize(face_right * x + face_front * z);
            }
            float3 view_vector = -normalize(gbuffer_data.w_position.xyz - lighting_data.camera_position.xyz);
            // 反射ベクトルベースでも良いが、動きが安定しやすいのでハーフベクトルにしておく
            float3 half_vector = normalize(lighting_direction + view_vector);

            //  r : 陰影判定の閾値に対するオフセット 1が標準で0だと必ず影になる
            float vertex_r  = asfloat(gbuffer_data.unique_data.x);

            float2 shading_threshold_and_offset = UnpackFloatToFloat2(gbuffer_data.unique_parameter.x);
            float shading_threshold = shading_threshold_and_offset.x;
            float shading_offset = shading_threshold_and_offset.y;

            //  各種係数算出
            float n_dot_l   = saturate(dot(normal, lighting_direction));
            float n_dot_v   = saturate(dot(normal, view_vector));
            float n_dot_h   = saturate(dot(normal, half_vector));

            //	ベースカラー算出
            //base_color  = apply_light_modifications(base_color, lighting_data.color);
            base_color *= lighting_data.color;

            //  スペキュラ算出
            float3 specular_color;
            {
                float specular_power = 10.0f;
                //  ILMのR成分を元にベースカラーを調整して元の色見に近い色を生成。
                float3  base  = lerp((float3)1.0f, min(base_color.rgb, 1.0f), 0.9f) * ilm_color.r;
                //specular_color = apply_light_modifications(base, lighting_data.color, specular_power);
                specular_color = base * lighting_data.color * specular_power;
            }

            //  計算補正
            float modified_n_dot_l = n_dot_l;
            float modified_n_dot_h = n_dot_h;
            {
                //  明/影の判定に対して、シェーディング調整値(頂点カラー/ILM)を適応。
                //  ILMのG成分をシェーディング調整係数として、
                //  0.0 = 暗
                //  0.5 = 基準
                //  1.0 = 明
                //  として計算
                float factor = (n_dot_l + shading_offset) * vertex_r * 0.5f;
                modified_n_dot_l    = saturate(factor + ilm_color.g * 2.0f - 1.0f);

                //  スペキュラの出方をILMのB成分で調整。
                //  小さいほど鋭く、大きいほど緩やかに
                //modified_n_dot_h    = blend_burn(n_dot_h, ilm_color.b);
                modified_n_dot_h    = (1.0f - (1.0f - n_dot_h) / max(ilm_color.b, 0.0001f));
            }

            //	ベースカラーとハイライト合成
            float3  base_and_specular_color = 0.0f;
            {
                float specular_size = 0.2f;
                float   factor  = (modified_n_dot_h >= saturate(1.0f - specular_size));
                base_and_specular_color = lerp(base_color.rgb, specular_color, factor);
            }

            //	シェーディング判定
            float shading_factor    = min(vertex_r >= shading_threshold, modified_n_dot_l >= shading_threshold);
            
            result.diffuse  = lerp(shade_color, base_and_specular_color, shading_factor);

            break;
        }
//	[エンジン改造:シェーディング]
    }

    return result;
}

float4 get_shading_model_color(in GBufferData gbuffer_data)
{
    float4 result = (float4) 0;
    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:       return float4(0.0f, 0.0f, 0.0f, 1);
        case shading_model_default_lit: return float4(0.5f, 0.5f, 0.5f, 1);
//	[エンジン改造:シェーディング]
        case shading_model_ggst_lit:    return float4(0.5f, 0.5f, 1.0f, 1);
        case shading_model_toon_lit:    return float4(0.5f, 1.0f, 0.5f, 1);
//	[エンジン改造:シェーディング]
    }

    return result;
}

#endif // __SHADING_MODEL_HLSLI__
