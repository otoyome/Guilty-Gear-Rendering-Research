#include "fullscreen_quad.hlsli"
#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"

Texture2D<float4> scene_color_map : register(t0);
SamplerState linear_sampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 pixel_size;
    scene_color_map.GetDimensions(pixel_size.x, pixel_size.y);

    float4 color = (float4)1;
    color.rgb = fxaa(scene_color_map, linear_sampler, pin.texcoord, rcp(pixel_size));
    return color;
}
