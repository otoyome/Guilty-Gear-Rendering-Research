#ifndef __EDIT_MODEL_COLOR_RENDERING_HLSLI__
#define __EDIT_MODEL_COLOR_RENDERING_HLSLI__

#define VERTEX_COLOR_MAX    1
#define BONE_MAX            8
#define TEXCOORD_MAX        0
#include "model.hlsli"

#include "material_parameter_constant_buffer.hlsli"

//  マテリアル情報
cbuffer MATERIAL_CONSTANT_BUFFER
{
	float line_factor;
	float3 line_color;
};

#endif	//	__EDIT_MODEL_COLOR_RENDERING_HLSLI__
