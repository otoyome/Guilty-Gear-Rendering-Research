//  VS_OUT
//  surface関数
//  を作る事

#include "common_gbuffer_writer.hlsli"

PS_GBUFFER_OUT main(VS_OUT pin, bool is_front_face : SV_IsFrontFace)
{
    SurfaceData surface_data = surface(pin, is_front_face);

    //	マスク処理
    if (material_blend_mode == material_blend_mode_mask)
    {
        float alpha_threshold = material_alpha_clip_threshold;
        alpha_threshold = lerp(alpha_threshold, 1.0f, calculate_alpha_dithering(pin.position.xy));
        clip(surface_data.base_color.a - alpha_threshold);
    }

    //  GBufferDataに出力情報をまとめる
    GBufferData g_buffer_data = convert_g_buffer_data(surface_data);
    encode_g_buffer_data encode_data = encode_g_buffer(g_buffer_data, view_projection_transform, z_buffer_parameteres);
    
    PS_GBUFFER_OUT pout;
    pout.base_color = encode_data.base_color;
    pout.normal_depth = encode_data.normal_depth;
    pout.parameter = encode_data.parameter;
    pout.velocity = encode_data.velocity;
    pout.unique_data = encode_data.unique_data;
    pout.shade_color = encode_data.shade_color; //  [エンジン改造:G-Buffer]
    pout.ilm_color = encode_data.ilm_color; //  [エンジン改造:G-Buffer]
    pout.scene_color = float4(surface_data.emissive_color, 1);  //  自己発光は直接シーンに書き込む
    return pout;
}

