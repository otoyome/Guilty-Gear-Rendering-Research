#ifndef __TOON_RENDERING_HLSLI__
#define __TOON_RENDERING_HLSLI__

#define HAS_TANGENT
#define VERTEX_COLOR_MAX    1
#define TEXCOORD_MAX        2
#define BONE_MAX            8
#include "model.hlsli"

#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseMap, 0)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, IlmMap, 1);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, SssMap, 2);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, DetailMap, 3);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, EmissiveMap, 4);
static const int texture_max = 5;

//  UV座標
static const uint BaseTexcoord = 0;
static const uint IlmTexcoord = 0;
static const uint SssTexcoord = 0;
static const uint DetailTexcoord = 1;
static const uint EmissiveTexcoord = 0;

//  サンプラーステート
SamplerState linear_sampler_state : register(s0);

//  マテリアル情報
cbuffer TOON_MATERIAL_CONSTANT_BUFFER
{
    float4  material_base_color;            //  ベースカラー
    float4  material_sss_color;             //  SSS乗算カラー
    float4  material_parameters;            //  r : 粗さ, g : 金属質, b : 陰影閾値, a : 陰影オフセット
    float4  fake_ambient_color;             //  疑似的な環境光色

    //  背面法による輪郭線情報
    float   outline_thickness;              //  輪郭線の厚み
    float   outline_depth_offset_distance;  //  輪郭線の深度オフセット距離(固定値でも良いと思う)

    //  顔マテリアル判別用
    bool face_material_flag;                //  顔
};

//	基底の範囲で0～1を算出する関数
#define linear_step(a, b, d) saturate((d - a) / (b - a))

//  輪郭線算出関数
float3 calculate_outline_position(float3 w_position, float3 w_normal, float4 vertex_color)
{
    // r : 陰影判定の閾値に対するオフセット 1が標準で0だと必ず影になる
    // g : カメラからの輪郭線の距離に応じてどの程度膨張させるかの係数
    // b : 輪郭線のZオフセット値
    // a : 輪郭線の太さ係数。 0.5:標準、0で無し

    // 距離による基準サイズ算出
    float base_thickness = 0.0f;
    {
        // カメラからの距離に応じた膨張係数を算出
        float camera_to_vertex = distance(w_position, camera_position.xyz);
#if 01
        camera_to_vertex *= get_tan_fov_y();   // 視野角補正
        camera_to_vertex *= 1.0f + vertex_color.g;    // 頂点色補正
        base_thickness = outline_thickness * camera_to_vertex;
#else   // 一応下記のような方法もある
        float2  outline_thickness;              //  輪郭線の厚み
        float2  outline_scaling_range;          //  輪郭線のサイズを変異させる範囲
        float camera_factor = linear_step(outline_scaling_range.x, outline_scaling_range.y, camera_to_vertex);
        camera_factor *= get_tan_fov_y();   // 視野角補正
        camera_factor *= vertex_color.g;    // 頂点色補正
        base_thickness = lerp(outline_thickness.x, outline_thickness.y, camera_factor);
#endif
    }

    // 厚み算出
    float thickness = convert_cm_to_unit(base_thickness);
    {
        thickness *= (vertex_color.a * 2.0f);   // 頂点色補正
    }

    // 位置調整
    w_position += normalize(w_normal) * thickness;

    // Zオフセット
    float depth_offset = convert_cm_to_unit(outline_depth_offset_distance) * vertex_color.b;
    w_position += normalize(w_position - camera_position.xyz) * depth_offset;

    return w_position;
}

#endif  //  __TOON_RENDERING_HLSLI__
