#ifndef __GLTF_SURFACE_ALPHA_HLSLI__
#define __GLTF_SURFACE_ALPHA_HLSLI__

#include "gltf_rendering.hlsli"
#include "gbuffer.hlsli"

//  ñ èÓïÒ
float surface(VS_OUT pin, bool is_front_face)
{
    float alpha = base_color_factor.a;
    if (HAS_RESOURCE(BaseColorMap))
    {
        alpha *= TEXTURE2D_SAMPLE(BaseColorMap, linear_sampler_state, pin.texcoord[BaseTexcoord]).a;
    }
    return alpha;
}

#endif	//	__GLTF_SURFACE_ALPHA_HLSLI__
