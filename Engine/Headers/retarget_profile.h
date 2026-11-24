#pragma once

#include <map>
#include <string>

//	リターゲティング用情報
struct retarget_profile
{
	enum class modes
	{
		normal,
		ik,
	};
	struct bone_data
	{
		std::string target_name;

		DirectX::XMFLOAT3 scale = {1, 1, 1};
		DirectX::XMFLOAT4 rotation = {0, 0, 0, 1};
		DirectX::XMFLOAT3 translation = {0, 0, 0};

		bool is_lock_scale = false;
		bool is_lock_rotation = false;
		bool is_lock_translation = false;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	modes mode = modes::normal;								//	モード
	std::map<std::string, bone_data> bone_map;				//	ボーンマップ
	DirectX::XMFLOAT3 base_scaling = { 1.0f, 1.0f, 1.0f };	//	基準のスケーリング情報

	template<class Archive>
	void serialize(Archive& archive, int version);
};

//	リターゲティング用情報
struct retarget_profile_data
{
	//	ファイルごとのリターゲッティング情報
	std::map<std::string, std::vector<retarget_profile>> profiles;

	bool has_profile(std::string_view file_name);

	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);
};
