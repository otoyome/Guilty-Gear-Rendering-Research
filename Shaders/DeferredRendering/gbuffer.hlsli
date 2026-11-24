#ifndef __GBUFFER_HLSLI__
#define __GBUFFER_HLSLI__

//  シェーディングモデル定数
static const uint shading_model_unlit       = 0;    //  シェーディング計算：ライティング無し
static const uint shading_model_default_lit = 1; //  シェーディング計算：基本計算
//	[エンジン改造:シェーディング]
static const uint shading_model_ggst_lit    = 2;    //  シェーディング計算：トゥーン計算
static const uint shading_model_toon_lit    = 3;    //  シェーディング計算：トゥーン計算
//	[エンジン改造:シェーディング]

//  マテリアルステンシル
static const uint material_attribute_indirect_lighting_off  = 1 <<  0;  //  間接光の処理を適応させない
static const uint material_attribute_shadow_receiver        = 1 <<  1;  //  影適応

//  空き

static const uint material_attribute_lighting_face_adjustor = 1 << 31;  //  [エンジン改造:顔専用ライティング]

//static const uint material_attribute_ = 1 << 31;

//  GBuffer情報構造体
struct GBufferData
{
    float3 base_color;          //  ベースカラー
    uint shading_model;         //  シェーディング方式
    float3 w_normal;            //  ワールド法線
    float3 w_position;          //  ワールド座標
    float2 velocity;            //  速度
    float linear_depth;         //  線形深度(Decode時のみ)
    float roughness;            //  粗さ
    float metalness;            //  金属質
    float occlusion_factor;     //  遮蔽値
    float occlusion_strength;   //  遮蔽強度
    uint stencil;               //  ビットフラグ(4バイト=32フラグ)
    uint3 unique_data;          //  シェーディングモデル毎に変わる変数(32ビット符号なし整数)

    float2 unique_parameter;    //  シェーディングモデル毎に変わる変数(32ビット浮動小数点)

    //  [エンジン改造:G-Buffer]
    float3 shade_color;         //  トゥーン用：陰色
    float4 ilm_color;           //  トゥーン用：イルミネーション
    //  [エンジン改造:G-Buffer]
};

//  GBufferテクスチャ受け渡し用構造体
struct GBufferTextures
{
    SamplerState state;
    Texture2D<float4> base_color;
    Texture2D<float4> normal_depth;
    Texture2D<float4> parameter;
    Texture2D<float4> velocity;
    Texture2D<uint4> unique_data;

    //  [エンジン改造:G-Buffer]
    Texture2D<float4> shade_color;
    Texture2D<float4> ilm_color;
    //  [エンジン改造:G-Buffer]
};

//  ピクセルシェーダーへの出力用構造体
struct encode_g_buffer_data
{
    float4 base_color;
    float4 normal_depth;
    float4 parameter;
    float4 velocity;
    uint4 unique_data;

    //  [エンジン改造:G-Buffer]
    float4 shade_color;
    float4 ilm_color;
    //  [エンジン改造:G-Buffer]
};

//  圧縮
uint PackFloat2ToUInt(float2 value)
{
    uint a16 = f32tof16(value.x);
    uint b16 = f32tof16(value.y);
    uint abPacked = (a16 << 16) | b16;
    return abPacked;
}

float PackFloat2ToFloat(float2 value)
{
    return asfloat(PackFloat2ToUInt(value));
}

uint PackNormal(float3 n)
{
    //  Octahedral Encoding
    //  https://liamtyler.github.io/posts/octahedral_analysis/
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 p = (n.z >= 0.0) ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
    uint x = (uint)((p.x * 0.5 + 0.5) * 65535.0);
    uint y = (uint)((p.y * 0.5 + 0.5) * 65535.0);
    return (x << 16) | y;
}

//  解凍
float2 UnpackUIntToFloat2(uint packedValue)
{
    float2 result;
    result.x = f16tof32(packedValue >> 16);
    result.y = f16tof32(packedValue);
    return result;
}

float2 UnpackFloatToFloat2(float packedValue)
{
    return UnpackUIntToFloat2(asuint(packedValue));
}

float3 UnpackNormal(uint p)
{
    float2 f;
    f.x = ((p >> 16) & 0xFFFF) / 65535.0 * 2.0 - 1.0;
    f.y = ( p        & 0xFFFF) / 65535.0 * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += (n.xy >= 0.0 ? -t : t);
    return normalize(n);
}

//  UV空間上の速度計算用関数
float2 calculate_uvspace_velocity(float4 current_clip_position, float4 previous_clip_position)
{
    current_clip_position /= current_clip_position.w;
    previous_clip_position /= previous_clip_position.w;
    return clamp((current_clip_position.xy - previous_clip_position.xy) * 0.5f, -1.0f, 1.0f);
}

