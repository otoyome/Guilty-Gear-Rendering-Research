#ifndef __FILTER_FUNCTIONS_HLSLI__
#define __FILTER_FUNCTIONS_HLSLI__

#include "common.hlsli"

//--------------------------------------------
//	RGB色空間の数値からHSV色空間の数値への変換関数
//--------------------------------------------
//rgb:RGB色空間の数値
float3 convert_rgb_to_hsv( float3 rgb )
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = (rgb.g < rgb.b) ? float4(rgb.bg, K.wz) : float4(rgb.gb, K.xy);
    float4 q = (rgb.r < p.x) ? float4(p.xyw, rgb.r) : float4(rgb.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float eps = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + eps)), d / (q.x + eps), q.x);
}

//--------------------------------------------
//	HSV色空間の数値からRGB色空間の数値への変換関数
//--------------------------------------------
//hsv:HSV色空間の数値
float3	convert_hsv_to_rgb( float3 hsv )
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), clamp(hsv.y, 0.0, 1.0));
}

//--------------------------------------------
//	RGB色から輝度への変換関数
//--------------------------------------------
float convert_rgb_to_luminance(float3 rgb)
{
    static const float3 luminanceValue = float3(0.299f, 0.587f, 0.114f);
    return dot(luminanceValue, rgb);
}

//--------------------------------------------
//	RGB色を対数圧縮関数
//--------------------------------------------
float3 compress_rgb_to_log(float3 color)
{
	return log2(color.rgb + 1.0f);
}
float4 compress_rgba_to_log(float4 color)
{
	return log2(color + 1.0f);
}

//--------------------------------------------
//	RGB色を対数圧縮関数
//--------------------------------------------
float3 decompress_rgb_to_log(float3 color)
{
	return exp2(color.rgb) - 1.0f;
}

float4 decompress_rgba_to_log(float4 color)
{
	return exp2(color) - 1.0f;
}

//--------------------------------------------
//	覆い焼きブレンド
//--------------------------------------------
#define	blend_burn(A, B)					(1.0f - (1.0f - (A)) / (B))
#define	blend_burn_factor(A, B, Factor)		((1.0f - Factor) * (A) + Factor * blend_burn((A), (B)))

//--------------------------------------------
//	スクリーンブレンド
//--------------------------------------------
#define	blend_screen(A, B)					(1.0f - (1.0f - (A)) * (1.0f - B))
#define	blend_screen_factor(A, B, Factor)	((1.0f - Factor) * (A) + Factor * blend_screen((A), (B)))

//--------------------------------------------
//	比較（暗）ブレンド
//--------------------------------------------
#define	blend_darken(A, B)					(min(A, B))
#define	blend_darken_factor(A, B, Factor)	((1.0f - Factor) * (A) + Factor * blend_darken((A), (B)))

//--------------------------------------------
//	シーンの平均輝度を中間グレーとして色を正規化する
//--------------------------------------------
float3 adjust_exposure_to_middle_gray(float3 color, float average_luminance)
{
    static const float mid_gray = 0.18f;
    average_luminance = mid_gray / (average_luminance * (1.0f - mid_gray));
    return color * average_luminance;
}

//--------------------------------------------
//	BicubicSamplingCatmullRom
//	http://vec3.ca/bicubic-filtering-in-fewer-taps/ (なんか消えてたので、ShaderToy側を下記に)
//	https://www.shadertoy.com/view/styXDh
//--------------------------------------------
float4 bicubic_sample_catmull_rom(Texture2D tex, SamplerState samp, float2 texcoord, float2 texture_size)
{
    float2 coord = texcoord * texture_size;

    float2 tc = floor(coord - 0.5f) + 0.5f;
    float2 f = coord - tc;

    float2 f2 = f * f;
    float2 f3 = f2 * f;

    float2 w0 = f2 - 0.5f * (f3 + f);
    float2 w1 = 1.5f * f3 - 2.5f * f2 + 1.0f;
    float2 w3 = 0.5f * (f3 - f2);
    float2 w2 = 1.0f - w0 - w1 - w3;

    float2 w12 = w1 + w2;

    float2 inverse_texture_size = rcp(texture_size);
    float2 tc0 = (tc - 1.0f) * inverse_texture_size;
    float2 tc12 = (tc + w2 / w12) * inverse_texture_size;
    float2 tc3 = (tc + 2.0f) * inverse_texture_size;

    float4 color = tex.Sample(samp, float2(tc0.x, tc0.y)) * (w0.x * w0.y)
			 	 + tex.Sample(samp, float2(tc0.x, tc12.y)) * (w0.x * w12.y)
			 	 + tex.Sample(samp, float2(tc0.x, tc3.y)) * (w0.x * w3.y)
			 	 + tex.Sample(samp, float2(tc12.x, tc0.y)) * (w12.x * w0.y)
			 	 + tex.Sample(samp, float2(tc12.x, tc12.y)) * (w12.x * w12.y)
			 	 + tex.Sample(samp, float2(tc12.x, tc3.y)) * (w12.x * w3.y)
			 	 + tex.Sample(samp, float2(tc3.x, tc0.y)) * (w3.x * w0.y)
			 	 + tex.Sample(samp, float2(tc3.x, tc12.y)) * (w3.x * w12.y)
				 + tex.Sample(samp, float2(tc3.x, tc3.y)) * (w3.x * w3.y);
    return max(0.0f, color);
}

