#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"

//	色調補正情報
cbuffer CAMERA_CONSTANT_BUFFER : register(b1)
{
	float4 parameter[3];	//	x : focal_length(焦点距離)
							//	y : focus_distance(ピントの合う距離)
							//	z : circle_of_confusion(許容散乱円)
							//	w : f_number(F値)

							//	xy : 被写界深度の調整距離
							//	z : 空き
							//	w : デバッグフラグ
    
    					    //	xy : 空き
    					    //	z : ブラー最大半径
                            //  w : スケールファクター

    int4 resolutions;       //  xy : シーン解像度, zw : バッファ解像度
};

static const uint summed_area_table_thread = 1024;
static const uint3 summed_area_table_threads = uint3(summed_area_table_thread, 1, 1);
static const uint2 summed_area_table_size = uint2(1024, 1024);

static const int4 bartlett_data[9] =
{
    { -1, -1, +1, +0 },
    { +0, -1, -2, +0 },
    { +1, -1, +1, +0 },
    { -1, +0, -2, +0 },
    { +0, +0, +4, +0 },
    { +1, +0, -2, +0 },
    { -1, +1, +1, +0 },
    { +0, +1, -2, +0 },
    { +1, +1, +1, +0 },
};

#define scene_resolution (resolutions.xy)
#define buffer_resolution (resolutions.zw)
#define max_blur_radius (parameter[2].z)
#define scale_factor (parameter[2].w)

int coc_to_blur_radius(const in float coc)
{
    return clamp(abs(int(coc)), 0, max_blur_radius);
}

uint calaculate_buffer_offset(int2 location)
{
    return buffer_resolution.x * location.y + location.x;
}
