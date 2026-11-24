#include "toon_rendering.hlsli"
#include "gbuffer.hlsli"
#include "shading_models.hlsli"
#include "forward_light_data.hlsli"

#define FORWARD_RENDERING
#define VS_OUT VS_FORWARD_OUT
#include "toon_surface.hlsli"

#include "model_forward_ps.hlsli"

