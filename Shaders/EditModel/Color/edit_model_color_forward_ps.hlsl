#include "edit_model_color_rendering.hlsli"
#include "gbuffer.hlsli"

#define FORWARD_RENDERING
#define VS_OUT VS_FORWARD_OUT
#include "edit_model_color_surface.hlsli"

#include "model_forward_ps.hlsli"

