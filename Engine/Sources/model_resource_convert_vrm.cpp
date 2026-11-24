#include <cmath>

#include "model_resource.h"
#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "ModelFormat/gltf_loader.h"

#include "animation_data_version.h"

namespace
{
	//	アニメーションサンプリングレート
	constexpr float AnimationSamplingRate = system_units::convert_fps_to_sec(1.0f);
}
