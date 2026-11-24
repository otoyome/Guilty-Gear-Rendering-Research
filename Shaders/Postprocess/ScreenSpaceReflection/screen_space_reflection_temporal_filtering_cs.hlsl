#include "screen_space_reflection.hlsli"

#include "screen_space_reflection_denoise.hlsli"

#define	THREAD_X	(16)
#define	THREAD_Y	(16)

Texture2D<float4> source_color_map : register(t0);
Texture2D<float4> source_accumlation_color_map : register(t1);
Texture2D<float2> velocity_map : register(t2);
Texture2D<float> source_counter_map : register(t3);

RWTexture2D<float4> destination_color_map : register(u0);
RWTexture2D<float4> destination_accumlation_color_map : register(u1);
RWTexture2D<float> destination_counter_map : register(u2);

[numthreads(THREAD_X, THREAD_Y, 1)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID, uint group_index : SV_GROUPINDEX)
{
    int3 center_texture_position = int3(dispatch_thread_id.xy, 0);
    float4 source_color = source_color_map.Load(center_texture_position);
    float4 accumlation_color = source_accumlation_color_map.Load(center_texture_position);
    float counter_color = source_counter_map.Load(center_texture_position);

    //  ‘¬“xŒW”¶¬
    float2 velocity = velocity_map.Load(center_texture_position).xy;
    float velocity_factor = saturate(smoothstep(0.0f, denoise_velocity_max + 0.01f, length(velocity * texture_size.xy)));

    //  ƒuƒŒƒ“ƒhŒW”’²®
    float blend_factor = denoise_temporal_blend_factor;
    {
        //  “®‚¢‚Ä‚¢‚È‚¢ê‡‚Í’~ÏB
        counter_color = (!reset_flag && velocity_factor <= 1.0f - 0.001f) ? counter_color + 1 : 0;

        //  ’~Ï”‚©‚çƒuƒŒƒ“ƒhŒW”‚ğZo
        blend_factor = lerp(blend_factor, 0.95f, saturate(smoothstep(0.0f, denoise_counter_max, counter_color)));

        //  ˆÚ“®’l‚ª‘å‚«‚¢ê‡‚ÍƒuƒŒƒ“ƒh‚ğ”jŠü‚·‚é‚æ‚¤‚É’²®
        blend_factor = saturate(blend_factor - velocity_factor);
    }

    //  FŠi”[
    float4 destination_color = lerp(source_color, accumlation_color, saturate(blend_factor - reset_flag));
    destination_color = max(0.0f, destination_color);
    destination_color_map[dispatch_thread_id.xy] = destination_color;
    destination_accumlation_color_map[dispatch_thread_id.xy] = destination_color;
    destination_counter_map[dispatch_thread_id.xy] = counter_color;
}
