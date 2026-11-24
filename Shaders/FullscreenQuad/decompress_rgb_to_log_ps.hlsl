#include "fullscreen_quad.hlsli"


Texture2D<float4> color_map : register(t0);
SamplerState sampler_states : register(s0);


float4 main(PS_IN pin) : SV_TARGET
{
    float4 color = color_map.Sample(sampler_states, pin.texcoord);
    color.rgb = decompress_rgb_to_log(color.rgb);
    return color;
}
