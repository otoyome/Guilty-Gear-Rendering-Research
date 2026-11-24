#ifndef __TONE_MAPPING_CONSTANT_BUFFER_HLSLI__
#define __TONE_MAPPING_CONSTANT_BUFFER_HLSLI__

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


#endif // __TONE_MAPPING_CONSTANT_BUFFER_HLSLI__
