#include "depth_of_field.hlsli"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RWStructuredBuffer<int4> destination_color_buffer : register(u0);

Texture2D<float4> source_color_map : register(t0);
Texture2D<float> scene_depth_stencil : register(t1);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float convert_depth(uint3 position)
{
    float depth = scene_depth_stencil.Load(int3(position.xy, 0)).r;
    float2 t = (position.xy / (float2) scene_resolution.xy) * 2.0f - 1.0f;
    float4 p = mul(float4(t.x, -t.y, depth, 1), inverse_projection_transform);
    depth = p.z / p.w;
    depth = convert_unit_to_mm(depth);
    return depth;
}

//	許容散乱円のサイズを算出
float calculate_circle_of_confusion(float focus_length)
{
    float focal_length = parameter[0].x;
    float focus_distance = parameter[0].y;
    float circle_of_confusion_limit = parameter[0].z;
    float f_number = parameter[0].w;
    float2 adjust_length = parameter[1].xy;

    const float distance_to_lense = focus_length - focal_length;
    const float distance_to_focus_plane = distance_to_lense - focus_distance;

	float r = (focus_length - focus_distance) / focus_length;
    float l = (focal_length * focal_length) / (f_number * (focus_distance - focal_length));
    float coc = r * l / circle_of_confusion_limit;

    return clamp(coc, -max_blur_radius, max_blur_radius);
}

//  整数バッファに色を格納
void interlocked_add_to_buffer(int2 location, int4 color)
{
    const uint offset = calaculate_buffer_offset(location);
    InterlockedAdd(destination_color_buffer[offset].r, color.r);
    InterlockedAdd(destination_color_buffer[offset].g, color.g);
    InterlockedAdd(destination_color_buffer[offset].b, color.b);
    InterlockedAdd(destination_color_buffer[offset].a, color.a);
}

//  整数バッファへ格納するために整数型に変換
int4 normalized_color(float4 color, float coc)
{
    coc += 1.0f;
    float weight = coc * coc * coc * coc;
    float normalization = scale_factor / weight;
    return int4(round(compress_rgba_to_log(color) * normalization));
}

void write_delta_bartlett(float4 color, int blur_radius, int2 location)
{
    int4 int_color = normalized_color(color, blur_radius);
    [loop] for (int index = 0; index < 9; ++index)
    {
        int2 delta = bartlett_data[index].xy * (blur_radius + 1);
        int delta_value = bartlett_data[index].z;

        //  画像中央に配置されないので位置をオフセット
        int2 buffer_location = location.xy + delta + max_blur_radius + int2(1, 1);

        // 差分書き込み
        interlocked_add_to_buffer(buffer_location, int_color * delta_value);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(8, 8, 1)]
void main(uint3 position : SV_DispatchThreadID)
{
    uint2 texture_size;
    scene_depth_stencil.GetDimensions(texture_size.x, texture_size.y);
    if ((position.x >= texture_size.x) || (position.y >= texture_size.y))
        return;

	//	ビュー空間深度
    float depth = convert_depth(position);

	//	CoC値算出
	float coc = calculate_circle_of_confusion(depth);

    //  シーンカラー取得
   	float4 color = source_color_map.Load(int3(position.xy, 0));

    //  デバッグ用に色加算
    if (abs(coc) > 1.0f && parameter[1].w > 0.5f)
    {
        float focus_distance = parameter[0].y;
        color.rgb *= (coc < 0) ? float3(1, 0, 0) : float3(0, 0, 1);
    }
    
    //  ブラー半径算出
    int blur_radius = coc_to_blur_radius(coc);

    //  中央値の差分を書き込み
    write_delta_bartlett(color, blur_radius, position.xy);
}