//--------------------------------------------
//	Fast Approximate Anti-Aliasing
//	https://www.geeks3d.com/20110405/fxaa-fast-approximate-anti-aliasing-demo-glsl-opengl-test-radeon-geforce/
//--------------------------------------------
#define FXAA_REDUCE_MIN (1.0f / 128.0f)
#define FXAA_REDUCE_MUL (1.0f / 8.0f)
#define FXAA_SPAN_MAX 8.0f
#define FXAA_SUBPIX_SHIFT (1.0f / 4.0f)
float3 fxaa(Texture2D color_texture, SamplerState samp, float2 texcoord, float2 pixel_size)
{
    //  周辺ピクセルを参照
    float2 texcoord2 = texcoord.xy - (pixel_size * (0.5 + FXAA_SUBPIX_SHIFT));
    float3 sampled_nw = color_texture.Sample(samp, texcoord2.xy).xyz;
    float3 sampled_ne = color_texture.Sample(samp, texcoord2.xy, float2(1, 0)).xyz;
    float3 sampled_sw = color_texture.Sample(samp, texcoord2.xy, float2(0, 1)).xyz;
    float3 sampled_se = color_texture.Sample(samp, texcoord2.xy, float2(1, 1)).xyz;
    float3 sampled_m = color_texture.Sample(samp, texcoord.xy).xyz;

    //  それぞれの輝度を算出
    float luma_nw = convert_rgb_to_luminance(sampled_nw);
    float luma_ne = convert_rgb_to_luminance(sampled_ne);
    float luma_sw = convert_rgb_to_luminance(sampled_sw);
    float luma_se = convert_rgb_to_luminance(sampled_se);
    float luma_m = convert_rgb_to_luminance(sampled_m);

    //  輝度差の最小最大を取得
    float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));

    //  輝度差から垂直な向きベクトルを算出
    float2 dir;
    dir.x = -((luma_nw + luma_ne) - (luma_sw + luma_se));
    dir.y = +((luma_nw + luma_sw) - (luma_ne + luma_se));

    //  複数ピクセルを横断するようにエッジを伸ばす
    float dir_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) * (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcp_dir_min = 1.0f / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX), max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcp_dir_min)) * pixel_size;

    //  向きベクトルを元に複数位置の色をブレンドして返す
    float3 rgb_a = (1.0f / 2.0f)
                 * (color_texture.Sample(samp, texcoord.xy + dir * (1.0f / 3.0f - 0.5f)).xyz +
                    color_texture.Sample(samp, texcoord.xy + dir * (2.0f / 3.0f - 0.5f)).xyz);
    float3 rgb_b = rgb_a * (1.0f / 2.0f)
                 + (1.0f / 4.0f) * (color_texture.Sample(samp, texcoord.xy + dir * (0.0f / 3.0f - 0.5f)).xyz +
		                            color_texture.Sample(samp, texcoord.xy + dir * (3.0f / 3.0f - 0.5f)).xyz);
    float luma_b = convert_rgb_to_luminance(rgb_b);
    return ((luma_b < luma_min) || (luma_b > luma_max)) ? rgb_a : rgb_b;
}

#endif	//	__FILTER_FUNCTIONS_HLSLI__
