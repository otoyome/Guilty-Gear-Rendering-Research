#ifndef __LIGHTS_HLSLI__
#define __LIGHTS_HLSLI__

// 平行光源
struct directional_lights
{
    float4 direction;
    float4 color;
};

// 点光源
struct point_lights
{
	float4	position;
	float4	color;
	float	range;
	float3	dummy;
};

// スポットライト
struct spot_lights
{
	float4	position;
	float4	direction;
	float4	color;
	float	range;
	float	innerCorn;
	float	outerCorn;
	float	dummy;
};

// ライトプローブ
struct light_probes
{
	float4 position;
	float4 color;
	float4 inner_area;
	float4 outer_area;
};

//  ライト種別ID
static const int light_kind_directional_light = 1; //  平行光源
static const int light_kind_point_light = 2; //  点光源
static const int light_kind_spot_light = 3; //  スポットライト

//  ライト系のマスク値
static const uint light_flag_active = 1 << 0;
static const uint light_flag_use_shadow = 1 << 1;

static const uint light_flag_debug_csm_area_flag = 1 << 20;
static const uint light_flag_debug_visualize_ailiasing_error_flag = 1 << 21;

//  纏めたライト情報
struct compund_lights
{
    //  共有の情報
    //  work_data[3].w は共有でライトの種別番号を入れておく

    //  平行光源の場合
    //  work_data[0] = xyz = direction, w = dummy
    //  work_data[1] = color
    //  work_data[2] = dummy
    //  work_data[3] = xyz = dummy, w = ライト種別

    //  点光源の場合
    //  work_data[0] = xyz = position, w = dummy
    //  work_data[1] = color
    //  work_data[2] = x = range, yzw = dummy
    //  work_data[3] = xyz = dummy, w = ライト種別

    //  スポットライトの場合
    //  work_data[0] = xyz = position, w = dummy
    //  work_data[1] = xyz = direction, w = dummy
    //  work_data[2] = color
    //  work_data[3] = x = range, y = inner cone, z = outer cone, w = ライト種別

	//	プローブの場合
	//  work_data[0] = xyz = position, w = dummy
	//  work_data[1] = color
	//  work_data[2] = xyz = inner area, w = dummy
	//  work_data[3] = xyz = outer area, w = ライト種別
    float4 work_data[4];
};

#endif	//	__LIGHTS_HLSLI__
