//  VS_OUT
//  surface関数(α値のみ出力)
//  を作る事

#include "gbuffer.hlsli"

void main(VS_OUT pin, bool is_front_face : SV_IsFrontFace)
{
    float alpha = surface(pin, is_front_face);

    //	マスク処理
    if (material_blend_mode == material_blend_mode_mask)
    {
        float alpha_threshold = material_alpha_clip_threshold;
        alpha_threshold = lerp(alpha_threshold, 1.0f, calculate_alpha_dithering(pin.position.xy));
	    clip(alpha - alpha_threshold);
	}
}