//  非線形深度から線形深度へ変換
float convert_projection_depth_to_linear_depth(float depth, float4 z_buffer_parameteres)
{
    return 1.0f / (z_buffer_parameteres.x * depth + z_buffer_parameteres.y);
}

//  線形深度から非線形深度へ変換
float convert_linear_depth_to_projection_depth(float depth, float4 z_buffer_parameteres)
{
    return (1.0f - z_buffer_parameteres.y * depth) / (z_buffer_parameteres.x * depth);
}

//  GBufferDataに纏めた情報をピクセルシェーダーの出力用構造体に変換
encode_g_buffer_data encode_g_buffer(in GBufferData data, matrix view_projection_matrix, float4 z_buffer_parameteres)
{
    encode_g_buffer_data ret = (encode_g_buffer_data) 0;
    ret.base_color.rgb = data.base_color.rgb;
    ret.base_color.a = data.shading_model / 32.0f;

    ret.normal_depth.rgb = data.w_normal;
    float4 position = mul(float4(data.w_position, 1.0f), view_projection_matrix);
    ret.normal_depth.a = convert_projection_depth_to_linear_depth(position.z / position.w, z_buffer_parameteres);

    ret.parameter.r = data.occlusion_factor;
    ret.parameter.g = data.roughness;
    ret.parameter.b = data.metalness;
    ret.parameter.a = data.occlusion_strength;
    
    ret.velocity.rg = data.velocity;
    ret.velocity.b = data.unique_parameter.x;
    ret.velocity.a = data.unique_parameter.y;
    
    ret.unique_data.x = data.stencil;
    ret.unique_data.yzw = data.unique_data.xyz;

    //  [エンジン改造:G-Buffer]
    ret.shade_color.xyz = data.shade_color.rgb;
    ret.ilm_color = data.ilm_color;

    return ret;
};

//  ピクセルシェーダーの出力用構造体からGBufferData情報に変換
GBufferData DecodeGBuffer(GBufferTextures textures, float2 uv, matrix inverse_view_projection_transform, float4 z_buffer_parameteres)
{
    uint2 texture_size;
    textures.unique_data.GetDimensions(texture_size.x, texture_size.y);

    //  各テクスチャから情報を取得
    float4 base_color = textures.base_color.SampleLevel(textures.state, uv, 0);
    float4 normal_depth = textures.normal_depth.SampleLevel(textures.state, uv, 0);
    float4 parameter = textures.parameter.SampleLevel(textures.state, uv, 0);
    float4 velocity = textures.velocity.SampleLevel(textures.state, uv, 0);
    uint4 unique_data = textures.unique_data.Load(uint3((uint2) (uv * texture_size), 0));

    //  [エンジン改造:G-Buffer]
    float4 shade_color = textures.shade_color.SampleLevel(textures.state, uv, 0);
    float4 ilm_color = textures.ilm_color.SampleLevel(textures.state, uv, 0);

    GBufferData data = (GBufferData) 0;
    data.base_color = base_color.rgb;
    data.shading_model = (int) (base_color.a * 32.0f + 0.5f);
    data.w_normal = normal_depth.rgb;
    data.linear_depth = normal_depth.a;
    data.occlusion_factor = parameter.r;
    data.roughness = parameter.g;
    data.metalness = parameter.b;
    data.occlusion_strength = parameter.a;
    data.velocity = velocity.rg;
    data.unique_parameter.x = velocity.b;
    data.unique_parameter.y = velocity.a;
    
    data.stencil = unique_data.x;
    data.unique_data.xyz = unique_data.yzw;

    float4 position = float4(uv.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), convert_linear_depth_to_projection_depth(data.linear_depth, z_buffer_parameteres), 1);
    position = mul(position, inverse_view_projection_transform);
    data.w_position = position.xyz / position.w;

    //  [エンジン改造:G-Buffer]
    data.shade_color.rgb = shade_color.xyz;
    data.ilm_color = ilm_color;

    return data;
}

//  GBufferステンシルのフラグが立っているか判定
bool IsGBufferFlags(uint data, uint bit_flag)
{
    return data & bit_flag;
}

bool IsGBufferFlags(GBufferData data, uint bit_flag)
{
    return data.stencil & bit_flag;
}

void SetGBufferFlags(inout uint data, uint bit_flag, bool flag)
{
    data = flag ? (data | bit_flag) : (data & ~bit_flag);
}

void SetGBufferFlags(inout GBufferData data, uint bit_flag, bool flag)
{
    SetGBufferFlags(data.stencil, bit_flag, flag);
}

#endif  //  __GBUFFER_HLSLI__
