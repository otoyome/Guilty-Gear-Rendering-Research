#include "filter_functions.hlsli"

cbuffer TONE_MAPPING_CONSTANT_BUFFER : register(b0)
{
    //  露出補正処理
    float4 exposure_compensation_data; //  x : 使用フラグ, y : 適応値, zw : 適応輝度範囲

    //  トーンマッピング
    float4 tone_mapping_variable[2]; //  0.xは種類として利用、以下は場合によりけり
};

#define use_exposure_compensation (exposure_compensation_data.x > 0.5f ? true : false)
#define exposure_adapt_rate exposure_compensation_data.y
#define exposure_luminance_range float2(exposure_compensation_data.zw)

#define tone_mapping_type (int)(tone_mapping_variable[0].x + 0.1f)
static const int tone_mapping_type_none = 0;
static const int tone_mapping_type_reinhard = 1;
static const int tone_mapping_type_aces_krzysztof_narkowicz = 2;
static const int tone_mapping_type_aces_stephen_hill = 3;
static const int tone_mapping_type_gran_turismo = 4;

///////////////////////////////////////////////////////////////////////////
//  reinhard
float3 reinhard_tone_mapping(float3 x)
{
    return saturate(x / (1 + x));
}

///////////////////////////////////////////////////////////////////////////
//  本家ACESは↓
//  https://github.com/aces-aswf/aces-core/tree/dev

//  近似ACES(Krzysztof Narkowicz)
//  https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 krzysztof_narkowicz_aces_tone_mapping(float3 x)
{
	float a = tone_mapping_variable[0].y;
	float b = tone_mapping_variable[0].z;
	float c = tone_mapping_variable[0].w;
	float d = tone_mapping_variable[1].x;
	float e = tone_mapping_variable[1].y;
    float3 A = x * (a * x + b);
    float3 B = x * (c * x + d) + e;
    return saturate(A / B);
}

//  近似ACES(Stephen Hill)
//  https://scrapbox.io/0b5vr/ACES_Filmic_Tonemapping
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367 },
    { -0.10208, 1.10813, -0.00605 },
    { -0.00327, -0.07276, 1.07602 }
};

float3 RRTAndODTFit(float3 v)
{
	float a = tone_mapping_variable[0].y;
	float b = tone_mapping_variable[0].z;
	float c = tone_mapping_variable[0].w;
	float d = tone_mapping_variable[1].x;
	float e = tone_mapping_variable[1].y;
    float3 A = v * (a + v) - b;
    float3 B = v * (c * v + d) + e;
    return A / B;
}

float3 stephen_hill_aces_tone_mapping(float3 color)
{
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

/////////////////////////////////////////////////////////////////////////////////////////////
//  GranTurismo Tone Mapping
//	http://cdn2.gran-turismo.com/data/www/pdi_publications/PracticalHDRandWCGinGTS_20181222.pdf
//	https://www.desmos.com/calculator/mbkwnuihbd?lang=ja
//      
float W_f(float x, float e0, float e1)
{
	float a = (x - e0) / (e1 - e0);
	return (x <= e0) ? 0.0f : (x >= e1) ? 1.0f : a * a * (3.0f - 2.0f * a);
}

float H_f(float x, float e0, float e1)
{
	return (x <= e0) ? 0.0f : (x >= e1) ? 1.0f : (x - e0) / (e1 - e0);
}

float3 gran_turismo_tone_mapping(float3 x)
{
    float P = tone_mapping_variable[0].y; //	モニター最大輝度
    float a = tone_mapping_variable[0].z; //	コントラスト
    float m = tone_mapping_variable[0].w; //	線形区間の始まり
    float l = tone_mapping_variable[1].x; //	線形区間の長さ
    float c = tone_mapping_variable[1].y; //	黒の締まり(トー部分の強さ)
    float b = tone_mapping_variable[1].z; //	黒の締まり(トーのオフセット)
    static const float e = 2.71828f;

	//	リニア領域の計算
    float l0 = (P - m) * l / a; //	スケール後のリニアの長さ
    float L0 = m - m / a;
    float L1 = m + (1 - m) / a;
    float3 L_x = m + a * (x - m);

	//	トー
    float3 T_x = m * pow(x / m, c) + b;

	//	ショルダー
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = a * P / (P - S1);

	//	全部まとめる
	//	w0はトーの重み
	//	w1はリニアの重み
	//	w2はショルダーの重み
    float3 S_x = P - (P - S1) * pow(e, -(C2 * (x - S0) / P));
    float3 w0_x = 1 - W_f(x, 0.0f, m);
    float3 w2_x = H_f(x, m + l0, m + l0);
    float3 w1_x = 1 - w0_x - w2_x;
    float3 f_x = T_x * w0_x + L_x * w1_x + S_x * w2_x;
    return f_x;
}