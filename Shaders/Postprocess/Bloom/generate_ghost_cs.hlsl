#include "filter_functions.hlsli"

//  https://john-chapman-graphics.blogspot.com/2013/02/pseudo-lens-flare.html

cbuffer LENSGHOST_CONSTANT_BUFFER : register(b0)
{
    float dispersion_scale;
    float dispersion_offset;
	float ghost_mask_radius;
    float ghost_mask_fade;

    uint iteration;
    uint ghost_color_offset;
	float ghost_size;
	float dummy;
};

RWTexture2D<float3> lens_ghost_texture : register(u0);
Texture2D<float3> luminance_texture : register(t0);
Texture2D<float3> ghost_color_lut : register(t1);

float3 get_ghost_color(uint index)
{
    int2 size;
    ghost_color_lut.GetDimensions(size.x, size.y);
	return ghost_color_lut[int2(index % size.x, 0)];
}

static const uint3 threads = int3(32, 32, 1);

[numthreads(threads.x, threads.y, threads.z)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 tex_size = (int2) 0;
    luminance_texture.GetDimensions(tex_size.x, tex_size.y);
    if (dispatch_thread_id.x >= tex_size.x || dispatch_thread_id.y >= tex_size.y)
        return;
    
    const int2 center_position = tex_size / 2;
    const float tex_length = length(tex_size);

    float3 color = float3(0.0f, 0.0f, 0.0f);

    const int2 uv = dispatch_thread_id.xy;
    const int2 ghost_base_position = (int2)tex_size - uv;
    const float2 center_vector = (center_position - ghost_base_position) * dispersion_scale;
    for (uint index = 0; index < iteration; ++index)
    {
        const float2 ghost_position = ghost_base_position + center_vector * (dispersion_offset + index);
        const float3 luminace_color = luminance_texture[ghost_position].rgb;

        const float r = length(center_vector - ghost_position);
        const float fadeout_radius = r - ghost_mask_radius;
        const float3 ghost_color = get_ghost_color(index);

        // 光源が重なったときに極端に明るくさせない重み
        float weight = (r / length(center_position));
        // 外周をフェードアウトさせる重み
        weight *= 1.0f - smoothstep(0.0f, ghost_mask_fade * tex_length, r - ghost_mask_radius);
        color += saturate(luminace_color) * ghost_color * weight;
    }
    
    lens_ghost_texture[dispatch_thread_id.xy] = color;
}
