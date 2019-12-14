#include "ObjectListUI.h"
#include "ObjectPreviewUI.h"
#include "PropertyUI.h"
#include "../Objects/Settings.h"
#include "../Objects/Logger.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/GLUtils.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define IMAGE_CONTEXT_WIDTH 150 * Settings::Instance().DPIScale


namespace ed
{
	void ObjectListUI::OnEvent(const SDL_Event & e)
	{
	}
	void ObjectListUI::Update(float delta)
	{
		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		bool itemMenuOpened = false;
		std::vector<std::string> items = m_data->Objects.GetObjects();
		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();

		ImGui::BeginChild("##object_scroll_container", containerSize);

		if (items.size() == 0)
			ImGui::TextWrapped("Right click on this window or go to Create menu in the menu bar to create an item.");

		for (int i = 0; i < items.size(); i++) {
			bool isPluginOwner = m_data->Objects.IsPluginObject(items[i]);

			GLuint tex = m_data->Objects.GetTexture(items[i]);
			if (m_data->Objects.IsImage(items[i]))
				tex = m_data->Objects.GetImage(items[i])->Texture;
			else if (m_data->Objects.IsImage3D(items[i]))
				tex = m_data->Objects.GetImage3D(items[i])->Texture;

			float imgWH = 0.0f;
			glm::vec2 imgSize(0,0);
			if (m_data->Objects.IsRenderTexture(items[i])) {
				glm::ivec2 rtSize = m_data->Objects.GetRenderTextureSize(items[i]);
				imgWH = (float)rtSize.y / rtSize.x;
				imgSize = glm::vec2(rtSize.x, rtSize.y);
			}
			else if (m_data->Objects.IsAudio(items[i])) {
				imgWH = 2.0f / 512.0f;
				imgSize = glm::vec2(512, 2);
			}
			else if (m_data->Objects.IsCubeMap(items[i])) {
				imgWH = 375.0f / 512.0f;
				imgSize = glm::vec2(512, 375);
			}
			else if (m_data->Objects.IsImage(items[i])) {
				imgSize = m_data->Objects.GetImageSize(items[i]);
				imgWH = imgSize.y / imgSize.x;
			}
			else if (m_data->Objects.IsImage3D(items[i])) {
				imgSize = m_data->Objects.GetImage3DSize(items[i]);
				imgWH = imgSize.y / imgSize.x;
			}
			else if (!isPluginOwner) {
				auto img = m_data->Objects.GetTextureSize(items[i]);
				imgWH = (float)img.y / img.x;
				imgSize = glm::vec2(img);
			}

			size_t lastSlash = items[i].find_last_of("/\\");
			std::string itemText = items[i];

			if (lastSlash != std::string::npos && !isPluginOwner)
				itemText = itemText.substr(lastSlash + 1);

			ImGui::Selectable(itemText.c_str());

			if (ImGui::BeginPopupContextItem(std::string("##context" + items[i]).c_str())) {
				itemMenuOpened = true;

				PluginObject* pobj = m_data->Objects.GetPluginObject(items[i]);

				bool isBuf = m_data->Objects.IsBuffer(items[i]);
				bool isImg3D = m_data->Objects.IsImage3D(items[i]);
				bool hasPluginExtendedPreview = isPluginOwner && pobj->Owner->HasObjectExtendedPreview(pobj->Type);
				if ((hasPluginExtendedPreview || !isPluginOwner) && !isImg3D && (isBuf ? ImGui::Selectable("Edit") : ImGui::Selectable("Preview"))) {
					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Open(items[i], imgSize.x, imgSize.y, tex,
							m_data->Objects.IsCubeMap(items[i]),
							m_data->Objects.IsRenderTexture(items[i]) ? m_data->Objects.GetRenderTexture(tex) : nullptr,
							m_data->Objects.IsAudio(items[i]) ? m_data->Objects.GetSoundBuffer(items[i]) : nullptr,
							isBuf ? m_data->Objects.GetBuffer(items[i]) : nullptr,
							isPluginOwner ? pobj : nullptr);
				}

				bool hasPluginPreview = isPluginOwner && pobj->Owner->HasObjectPreview(pobj->Type);
				if (m_data->Objects.IsCubeMap(items[i])) {
					m_cubePrev.Draw(tex);
					ImGui::Image((void*)(intptr_t)m_cubePrev.GetTexture(), ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH)* IMAGE_CONTEXT_WIDTH), ImVec2(0,1), ImVec2(1,0));
				} else if (!isBuf && !isImg3D && !isPluginOwner)
					ImGui::Image((void*)(intptr_t)tex, ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH)* IMAGE_CONTEXT_WIDTH), ImVec2(0,1), ImVec2(1,0));
				else if (hasPluginPreview)
					pobj->Owner->ShowObjectPreview(pobj->Type, pobj->Data, pobj->ID);

				ImGui::Separator();

				if (m_data->Objects.IsImage(items[i]) || isImg3D) {
					if (ImGui::BeginMenu(isImg3D ? "Bind UAV/image3D" : "Bind UAV/image2D")) {
						for (int j = 0; j < passes.size(); j++) {
							if (passes[j]->Type == PipelineItem::ItemType::ComputePass) {
								int boundID = m_data->Objects.IsUniformBound(items[i], passes[j]);
								size_t boundItemCount = m_data->Objects.GetUniformBindList(passes[j]).size();
								bool isBound = boundID != -1;
								if (ImGui::MenuItem(passes[j]->Name, ("(" + std::to_string(boundID == -1 ? boundItemCount : boundID) + ")").c_str(), isBound)) {
									if (!isBound)
										m_data->Objects.BindUniform(items[i], passes[j]);
									else
										m_data->Objects.UnbindUniform(items[i], passes[j]);
								}
							}
						}
						ImGui::EndMenu();
					}
				}

				bool isPluginObjBindable = isPluginOwner && (pobj->Owner->IsObjectBindable(pobj->Type) || pobj->Owner->IsObjectBindableUAV(pobj->Type));
				if ((isPluginObjBindable || !isPluginOwner) && ImGui::BeginMenu("Bind")) {
					bool isPluginObjUAV = isPluginOwner && pobj->Owner->IsObjectBindableUAV(pobj->Type);

					if (isBuf || isPluginObjUAV) {
						for (int j = 0; j < passes.size(); j++) {
							int boundID = m_data->Objects.IsUniformBound(items[i], passes[j]);
							size_t boundItemCount = m_data->Objects.GetUniformBindList(passes[j]).size();
							bool isBound = boundID != -1;
							if (ImGui::MenuItem(passes[j]->Name, ("(" + std::to_string(boundID == -1 ? boundItemCount : boundID) + ")").c_str(), isBound)) {
								if (!isBound)
									m_data->Objects.BindUniform(items[i], passes[j]);
								else
									m_data->Objects.UnbindUniform(items[i], passes[j]);
							}
						}
					} else {
						for (int j = 0; j < passes.size(); j++) {
							int boundID = m_data->Objects.IsBound(items[i], passes[j]);
							size_t boundItemCount = m_data->Objects.GetBindList(passes[j]).size();
							bool isBound = boundID != -1;
							if (ImGui::MenuItem(passes[j]->Name, ("(" + std::to_string(boundID == -1 ? boundItemCount : boundID) + ")").c_str(), isBound)) {
								if (!isBound)
									m_data->Objects.Bind(items[i], passes[j]);
								else
									m_data->Objects.Unbind(items[i], passes[j]);
							}
						}
					}
					ImGui::EndMenu();
				}

				bool hasPluginContext = isPluginOwner && pobj->Owner->HasObjectContext(pobj->Type);
				if (hasPluginContext) {
					ImGui::Separator();
					pobj->Owner->ShowObjectContext(pobj->Type, pobj->Data);
					ImGui::Separator();
				}

				if (m_data->Objects.IsRenderTexture(items[i]) ||
					m_data->Objects.IsImage(items[i]) ||
					isImg3D ||
					(isPluginOwner && pobj->Owner->HasObjectProperties(pobj->Type)))
				{
					if (ImGui::Selectable("Properties"))
						((ed::PropertyUI *)m_ui->Get(ViewID::Properties))->Open(items[i], m_data->Objects.GetObjectManagerItem(items[i]));
				}

				if (m_data->Objects.IsAudio(items[i])) {
					bool isMuted = m_data->Objects.IsAudioMuted(items[i]);
					if (ImGui::MenuItem("Mute", (const char*)0, &isMuted)) {
						if (isMuted)
							m_data->Objects.Mute(items[i]);
						else
							m_data->Objects.Unmute(items[i]);
					}
				}

				if (ImGui::Selectable("Delete")) {
					if (m_data->Objects.IsRenderTexture(items[i])) {
						auto& passes = m_data->Pipeline.GetList();
						for (int j = 0; j < passes.size(); j++) {
							if (passes[i]->Type != PipelineItem::ItemType::ShaderPass)
								continue;

							pipe::ShaderPass* sData = (pipe::ShaderPass*)passes[j]->Data;

							// check if shader pass uses this rt
							for (int k = 0; k < MAX_RENDER_TEXTURES; k++) {
								if (tex == sData->RenderTextures[k]) {
									// TODO: maybe implement better logic for what to replace the deleted RT with
									bool alreadyUsesWindow = false;
									for (int l = 0; l < MAX_RENDER_TEXTURES; l++)
										if (sData->RenderTextures[l] == m_data->Renderer.GetTexture()) {
											alreadyUsesWindow = true;
											break;
										}

									if (!alreadyUsesWindow)
										sData->RenderTextures[k] = m_data->Renderer.GetTexture();
									else if (k != 0) {
										for (int l = j; l < MAX_RENDER_TEXTURES; l++)
											sData->RenderTextures[l] = 0;
									}
									else {
										for (int l = 0; l < MAX_RENDER_TEXTURES; l++)
											sData->RenderTextures[l] = 0;
										sData->RenderTextures[0] = m_data->Renderer.GetTexture();
									}
								}
							}
						}
					}

					if (isBuf) {
						auto& passes = m_data->Pipeline.GetList();
						for (int j = 0; j < passes.size(); j++) {
							if (passes[i]->Type != PipelineItem::ItemType::ShaderPass)
								continue;

							pipe::ShaderPass* pdata = (pipe::ShaderPass*)passes[j]->Data;
							for (int k = 0; k < pdata->Items.size(); k++) {
								PipelineItem* pitem = pdata->Items[k];
								if (pitem->Type == ed::PipelineItem::ItemType::Geometry) {
									pipe::GeometryItem* gitem = (pipe::GeometryItem*)pitem->Data;

									if (gitem->InstanceBuffer == m_data->Objects.GetBuffer(items[i]))
										gl::CreateVAO(gitem->VAO, gitem->VBO, pdata->InputLayout);
									gitem->InstanceBuffer = nullptr;
								}
								else if (pitem->Type == ed::PipelineItem::ItemType::Model) {
									pipe::Model* mitem = (pipe::Model*)pitem->Data;

									if (mitem->InstanceBuffer == m_data->Objects.GetBuffer(items[i])) {
										for (auto& mesh : mitem->Data->Meshes)
											gl::CreateVAO(mesh.VAO, mesh.VBO, pdata->InputLayout, mesh.EBO);
										mitem->InstanceBuffer = nullptr;
									}
								}
							}
						}
					}

					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Close(items[i]);
					
					PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
					if (props->CurrentItemName() == itemText)
						props->Open(nullptr); // TODO: test this, deleting RT while having sth opened in properties
					m_data->Objects.Remove(items[i]);
				}

				ImGui::EndPopup();
			}
		}

		ImGui::EndChild();

		if (!itemMenuOpened && ImGui::BeginPopupContextItem("##context_main_objects")) {
			if (ImGui::Selectable("Create Texture")) { m_ui->CreateNewTexture(); }
			if (ImGui::Selectable("Create Cubemap")) { m_ui->CreateNewCubemap(); }
			if (ImGui::Selectable("Create Render Texture")) { m_ui->CreateNewRenderTexture(); }
			if (ImGui::Selectable("Create Audio")) { m_ui->CreateNewAudio(); }
			if (ImGui::Selectable("Create Buffer")) { m_ui->CreateNewBuffer(); }
			if (ImGui::Selectable("Create Empty Image")) m_ui->CreateNewImage();
			if (ImGui::Selectable("Create Empty 3D Image")) m_ui->CreateNewImage3D();

			m_data->Plugins.ShowContextItems("objects");

			ImGui::EndPopup();
		}
	}
}