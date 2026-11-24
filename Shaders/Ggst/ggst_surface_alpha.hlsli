#ifndef __GGST_SURFACE_ALPHA_HLSLI__
#define __GGST_SURFACE_ALPHA_HLSLI__

#include "ggst_rendering.hlsli"
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

#endif	//	__GGST_SURFACE_ALPHA_HLSLI__
