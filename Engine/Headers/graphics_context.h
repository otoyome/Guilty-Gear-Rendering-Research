#pragma once

#include "light.h"
#include "texture.h"
#include "buffer_view.h"
#include "collision/capsule.h"

#include "post_process_volume.h"

#include "common_pipeline_states.h"

//	表面描画パス
enum class surface_paths : std::uint8_t
{
	depth_write_pass,	//	深度書き込みパス
	g_buffer_pass,		//	G-Buffer生成パス
	shadow_write_pass,	//	シャドウマップ生成パス
	forward_pass,		//	フォワードレンダリングパス
};

//	半透明 surface 用描画タイミング
enum class translucency_paths : std::uint8_t
{
	before_dof,			//	被写界深度前
	after_dof,			//	被写界深度後
	after_motion_blur,	//	モーションブラー後

	max,

	unknown,
};

//	post process 用描画タイミング
enum class blendable_locations : std::uint8_t
{
	before_dof,				//	被写界深度前
	after_dof,				//	被写界深度後
	after_dof_translucency,	//	被写界深度後半透明描画後

	before_bloom,			//	ブルーム前
	after_tone_mapping,		//	トーンマッピング後

	unknown,
};

//	材質
class material;
struct material_datas
{
	material* material;
};

//	メッシュ描画情報コンテナ
struct mesh_container
{
	//	カプセルシャドウ情報
	struct capsule_shadow_datas
	{
		collision::bounding_capsule capsule;
	};
	std::vector<buffer_datas> buffer_datas;
	std::vector<material_datas> material_datas;
	std::vector<capsule_shadow_datas> capsule_shadow_datas;
	//	描画情報
	struct draw_datas
	{
		UINT buffer_index;
		UINT material_index;
	};
	std::vector<draw_datas> draw_datas;
	std::vector<std::size_t> color_datas;
	std::vector<std::size_t> shadow_datas;
	void clear()
	{
		draw_datas.clear();
		shadow_datas.clear();
		color_datas.clear();
	}
};

//	スプライト情報コンテナ
class sprite;
struct sprite_datas
{
	int sort_key;
	int sort_order;
	const sprite* sprite;
};
struct sprite_container
{
	std::vector<sprite_datas> sprite_datas;
};

#if defined(USE_IMGUI)
//	デバッグ描画情報コンテナ
struct debug_draw_primitive_container
{
	enum class line_buffer_id
	{
		normal,
		disable_depth_test,

		max,
	};
	buffer_datas line_buffer_datas[static_cast<int>(line_buffer_id::max)];
};
#endif // defined(USE_IMGUI)

//	描画データコンテキスト
struct render_data_context
{
	light_container light_container;
	post_process_volume_container post_process_volume_container;
	mesh_container mesh_container;
	sprite_container sprite_container;
#if defined(USE_IMGUI)
	debug_draw_primitive_container debug_draw_container;
#endif // defined(USE_IMGUI)
};

//	描画コンテキスト
struct graphics_context
{
	ID3D11DeviceContext* device_context;
	std::shared_ptr<render_data_context> render_data;

	//	シーン情報
	common_pipeline_states::scene_constants scene_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;

	//	IBL関係
	texture_ptr diffuse_ibl_texture;
	texture_ptr specluar_ibl_texture;
	texture_ptr lut_ggx_texture;

	//	シーンテクスチャ
	texture_ptr scene_color_texture;
	texture_ptr scene_depth_texture;
	texture_ptr g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::max)];
	texture_ptr work_texture[8];

	//	遅延ライティング処理用情報
	common_pipeline_states::deferred_light_constants deferred_light_constant;

	//	シャドウマップ用情報
	bool is_shadow_rendering = false;	//	影生成描画処理中判定
	common_pipeline_states::shadow_map_constants shadow_map_constant;
	texture_ptr shadow_map_texture;

	//	スクリーン空間反射用情報
	common_pipeline_states::screen_space_reflection_constants screen_space_reflection_constant;

public:
	//	定数バッファの設定
	void set_constant_buffer(int start_slot, int num, ID3D11Buffer* const* constant_buffers) const;

	//	シェーダーリソースビューの設定
	void set_shader_resource(int start_slot, int num, ID3D11ShaderResourceView* const* shader_resources) const;

	//	サンプラーステートの設定
	void set_sampler(int start_slot, int num, ID3D11SamplerState* const* sampler_state) const;

	//	設定していたシェーダーを全て解除する
	void clear_shader_slots() const;

	//	設定されている頂点バッファを外す
	void clear_vertex_buffer() const;

	//	設定されているインデックスバッファを外す
	void clear_index_buffer() const;

	//	描画ターゲットのクリア
	void clear_render_target_slots() const;

	//	CBVのクリア設定
	void clear_constant_buffers(int start_slot = 0, int num = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) const;

	//	設定していたSRVを全て解除する
	void clear_shader_resorce_views(int start_slot = 0, int num = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) const;

	//	設定していたUAVを全て解除する
	//	FEATURELEVELが11.1ならnumはD3D11_1_UAV_SLOT_COUNTになる
	void clear_unordered_access_views(int start_slot = 0, int num = 8) const;

	//	設定していたSamplerを全て解除する
	void clear_sampler(int start_slot = 0, int num = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) const;

	//	デバッグイベント開始
	void debug_begin_event(const wchar_t* event_name);

	//	デバッグイベント終了
	void debug_end_event();
};
