#ifndef __TOON_SURFACE_HLSLI__
#define __TOON_SURFACE_HLSLI__

#include "toon_rendering.hlsli"
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;
    
    bool is_outline_pixel = pin.w_normal.w > 0.5f;
    
    data.base_color = float4(0, 0, 0, 1);
    data.shade_color = 0;
    data.emissive_color = 0;
    data.roughness = 0.8f;
    data.metalness = 0;
    data.occlusion_factor = 0;
    data.occlusion_strength = 0;
    data.unique_parameter = 0;
    data.w_normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.stencil = material_flags;

    //  ベースカラー
    data.base_color = material_base_color;
    if (HAS_RESOURCE(BaseMap))
    {
        data.base_color *= TEXTURE2D_SAMPLE(BaseMap, linear_sampler_state, pin.texcoord[BaseTexcoord]);
    }

    if (is_outline_pixel)
    {
        data.base_color.rgb = float3(0, 0, 0);
        data.roughness = 1.0f;
    }
    else
    {
        //  SSSカラー
        //  RGB : 陰色
        //  A : 空き
        data.shade_color = material_sss_color.rgb;
        if (HAS_RESOURCE(SssMap))
        {
            data.shade_color *= TEXTURE2D_SAMPLE(SssMap, linear_sampler_state, pin.texcoord[SssTexcoord]);
        }
        //  環境光の色は陰部にのみ適応
        //data.shade_color.rgb = apply_light_modifications(data.shade_color.rgb, fake_ambient_color.rgb);
        data.shade_color.rgb *= fake_ambient_color.rgb;
        
        //  ILMカラー
        //  R : ハイライト強度
        //  G : テクスチャベースでの焼き込み影
        //  B : ハイライトの入りやすさ
        //  A : 
        data.ilm_color = float4(0, 1, 0, 0);
        if (HAS_RESOURCE(IlmMap))
        {
            data.ilm_color = TEXTURE2D_SAMPLE(IlmMap, linear_sampler_state, pin.texcoord[IlmTexcoord]);
        }

        data.roughness = material_parameters.x;
        data.metalness = material_parameters.y;

        //  顔の場合はライティングのY軸を無視したいので調整用に顔の姿勢軸を入れる
        float3 face_front = float3(0, 0, 1), face_right = float3(1, 0, 0);
        if (face_material_flag)
        {
            //  本当は顔の方向を設定するので対応するボーン番号を入れる事
            face_right = normalize(bone_transforms[0]._11_12_13);
            face_front = normalize(bone_transforms[0]._31_32_33);
            //  顔専用ライティングを行うかどうか
            SetGBufferFlags(data.stencil, material_attribute_lighting_face_adjustor, face_material_flag);
        }

        //  詳細情報
        if (HAS_RESOURCE(DetailMap))
        {
            float4 detail_color = TEXTURE2D_SAMPLE(DetailMap, linear_sampler_state, pin.texcoord[DetailTexcoord]);
            data.base_color.rgb *= detail_color.rgb;
            data.shade_color.rgb *= detail_color.rgb;
        }

        //  シェーディングモデル側にパラメーター引き渡し
        float2 shading_threshold_and_offset;
        shading_threshold_and_offset.x = saturate(material_parameters.z);
        shading_threshold_and_offset.y = saturate(material_parameters.w);
        data.unique_parameter.x = PackFloat2ToFloat(shading_threshold_and_offset);
        data.unique_parameter.y = 0;
        data.unique_data.x = asuint(pin.color[0].r); // 赤色がシェーディング補正値なので格納
        data.unique_data.y = PackNormal(face_right);
        data.unique_data.z = PackNormal(face_front);
    }

    data.shading_model = is_outline_pixel ? shading_model_unlit : material_shading_model;
    return data;
}

#endif	//	__TOON_SURFACE_HLSLI__
