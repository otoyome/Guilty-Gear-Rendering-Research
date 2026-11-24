#include "gltf_rendering.hlsli"
#include "gbuffer.hlsli"

#define FORWARD_RENDERING
#define VS_OUT VS_FORWARD_OUT
#include "gltf_surface.hlsli"

#include "model_forward_ps.hlsli"

