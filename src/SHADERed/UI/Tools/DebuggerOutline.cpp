#include <SHADERed/UI/Tools/DebuggerOutline.h>
#include <imgui/imgui.h>

namespace ed {
	glm::vec2 getScreenCoord(glm::vec4 v)
	{
		return glm::vec2(((v / v.w) + 1.0f) * 0.5f);
	}
	glm::vec2 getScreenCoordInverted(glm::vec4 v)
	{
		glm::vec2 ret = getScreenCoord(v);
		return glm::vec2(ret.x, 1-ret.y);
	}

	void DebuggerOutline::RenderPrimitiveOutline(const PixelInformation& pixel, glm::vec2 uiPos, glm::vec2 itemSize, glm::vec2 zoomPos, glm::vec2 zoomSize)
	{
		unsigned int outlineColor = 0xffffffff;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		glm::ivec2 vPos[6];
		int i, j;

		for (i = 0; i < pixel.VertexCount; ++i) {
			vPos[i] = (getScreenCoord(pixel.VertexShaderPosition[i]) - zoomPos) * (1.0f / zoomSize) * itemSize;
			vPos[i].y = itemSize.y - vPos[i].y;
		}

		for (i = 0, j = 1; j < pixel.VertexCount; ++i, ++j)
			drawList->AddLine(ImVec2(uiPos.x + vPos[i].x, uiPos.y + vPos[i].y), ImVec2(uiPos.x + vPos[j].x, uiPos.y + vPos[j].y), outlineColor);
		if (pixel.VertexCount > 2)
			drawList->AddLine(ImVec2(uiPos.x + vPos[i].x, uiPos.y + vPos[i].y), ImVec2(uiPos.x + vPos[0].x, uiPos.y + vPos[0].y), outlineColor);

		for (i = 0; i < pixel.VertexCount; ++i) {
			const char c[1] = { '0' + i };
			drawList->AddText(ImVec2(uiPos.x + vPos[i].x, uiPos.y + vPos[i].y), outlineColor, &c[0], &c[1]);
		}

		if (pixel.GeometryShaderUsed) {
			int gsOutputsCount = 0;
			glm::ivec2 gsPos[3];

			switch (pixel.GeometryOutputType) {
			case GeometryShaderOutput::Points:
				gsOutputsCount = 1;
				break;
			case GeometryShaderOutput::LineStrip:
				gsOutputsCount = 2;
				break;
			case GeometryShaderOutput::TriangleStrip:
				gsOutputsCount = 3;
				break;
			}

			for (i = 0; i < gsOutputsCount; ++i)
				gsPos[i] = (getScreenCoordInverted(pixel.FinalPosition[i]) - glm::vec2(zoomPos.x, -zoomPos.y)) * (1.0f / zoomSize) * itemSize;
			for (i = 0, j = 1; j < gsOutputsCount; ++i, ++j)
				drawList->AddLine(ImVec2(uiPos.x + gsPos[i].x, uiPos.y + gsPos[i].y), ImVec2(uiPos.x + gsPos[j].x, uiPos.y + gsPos[j].y), outlineColor);
			if (gsOutputsCount > 2)
				drawList->AddLine(ImVec2(uiPos.x + gsPos[i].x, uiPos.y + gsPos[i].y), ImVec2(uiPos.x + gsPos[0].x, uiPos.y + gsPos[0].y), outlineColor);
			for (i = 0; i < gsOutputsCount; ++i) {
				const char GS0[4] = { 'G', 'S', '0' + i, '\0' };
				drawList->AddText(ImVec2(uiPos.x + gsPos[i].x, uiPos.y + gsPos[i].y), outlineColor, GS0);
			}
		}
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