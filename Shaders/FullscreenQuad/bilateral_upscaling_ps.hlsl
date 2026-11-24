#include "fullscreen_quad.hlsli"
#include "shading_functions.hlsli"
#include "scene_constant_buffer.hlsli"


Texture2D color_map : register(t0);
Texture2D low_resolution_normal_depth_map : register(t1);
Texture2D high_resolution_normal_depth_map : register(t2);

float4 main(VS_OUT pin) : SV_TARGET
{
    //  各種テクスチャサイズ取得
    uint2 low_resolution_texture_size, hifh_resolution_texture_size;
    low_resolution_normal_depth_map.GetDimensions(low_resolution_texture_size.x, low_resolution_texture_size.y);
    high_resolution_normal_depth_map.GetDimensions(hifh_resolution_texture_size.x, hifh_resolution_texture_size.y);

    float4 color = 0.0f;
    {
        //  低解像度テクスチャのピクセル座標を算出
        int2 low_resolution_texposition = int2(pin.texcoord.xy * low_resolution_texture_size.xy + 0.1f);
        float4 low_resolution_center_color = color_map.Load(int3(low_resolution_texposition, 0));

        //  高解像度ピクセルのインデックスを求める
        float2 high_resolution_uv = (floor(pin.texcoord * hifh_resolution_texture_size.xy) + 0.1f);
        int2 high_resolution_texposition = int2(floor(high_resolution_uv * hifh_resolution_texture_size.xy));
        int high_resolution_index = (1 - (high_resolution_texposition.y & 0x01)) * 2
                                  + (1 - (high_resolution_texposition.x & 0x01));

        //  高解像度ピクセルの基準位置を取得
        float4 high_resolution_normal_depth;
        high_resolution_normal_depth = high_resolution_normal_depth_map.Load(int3(high_resolution_texposition, 0));

        //  低解像度から4ピクセルの法線・深度・色情報を求める
        float4 low_resolution_normal_depth[4];
        float4 low_resolution_color[4];
        switch (high_resolution_index)
        {
            case 0:
            {
                static const int2 offsets[4] =
                {
                    int2(0, 0),
                    int2(1, 0),
                    int2(0, 1),
                    int2(1, 1),
                };
                low_resolution_normal_depth[0] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[0]);
                low_resolution_normal_depth[1] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[1]);
                low_resolution_normal_depth[2] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[2]);
                low_resolution_normal_depth[3] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[3]);

                low_resolution_color[0] = low_resolution_center_color;
                low_resolution_color[1] = color_map.Load(int3(low_resolution_texposition, 0), offsets[1]);
                low_resolution_color[2] = color_map.Load(int3(low_resolution_texposition, 0), offsets[2]);
                low_resolution_color[3] = color_map.Load(int3(low_resolution_texposition, 0), offsets[3]);
                break;
            }
            case 1:
            {
                static const int2 offsets[4] =
                {
                    int2(-1, 0),
                    int2(0, 0),
                    int2(-1, 1),
                    int2(0, 1),
                };
                low_resolution_normal_depth[0] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[0]);
                low_resolution_normal_depth[1] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[1]);
                low_resolution_normal_depth[2] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[2]);
                low_resolution_normal_depth[3] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[3]);
                low_resolution_color[0] = color_map.Load(int3(low_resolution_texposition, 0), offsets[0]);
                low_resolution_color[1] = low_resolution_center_color;
                low_resolution_color[2] = color_map.Load(int3(low_resolution_texposition, 0), offsets[2]);
                low_resolution_color[3] = color_map.Load(int3(low_resolution_texposition, 0), offsets[3]);
                break;
            }
            case 2:
            {
                static const int2 offsets[4] =
                {
                    int2(0, -1),
                    int2(1, -1),
                    int2(0, 0),
                    int2(1, 0),
                };
                low_resolution_normal_depth[0] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[0]);
                low_resolution_normal_depth[1] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[1]);
                low_resolution_normal_depth[2] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[2]);
                low_resolution_normal_depth[3] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[3]);
                low_resolution_color[0] = color_map.Load(int3(low_resolution_texposition, 0), offsets[0]);
                low_resolution_color[1] = color_map.Load(int3(low_resolution_texposition, 0), offsets[1]);
                low_resolution_color[2] = low_resolution_center_color;
                low_resolution_color[3] = color_map.Load(int3(low_resolution_texposition, 0), offsets[3]);
                break;
            }
            case 3:
            {
                static const int2 offsets[4] =
                {
                    int2(-1, -1),
                    int2(0, -1),
                    int2(-1, 0),
                    int2(0, 0),
                };
                low_resolution_normal_depth[0] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[0]);
                low_resolution_normal_depth[1] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[1]);
                low_resolution_normal_depth[2] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[2]);
                low_resolution_normal_depth[3] = low_resolution_normal_depth_map.Load(int3(low_resolution_texposition, 0), offsets[3]);
                low_resolution_color[0] = color_map.Load(int3(low_resolution_texposition, 0), offsets[0]);
                low_resolution_color[1] = color_map.Load(int3(low_resolution_texposition, 0), offsets[1]);
                low_resolution_color[2] = color_map.Load(int3(low_resolution_texposition, 0), offsets[2]);
                low_resolution_color[3] = low_resolution_center_color;
                break;
            }
        }

        //  カラーブレンド
        static const float4 bilinear_weights[4] =
        {
            float4(9.0f / 16.0f, 3.0f / 16.0f, 3.0f / 16.0f, 1.0f / 16.0f),
            float4(3.0f / 16.0f, 9.0f / 16.0f, 1.0f / 16.0f, 3.0f / 16.0f),
            float4(3.0f / 16.0f, 1.0f / 16.0f, 9.0f / 16.0f, 3.0f / 16.0f),
            float4(1.0f / 16.0f, 3.0f / 16.0f, 3.0f / 16.0f, 9.0f / 16.0f)
        };
        float total_weight = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            //  法線のウェイトを求める
            float normal_weight = dot(low_resolution_normal_depth[i].xyz, high_resolution_normal_depth.xyz);
            normal_weight = pow(saturate(normal_weight), 32.0f);
 
            //  深度のウェイトを求める
            float depth_diff = high_resolution_normal_depth.w - low_resolution_normal_depth[i].w;
            float depth_weight = 1.0f / (1.0f + abs(depth_diff));

            float weight = normal_weight * depth_weight * bilinear_weights[high_resolution_index][i];
            total_weight += weight;
            color += low_resolution_color[i] * weight;
        }
        color = total_weight <= 0.0001f ? low_resolution_center_color : color / total_weight;
    }
    return color;
}

