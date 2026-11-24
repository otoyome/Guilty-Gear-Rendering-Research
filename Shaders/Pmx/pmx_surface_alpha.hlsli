#ifndef __PMX_SURFACE_ALPHA_HLSLI__
#define __PMX_SURFACE_ALPHA_HLSLI__

#include "pmx_rendering.hlsli"
#include "gbuffer.hlsli"

//  面情報
float surface(VS_OUT pin, bool is_front_face)
{
    //  ベースカラー
    float alpha = base_color_factor.a;
    if (HAS_RESOURCE(BaseMap))
    {
        alpha *= TEXTURE2D_SAMPLE(BaseMap, linear_sampler_state, pin.texcoord[BaseTexcoord]).a;
    }
    return alpha;
}

#endif	//	__PMX_SURFACE_ALPHA_HLSLI__
