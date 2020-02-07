#include "PixelInspectUI.h"
#include "../Objects/DebugInformation.h"
#include "../Objects/Settings.h"
#include "Icons.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#define ICON_BUTTON_WIDTH 25 * Settings::Instance().DPIScale
#define BUTTON_SIZE 20 * Settings::Instance().DPIScale

namespace ed
{
	void PixelInspectUI::OnEvent(const SDL_Event& e)
	{
	}
	void PixelInspectUI::Update(float delta)
	{
		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		std::vector<PixelInformation>& pixels = m_data->Debugger.GetPixelList();

		ImGui::BeginChild("##pixel_scroll_container", containerSize);

		int pxId = 0;

		for (auto& pixel : pixels) {
			ImGui::Text("%s(%s) - %s", pixel.Owner->Name, pixel.RenderTexture.c_str(), pixel.Object->Name);
			ImGui::PushItemWidth(-1);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::ColorEdit4("##pixel_edit", const_cast<float*>(glm::value_ptr(pixel.Color)));
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();

			if (!pixel.Fetched) {
				if (ImGui::Button(("Fetch##pixel_fetch_" + std::to_string(pxId)).c_str(), ImVec2(-1, 0))) {
					int vertID = m_data->Renderer.DebugVertexPick(pixel.Owner, pixel.Object, pixel.Coordinate.x, pixel.Coordinate.y);
					
					// TODO: lines, points, etc...
					int vertCount = 3;

					if (pixel.Object->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem::GeometryType geoType = ((pipe::GeometryItem*)pixel.Object->Data)->Type;
						GLuint vbo = ((pipe::GeometryItem*)pixel.Object->Data)->VBO;

						// forgive me for my sins :'D¸TODO: v1.3.*
						glBindBuffer(GL_ARRAY_BUFFER, vbo);
						if (geoType == pipe::GeometryItem::GeometryType::ScreenQuadNDC) {
							GLfloat bufData[4 * 4] = { 0.0f };
							glGetBufferSubData(GL_ARRAY_BUFFER, 0, 4 * 4 * sizeof(float), &bufData[0]);

							pixel.Vertex[0] = glm::vec3(bufData[0], bufData[1], 0.0f);
							pixel.Vertex[1] = glm::vec3(bufData[2], bufData[3], 0.0f);
							pixel.Vertex[2] = glm::vec3(0,0,0);
							pixel.VertexCount = vertCount;
						} else {
							GLfloat bufData[3 * 18] = { 0.0f };
							int vertStart = ((int)(vertID / vertCount)) * vertCount;
							glGetBufferSubData(GL_ARRAY_BUFFER, vertStart * 18 * sizeof(float), vertCount * 18 * sizeof(float), &bufData[0]);

							pixel.Vertex[0] = glm::vec3(bufData[0], bufData[1], bufData[2]);
							pixel.Vertex[1] = glm::vec3(bufData[18], bufData[19], bufData[20]);
							pixel.Vertex[2] = glm::vec3(bufData[36], bufData[37], bufData[38]);
							pixel.VertexCount = vertCount;
						}
						glBindBuffer(GL_ARRAY_BUFFER, 0);
					} else {
						int vertStart = ((int)(vertID / vertCount)) * vertCount;

						pipe::Model* mdl = ((pipe::Model*)pixel.Object->Data);
						pixel.Vertex[0] = mdl->Data->Meshes[0].Vertices[vertStart+0].Position;
						pixel.Vertex[1] = mdl->Data->Meshes[0].Vertices[vertStart+1].Position;
						pixel.Vertex[2] = mdl->Data->Meshes[0].Vertices[vertStart+2].Position;
					}

					pixel.Fetched = true;
				}
			}

			if (pixel.Fetched) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

				ImGui::Button(UI_ICON_PLAY, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)); ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::ColorEdit4("##dbg_pixel_edit", const_cast<float*>(glm::value_ptr(pixel.DebuggerColor)));
				ImGui::PopItemFlag();
				ImGui::PopItemWidth();

				ImGui::Button(UI_ICON_PLAY, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)); ImGui::SameLine();
				ImGui::Text("Vertex[0] = (%.2f, %.2f, %.2f)", pixel.Vertex[0].x, pixel.Vertex[0].y, pixel.Vertex[0].z);

				ImGui::Button(UI_ICON_PLAY, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)); ImGui::SameLine();
				ImGui::Text("Vertex[1] = (%.2f, %.2f, %.2f)", pixel.Vertex[1].x, pixel.Vertex[1].y, pixel.Vertex[1].z);

				ImGui::Button(UI_ICON_PLAY, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)); ImGui::SameLine();
				ImGui::Text("Vertex[2] = (%.2f, %.2f, %.2f)", pixel.Vertex[2].x, pixel.Vertex[2].y, pixel.Vertex[2].z);

				ImGui::PopStyleColor();
			}

			ImGui::Separator();
			ImGui::NewLine();

			pxId++;
		}
		
		ImGui::EndChild();
	}
}