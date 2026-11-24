#pragma once

//	メッシュ描画順リスト
struct render_mesh_indices
{
	static constexpr int forward_size = static_cast<int>(translucency_paths::max);
	std::vector<float> lengths;						//	距離リスト
	std::vector<std::size_t> opaque;				//	不透明描画リスト
	std::vector<std::size_t> mask;					//	マスク描画リスト
	std::vector<std::size_t> forward[forward_size];	//	半透明描画リスト
	void clear()
	{
		opaque.clear();
		mask.clear();
		for (auto& indices : forward)
			indices.clear();
	}
};

//	レンダラー用コンテキスト
struct renderer_context
{
	common_pipeline_states::scene_constants scene_constant;		//	シーン情報
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;	//	シーン定数バッファ
	std::shared_ptr<render_data_context> render_data;			//	描画情報
	std::shared_ptr<render_mesh_indices> render_mesh_indices;	//	メッシュ描画順データ
};
