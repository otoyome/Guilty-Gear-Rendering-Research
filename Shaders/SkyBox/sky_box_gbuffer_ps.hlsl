#include "sky_box.hlsli"

#include "deferred_light_data.hlsli"
#include "common_gbuffer_writer.hlsli"

TextureCube specular_pmrem : register(t0);
SamplerState samplerstate : register(s0);

PS_GBUFFER_OUT main(VS_OUT pin)
{
    //  ライトプローブ
    light_probes light_probe = convert_light_probes();

    //	視線ベクトル
    float3 E = normalize(pin.world_position.xyz - camera_position.xyz);

	//	mipmap使うと崩れるのでmipmapなしになるようにしておく
    float3 base_color = specular_pmrem.SampleLevel(samplerstate, E, 0);
    base_color *= light_probe.color.rgb * light_probe.color.a;

    //  GBufferDataに出力情報をまとめる
    GBufferData data = (GBufferData)0;
    data.shading_model = shading_model_unlit;
    data.w_position = pin.world_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    encode_g_buffer_data encode_data = encode_g_buffer(data, view_projection_transform, z_buffer_parameteres);

    PS_GBUFFER_OUT pout;
    pout.base_color = encode_data.base_color;
    pout.normal_depth = encode_data.normal_depth;
    pout.parameter = encode_data.parameter;
    pout.velocity = encode_data.velocity;
    pout.unique_data = encode_data.unique_data;
    pout.shade_color = encode_data.shade_color; //  [エンジン改造:G-Buffer]
    pout.ilm_color = encode_data.ilm_color; //  [エンジン改造:G-Buffer]
    pout.scene_color = float4(base_color.rgb, 1); //  自己発光は直接シーンに書き込む
    return pout;
}
