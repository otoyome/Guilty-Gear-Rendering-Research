//  下記の定数を定義してインクルードする事
//  #define HAS_TANGENT         : 接線を所持しているかどうか
//  #define VERTEX_COLOR_MAX    : 頂点カラー数
//  #define TEXCOORD_MAX        : UV座標数
//  #define BONE_MAX            : ボーンの最大数
//  #define FORWARD_RENDERING   : フォワードレンダリング用の計算を行う
#ifndef __MODEL_HLSLI__
#define __MODEL_HLSLI__

#include	"lights.hlsli"
#include    "common.hlsli"

#include	"scene_constant_buffer.hlsli"
#include	"shadow_map.hlsli"

#include    "gbuffer.hlsli"

//  ↑のマクロの宣言状況に応じて追加するかしないかを決めるためのマクロ
#if VERTEX_COLOR_MAX > 0
static const uint VertexColorMax = VERTEX_COLOR_MAX;
#define DECLARATION_IN_COLOR    float4 color[VertexColorMax] : COLOR;
#define DECLARATION_OUT_COLOR   float4 color[VertexColorMax] : COLOR;
#else   //  VERTEX_COLOR_MAX > 0
#define DECLARATION_IN_COLOR
#define DECLARATION_OUT_COLOR
#endif  //  VERTEX_COLOR_MAX > 0

#if TEXCOORD_MAX > 0
static const uint TexcoordMax = TEXCOORD_MAX;
#define DECLARATION_IN_TEXCOORD     float2 texcoord[TexcoordMax] : TEXCOORD;
#define DECLARATION_OUT_TEXCOORD    float2 texcoord[TexcoordMax] : TEXCOORD;
#else   //  TEXCOORD_MAX > 0
#define DECLARATION_IN_TEXCOORD
#define DECLARATION_OUT_TEXCOORD
#endif  //  TEXCOORD_MAX > 0

#if BONE_MAX > 0
static const uint AlignmentBoneMax = alignment( BONE_MAX, 4 ) / 4;
static const uint BoneMax = BONE_MAX;
#define DECLARATION_IN_JOINTS   uint4 joints[AlignmentBoneMax] : JOINTS;
#define DECLARATION_IN_WEIGHTS  float4 weights[AlignmentBoneMax] : WEIGHTS;
#else   //  BONE_MAX > 0
#define DECLARATION_IN_JOINTS
#define DECLARATION_IN_WEIGHTS
#endif  //  BONE_MAX > 0

#if defined(HAS_TANGENT)
#define DECLARATION_IN_TANGENT  float4 tangent : TANGENT;
#define DECLARATION_OUT_TANGENT float4 w_tangent : TANGENT;
#else   //  defined(HAS_TANGENT)
#define DECLARATION_IN_TANGENT
#define DECLARATION_OUT_TANGENT
#endif  //  defined(HAS_TANGENT)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  深度書き込み用
struct VS_DEPTH_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    DECLARATION_IN_TANGENT
    DECLARATION_IN_COLOR
    DECLARATION_IN_TEXCOORD
    DECLARATION_IN_JOINTS
    DECLARATION_IN_WEIGHTS
};

struct VS_DEPTH_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION;   //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_TANGENT
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  シャドウマップ生成用
struct VS_SHADOW_MAP_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    DECLARATION_IN_COLOR
    DECLARATION_IN_TEXCOORD
    DECLARATION_IN_JOINTS
    DECLARATION_IN_WEIGHTS
};

struct VS_SHADOW_MAP_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION;   //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  GBuffer生成用
struct VS_GBUFFER_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    DECLARATION_IN_TANGENT
    DECLARATION_IN_COLOR
    DECLARATION_IN_TEXCOORD
    DECLARATION_IN_JOINTS
    DECLARATION_IN_WEIGHTS
};

struct VS_GBUFFER_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION;   //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_TANGENT
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD

    float4 current_clip_position : CLIP_POSITION0;
    float4 previous_clip_position : CLIP_POSITION1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  フォワードレンダリング用
#define VS_FORWARD_IN VS_GBUFFER_IN

struct VS_FORWARD_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION; //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_TANGENT
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD

    float4 current_clip_position : CLIP_POSITION0;
    float4 previous_clip_position : CLIP_POSITION1;
    float4 shadow_texcoords[shadow_map_max] : SHADOW_TEXCOORDS;
};

#if BONE_MAX > 0
////////////////////////////////////////////////////////////////////////////////////////////////////
//  ボーン対応する必要のあるモデルの計算
static const uint mesh_bone_max = 500;	//	1メッシュ500本あれば大丈夫？
cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b1)
{
	bool has_tangent;	//	接線を所持しているかどうか
	uint3 primitive_dummy;
    row_major float4x4 bone_transforms[mesh_bone_max];
    row_major float4x4 previous_bone_transforms[mesh_bone_max];
};

#endif  //  BONE_MAX > 0

////////////////////////////////////////////////////////////////////////////////////////////////////
//  サーフェイス計算結果格納情報
struct SurfaceData
{
    float4 base_color; //  ベースカラー
    float3 emissive_color; //  自己発光
    uint shading_model; //  シェーディング方式
    float3 w_normal; //  ワールド法線
    float3 w_position; //  ワールド座標
    float2 velocity; //  速度
    float roughness; //  粗さ
    float metalness; //  金属質
    float occlusion_factor; //  遮蔽値
    float occlusion_strength; //  遮蔽強度
    uint stencil; //  ビットフラグ(4バイト=32フラグ)
    uint3 unique_data; //  シェーディングモデル毎に変わる変数(32ビット符号なし整数)
    float2 unique_parameter; //  シェーディングモデル毎に変わる変数(32ビット浮動小数点)

    //  [エンジン改造:G-Buffer]
    float3 shade_color; //  トゥーン用：陰色
    float4 ilm_color; //  トゥーン用：イルミネーション色
};

GBufferData convert_g_buffer_data(in SurfaceData surface_data)
{
    GBufferData g_buffer_data = (GBufferData)0;
    g_buffer_data.base_color = surface_data.base_color.rgb;
    g_buffer_data.shading_model = surface_data.shading_model;
    g_buffer_data.w_normal = surface_data.w_normal;
    g_buffer_data.w_position = surface_data.w_position;
    g_buffer_data.velocity = surface_data.velocity;
    g_buffer_data.roughness = surface_data.roughness;
    g_buffer_data.metalness = surface_data.metalness;
    g_buffer_data.occlusion_factor = surface_data.occlusion_factor;
    g_buffer_data.occlusion_strength = surface_data.occlusion_strength;
    g_buffer_data.stencil = surface_data.stencil;
    g_buffer_data.unique_parameter = surface_data.unique_parameter;
    g_buffer_data.unique_data = surface_data.unique_data;

    //  [エンジン改造:G-Buffer]
    g_buffer_data.shade_color = surface_data.shade_color;
    g_buffer_data.ilm_color = surface_data.ilm_color;
    //  [エンジン改造:G-Buffer]

    return g_buffer_data;
}


#endif  //  __MODEL_HLSLI__
