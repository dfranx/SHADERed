#include <SHADERed/UI/Tools/DebuggerOutline.h>
#include <imgui/imgui.h>

namespace ed {
	glm::vec2 getScreenCoord(glm::vec4 v)
	{
		return glm::vec2(((v / v.w) + 1.0f) * 0.5f);
	}

	void DebuggerOutline::RenderPrimitiveOutline(const PixelInformation& pixel, glm::vec2 uiPos, glm::vec2 itemSize, glm::vec2 zoomPos, glm::vec2 zoomSize)
	{
		unsigned int outlineColor = 0xffffffff;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		glm::ivec2 vertPos1 = (getScreenCoord(pixel.glPosition[0]) - zoomPos) * (1.0f / zoomSize) * itemSize;
		glm::ivec2 vertPos2 = (getScreenCoord(pixel.glPosition[1]) - zoomPos) * (1.0f / zoomSize) * itemSize;
		glm::ivec2 vertPos3 = (getScreenCoord(pixel.glPosition[2]) - zoomPos) * (1.0f / zoomSize) * itemSize;

		vertPos1.y = itemSize.y - vertPos1.y;
		vertPos2.y = itemSize.y - vertPos2.y;
		vertPos3.y = itemSize.y - vertPos3.y;

		if (pixel.VertexCount > 1) drawList->AddLine(ImVec2(uiPos.x + vertPos1.x, uiPos.y + vertPos1.y), ImVec2(uiPos.x + vertPos2.x, uiPos.y + vertPos2.y), outlineColor);
		else drawList->AddLine(ImVec2(uiPos.x + vertPos1.x, uiPos.y + vertPos1.y), ImVec2(uiPos.x + 1.0f, uiPos.y + 1.0f), outlineColor);
		if (pixel.VertexCount > 2) drawList->AddLine(ImVec2(uiPos.x + vertPos2.x, uiPos.y + vertPos2.y), ImVec2(uiPos.x + vertPos3.x, uiPos.y + vertPos3.y), outlineColor);
		if (pixel.VertexCount > 2) drawList->AddLine(ImVec2(uiPos.x + vertPos3.x, uiPos.y + vertPos3.y), ImVec2(uiPos.x + vertPos1.x, uiPos.y + vertPos1.y), outlineColor);

		drawList->AddText(ImVec2(uiPos.x + vertPos1.x, uiPos.y + vertPos1.y), outlineColor, "0");
		if (pixel.VertexCount > 2) drawList->AddText(ImVec2(uiPos.x + vertPos2.x, uiPos.y + vertPos2.y), outlineColor, "1");
		if (pixel.VertexCount > 2) drawList->AddText(ImVec2(uiPos.x + vertPos3.x, uiPos.y + vertPos3.y), outlineColor, "2");
	}
	void DebuggerOutline::RenderPixelOutline(const PixelInformation& pixel, glm::vec2 uiPos, glm::vec2 itemSize, glm::vec2 zoomPos, glm::vec2 zoomSize)
	{
		unsigned int outlineColor = 0xffffffff;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		glm::vec2 relCoord = glm::vec2(pixel.Coordinate) / glm::vec2(pixel.RenderTextureSize);
		relCoord += glm::vec2(0.5f / pixel.RenderTextureSize.x, 0.5f / pixel.RenderTextureSize.y);

		glm::ivec2 pxPos = (relCoord - zoomPos) * (1.0f / zoomSize) * itemSize;
		pxPos.y = itemSize.y - pxPos.y;

		float hfpxSizeX = 0.5f / zoomSize.x;
		float hfpxSizeY = 0.5f / zoomSize.y;

		// lines
		drawList->AddLine(ImVec2(uiPos.x, uiPos.y + pxPos.y), ImVec2(uiPos.x + pxPos.x - hfpxSizeY, uiPos.y + pxPos.y), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + pxPos.x + hfpxSizeY, uiPos.y + pxPos.y), ImVec2(uiPos.x + itemSize.x, uiPos.y + pxPos.y), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + pxPos.x, uiPos.y), ImVec2(uiPos.x + pxPos.x, uiPos.y + pxPos.y - hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + pxPos.x, uiPos.y + pxPos.y + hfpxSizeY), ImVec2(uiPos.x + pxPos.x, uiPos.y + itemSize.y), outlineColor);

		// rectangle
		drawList->AddLine(ImVec2(uiPos.x + pxPos.x - hfpxSizeX, uiPos.y + pxPos.y - hfpxSizeY), ImVec2(uiPos.x + pxPos.x - hfpxSizeX, uiPos.y + pxPos.y + hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + pxPos.x + hfpxSizeX, uiPos.y + pxPos.y - hfpxSizeY), ImVec2(uiPos.x + pxPos.x + hfpxSizeX, uiPos.y + pxPos.y + hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + pxPos.x - hfpxSizeX, uiPos.y + pxPos.y + hfpxSizeY), ImVec2(uiPos.x + pxPos.x + hfpxSizeX, uiPos.y + pxPos.y + hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + pxPos.x - hfpxSizeX, uiPos.y + pxPos.y - hfpxSizeY), ImVec2(uiPos.x + pxPos.x + hfpxSizeX, uiPos.y + pxPos.y - hfpxSizeY), outlineColor);

		if (zoomSize.x < 0.3f) {
			static char pxCoord[32] = { 0 };
			sprintf(pxCoord, "(%d, %d)", pixel.Coordinate.x, pixel.Coordinate.y);
			drawList->AddText(ImVec2(uiPos.x + pxPos.x + hfpxSizeX, uiPos.y + pxPos.y - hfpxSizeY), outlineColor, pxCoord);
		}
	}
}