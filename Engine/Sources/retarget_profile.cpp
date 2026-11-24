#include <cmath>

#include "retarget_profile.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/unordered_map.hpp"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(retarget_profile::bone_data, 2)
CEREAL_CLASS_VERSION(retarget_profile, 1)

template<class Archive>
void retarget_profile::bone_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 2:
		{
			archive(CEREAL_NVP(target_name),
					CEREAL_NVP(scale),
					CEREAL_NVP(rotation),
					CEREAL_NVP(translation),
					CEREAL_NVP(is_lock_scale),
					CEREAL_NVP(is_lock_rotation),
					CEREAL_NVP(is_lock_translation));
			break;
		}
		default:
		{
			archive(CEREAL_NVP(target_name),
					CEREAL_NVP(scale),
					CEREAL_NVP(rotation),
					CEREAL_NVP(translation));
			break;
		}
	}
}
template<class Archive>
void retarget_profile::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(bone_map), CEREAL_NVP(mode), CEREAL_NVP(base_scaling));
}

bool retarget_profile_data::has_profile(std::string_view file_name)
{
	return profiles.find(std::string(file_name)) != profiles.end();
}

bool retarget_profile_data::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if(ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(profiles));
		}
		catch (...)
		{
			DEBUG_WARNING("retarget_profile_data[%s] serialize failed.", file_path.data());
			return false;
		}

		return true;
	}
	return false;
}

bool retarget_profile_data::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if(istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(profiles));
		}
		catch (...)
		{
			DEBUG_WARNING("retarget_profile_data[%s] deserialize failed.", file_path.data());
			return false;
		}

		return true;
	}
	return false;
}
