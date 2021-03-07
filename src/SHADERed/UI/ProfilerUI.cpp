#include <SHADERed/UI/ProfilerUI.h>
#include <SHADERed/Objects/Settings.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>

#define PROFILER_PADDING 5.0f

namespace ed {
	void ProfilerUI::OnEvent(const SDL_Event& e)
	{
	}
	void ProfilerUI::Update(float delta)
	{
		if (!Settings::Instance().General.Profiler) {
			ImGui::TextWrapped("Turn on the 'Profiler' in Options -> General.");
			return;
		}
		const std::vector<PerformanceTimer>& perfTimers = m_data->Renderer.GetPerformanceTimers();
		unsigned long long gpuTime = m_data->Renderer.GetGPUTime();
		unsigned long long timeOffset = 0;

		m_renderRow(0, "Total time", gpuTime, 0ull, gpuTime);

		int index = 1;
		for (const auto& timer : perfTimers) {
			m_renderRow(index, timer.Pass->Name, timer.LastTime, timeOffset, gpuTime);
			timeOffset += timer.LastTime;
			index++;
		}
	}
	void ProfilerUI::m_renderRow(int index, const char* name, uint64_t time, uint64_t timeOffset, uint64_t totalTime)
	{
		const float barWidth = (float)time / totalTime;
		const float barX = (float)timeOffset / totalTime;

		const float windowWidth = ImGui::GetWindowWidth();
		const float barMaxWidth = (3 * windowWidth) / 4 - 2*PROFILER_PADDING;

		const float rowHeight = Settings::Instance().General.FontSize + 2 * PROFILER_PADDING;
		const float rowPosY = rowHeight * index;
		
		const ImVec2 screenPos = ImGui::GetWindowPos() + ImGui::GetCursorStartPos();


		// title
		ImGui::SetCursorPos(ImVec2(5.0f, ImGui::GetWindowContentRegionMin().y + rowPosY + 5.0f));
		ImGui::TextUnformatted(name);
		ImGui::NewLine();

		// bar
		ImVec2 barStart = ImVec2(screenPos.x + (windowWidth - barMaxWidth - 10.0f) + barX * barMaxWidth + 5.0f, screenPos.y + rowPosY + 2.0f);
		ImVec2 barEnd = ImVec2(barStart.x + barWidth * barMaxWidth, barStart.y + rowHeight - 4.0f);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(barStart, barEnd, ImGui::GetColorU32(ImGuiCol_FrameBg));

		// time text
		char timeText[16] = { 0 };
		sprintf(timeText, "%.4f ms", time / 1000000.0f);
		ImVec2 textSize = ImGui::CalcTextSize(timeText);
		drawList->AddText(ImVec2(barStart.x + (barWidth * barMaxWidth - textSize.x) / 2, barStart.y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), timeText);
	
		// vertical border
		ImVec2 vertStart = ImVec2(screenPos.x + (windowWidth - barMaxWidth - 10.0f), screenPos.y + rowPosY);
		ImVec2 vertEnd = ImVec2(vertStart.x, vertStart.y + rowHeight);
		drawList->AddLine(vertStart, vertEnd, ImGui::GetColorU32(ImGuiCol_Separator));

		// horizontal border
		ImVec2 horizStart = ImVec2(screenPos.x, screenPos.y + rowPosY + rowHeight);
		ImVec2 horizEnd = ImVec2(screenPos.x + windowWidth - 5.0f, horizStart.y);
		drawList->AddLine(horizStart, horizEnd, ImGui::GetColorU32(ImGuiCol_Separator));
	}
}