#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace ImGui
{
	bool InputText(const char* label, std::string& str, ImGuiInputTextFlags flags = 0);
}
