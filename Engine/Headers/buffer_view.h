#pragma once

//	バッファビュー
struct buffer_view
{
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	std::vector<BYTE> row_data;
	size_t stride_in_bytes = 0;
	size_t size_in_bytes = 0;
	size_t offset_in_bytes = 0;
	size_t count() const
	{
		return size_in_bytes / stride_in_bytes;
	}
	size_t offset() const
	{
		return offset_in_bytes / stride_in_bytes;
	}
	buffer_view() = default;
	~buffer_view() {}

	template<class Archive>
	void serialize(Archive& archive, int version)
	{
		archive(CEREAL_NVP(format),
				CEREAL_NVP(row_data),
				CEREAL_NVP(stride_in_bytes),
				CEREAL_NVP(size_in_bytes),
				CEREAL_NVP(offset_in_bytes));
	}
};

//	識別子
class semantic
{
public:
	semantic() {}
	semantic(std::string_view name, UINT index)
	: name(name), index(index)
	{
		build();
	}
	bool operator==(const semantic& l) const
	{
		assert(hash != -1);
		assert(l.hash != -1);
		return hash == l.hash;
	}

	template<class Archive>
	void serialize(Archive& archive, int version)
	{
		archive(CEREAL_NVP(name), CEREAL_NVP(index));
		dirty = true;
		build();	//	とりあえずここで
	}

	void build()
	{
		if (!dirty)
			return;

		dirty = false;
		hash = system_utility::generate_hash(name) + index;
	}

	system_utility::hash_key get_hash_key() const
	{
		assert(hash != -1);
		return hash;
	}

private:
	bool dirty = true;
	std::string name;
	UINT index;
	system_utility::hash_key hash = -1;
};

//	描画バッファ情報
struct buffer_datas
{
	enum class flags
	{
		culling = 1 << 0,	//	カリングを行うかどうか
	};
	struct sub_mesh
	{
		const std::unordered_map<semantic, buffer_view>* vertex_buffer_views;
		const buffer_view* index_buffer_view;
		D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	};

	enum class transform_matrix_index
	{
		current = 0,
		previous,

		max,
	};
	UINT vertex_count;					//	Drawで使用する際の頂点数
	UINT system_flags = 0;				//	モデルフラグ類
	std::vector<sub_mesh> color_meshs;	//	色描画メッシュ
	sub_mesh shadow_mesh;				//	影用描画メッシュ
	DirectX::XMFLOAT2 lod_range_rate;	//	LOD距離範囲

	DirectX::XMFLOAT3 bounds_min, bounds_max;

	std::vector<DirectX::XMFLOAT4X4> transform_matrices[static_cast<int>(buffer_datas::transform_matrix_index::max)];
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_buffer;
};


namespace std
{
    template <>
    struct hash<semantic>
	{
		std::size_t operator()(const semantic& item) const
		{
			return item.get_hash_key();
		}
	};
}	//	namespace std
