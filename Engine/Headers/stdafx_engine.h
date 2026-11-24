#pragma once

#define	NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <algorithm>

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>

#include "System/misc.h"
#include "System/high_resolution_timer.h"
#include "System/enum_class_attribute.h"
#include "System/system_units.h"
#include "System/system_utility.h"
#include "System/math_utility.h"
#include "System/singleton.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl.h>
#include <directxmath.h>
#include <cassert>


#include "render_data.h"
#include "shader.h"
#include "graphics_utility.h"

#include "material.h"

#include "input/mouse.h"
#include "input/keyboard.h"

#include "Graphics/direct_x_helper.h"

#if	defined( USE_IMGUI )
#define	IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif	//	defined( USE_IMGUI )
