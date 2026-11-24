#ifndef __GLTF_SURFACE_HLSLI__
#define __GLTF_SURFACE_HLSLI__

#include "gltf_rendering.hlsli"
#include "gbuffer.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;

    //  ベースカラー
    data.base_color = base_color_factor;
    if (HAS_RESOURCE(BaseColorMap))
    {
        data.base_color *= TEXTURE2D_SAMPLE(BaseColorMap, linear_sampler_state, pin.texcoord[BaseTexcoord]);
    }

    //	自己発光色を取得
    data.emissive_color = emissive_factor;
    if (HAS_RESOURCE(EmissiveMap))
    {
        data.emissive_color *= TEXTURE2D_SAMPLE(EmissiveMap, linear_sampler_state, pin.texcoord[BaseTexcoord]).rgb;
    }
    else
    {
        data.emissive_color *= data.base_color;
    }

    //	法線/従法線/接線
    float3 N = normalize(pin.w_normal.xyz);
    float3 T = has_tangent ? normalize(pin.w_tangent.xyz) : float3(1.0f, 0.0f, 0.0f);
    float sigma = has_tangent ? pin.w_tangent.w : 1.0f; //  sigmaはUVの方向による係数
    T = normalize(T - N * dot(N, T)); //  N と正確に直交するように調整
    float3 B = normalize(cross(N, T) * sigma);
    if (!is_front_face)
    {
        T = -T;
        B = -B;
        N = -N;
    }
    //	法線マッピング
    if (HAS_RESOURCE(NormalMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(NormalMap, linear_sampler_state, pin.texcoord[BaseTexcoord]);
        float3 normal_factor = (sampled.xyz * 2.0f) - 1.0f;
        normal_factor = normalize(normal_factor * float3(normal_map_scale, normal_map_scale, 1.0f));
        N = normalize((normal_factor.x * T) + (normal_factor.y * B) + (normal_factor.z * N));
    }
    data.w_normal = N.xyz;

	//	金属質/粗さを取得
    data.roughness = saturate(roughness_factor);
    data.metalness = saturate(metallic_factor);
    if (HAS_RESOURCE(RoughnessMetallicMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(RoughnessMetallicMap, linear_sampler_state, pin.texcoord[BaseTexcoord]);
        data.roughness *= sampled.g;
        data.metalness *= sampled.b;
    }

	//	光の遮蔽値を取得
    data.occlusion_factor = 1.0f;
    if (HAS_RESOURCE(OcclusionMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(OcclusionMap, linear_sampler_state, pin.texcoord[BaseTexcoord]);
        data.occlusion_factor *= sampled.r;
    }
    data.occlusion_strength = occlusion_strength;

    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.shading_model = material_shading_model;
    data.stencil = material_flags;
    data.unique_data = 0;
    data.unique_parameter = 0;
    return data;
}

#endif	//	__GLTF_SURFACE_HLSLI__
