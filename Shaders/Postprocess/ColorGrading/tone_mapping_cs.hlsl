#include "tone_mapping.hlsli"

////////////////////////////////////////////////////////////////////////////////////////////////////////
Texture2D<float> exposure_value_buffer : register(t0);
RWTexture2D<float4> scene_color_map : register(u0);

static const int3 threads = int3(32, 32, 1);

////////////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(threads.x, threads.y, threads.z)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID)
{
    int2 tex_size = (int2) 0;
    scene_color_map.GetDimensions(tex_size.x, tex_size.y);
    if (dispatch_thread_id.x >= tex_size.x || dispatch_thread_id.y >= tex_size.y)
        return;
    
    float3 color = scene_color_map[dispatch_thread_id.xy].rgb;
    
    //  露出補正
    float average_luminance = 1.0f;
    if (use_exposure_compensation)
        average_luminance = exposure_value_buffer[int2(0, 0)];
    color = adjust_exposure_to_middle_gray(color, average_luminance);

    //  トーンマッピング
    switch (tone_mapping_type)
    {
        case tone_mapping_type_reinhard:
        {
            color = reinhard_tone_mapping(color);
            break;
        }
        case tone_mapping_type_aces_krzysztof_narkowicz:
        {
            color = saturate(krzysztof_narkowicz_aces_tone_mapping(color));
            break;
        }
        case tone_mapping_type_aces_stephen_hill:
        {
            color = saturate(stephen_hill_aces_tone_mapping(color));
            break;
        }
        case tone_mapping_type_gran_turismo:
        {
            color = saturate(gran_turismo_tone_mapping(color));
            break;
        }
    }

    scene_color_map[dispatch_thread_id.xy] = float4(color, 1);
}
