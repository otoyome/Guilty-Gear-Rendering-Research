#include "ImGuiExternal/imgui_external.h"

namespace
{
	static inline int InputTextCallback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			auto* str = reinterpret_cast<std::string*>(data->UserData);
			str->resize(data->BufTextLen);
			data->Buf = str->data();
		}
		return 0;
	}
}

bool ImGui::InputText(const char* label, std::string& str, ImGuiInputTextFlags flags)
{
	flags |= ImGuiInputTextFlags_CallbackResize;

	return ImGui::InputText(
		label,
		str.data(),
		str.capacity() + 1,
		flags,
		InputTextCallback,
		&str
	);
}
