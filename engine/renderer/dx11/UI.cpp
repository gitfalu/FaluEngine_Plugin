#include "FaluEngine/UI.h"
#include <imgui.h>

namespace FaluEngine
{
	void drawText(float x, float y,const std::string& text, uint32_t colorABGR)
	{
		ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), colorABGR, text.c_str());
	}
}
