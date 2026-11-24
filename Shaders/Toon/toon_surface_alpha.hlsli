#ifndef __TOON_SURFACE_ALPHA_HLSLI__
#define __TOON_SURFACE_ALPHA_HLSLI__

#include "toon_rendering.hlsli"
#include "gbuffer.hlsli"

//  面情報
float surface(VS_OUT pin, bool is_front_face)
{
    //  ベースカラー
    float alpha = material_base_color.a;
    if (HAS_RESOURCE(BaseMap))
    {
        alpha *= TEXTURE2D_SAMPLE(BaseMap, linear_sampler_state, pin.texcoord[BaseTexcoord]).a;
    }
    return alpha;
}

#endif	//	__TOON_SURFACE_ALPHA_HLSLI__
