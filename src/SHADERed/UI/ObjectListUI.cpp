#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/UI/ObjectListUI.h>
#include <SHADERed/UI/ObjectPreviewUI.h>
#include <SHADERed/UI/PropertyUI.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define IMAGE_CONTEXT_WIDTH Settings::Instance().CalculateSize(150)

namespace ed {
	void ObjectListUI::OnEvent(const SDL_Event& e)
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
			ImGui::TextWrapped("Right click here or go to Project -> Create menu in the menu bar to create an item.");

		for (int i = 0; i < items.size(); i++) {
			ObjectManagerItem* oItem = m_data->Objects.GetObjectManagerItem(items[i]);
			bool isPluginOwner = oItem->Plugin != nullptr;

			GLuint tex = oItem->Texture;
			if (oItem->Image != nullptr)
				tex = oItem->Image->Texture;
			else if (oItem->Image3D != nullptr)
				tex = oItem->Image3D->Texture;

			float imgWH = 0.0f;
			glm::vec2 imgSize(0, 0);
			if (oItem->RT != nullptr) {
				glm::ivec2 rtSize = m_data->Objects.GetRenderTextureSize(items[i]);
				imgWH = (float)rtSize.y / rtSize.x;
				imgSize = glm::vec2(rtSize.x, rtSize.y);
			} else if (oItem->Sound != nullptr) {
				imgWH = 2.0f / 512.0f;
				imgSize = glm::vec2(512, 2);
			} else if (oItem->IsCube) {
				imgWH = 375.0f / 512.0f;
				imgSize = glm::vec2(512, 375);
			} else if (oItem->Image != nullptr) {
				imgSize = oItem->Image->Size;
				imgWH = imgSize.y / imgSize.x;
			} else if (oItem->Image3D != nullptr) {
				imgSize = oItem->Image3D->Size;
				imgWH = imgSize.y / imgSize.x;
			} else if (!isPluginOwner) {
				auto img = oItem->ImageSize;
				imgWH = (float)img.y / img.x;
				imgSize = glm::vec2(img);
			}

			std::string itemText = items[i];
			std::string fullItemText = items[i];

			if (oItem->IsTexture || oItem->Sound != nullptr) {
				size_t lastSlash = items[i].find_last_of("/\\");

				if (lastSlash != std::string::npos && !isPluginOwner)
					itemText = itemText.substr(lastSlash + 1);
			}

			PluginObject* pobj = oItem->Plugin;

			bool isBuf = oItem->Buffer != nullptr;
			bool isImg3D = oItem->Image3D != nullptr;
			bool hasPluginExtendedPreview = isPluginOwner && pobj->Owner->HasObjectExtendedPreview(pobj->Type);
			
			if (ImGui::Selectable(itemText.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
				// open preview on double click
				if (ImGui::IsMouseDoubleClicked(0) && (hasPluginExtendedPreview || !isPluginOwner) && !isImg3D)
					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Open(items[i], imgSize.x, imgSize.y, tex, m_data->Objects.IsCubeMap(items[i]), m_data->Objects.IsRenderTexture(items[i]) ? m_data->Objects.GetRenderTexture(tex) : nullptr, m_data->Objects.IsAudio(items[i]) ? m_data->Objects.GetSoundBuffer(items[i]) : nullptr, isBuf ? m_data->Objects.GetBuffer(items[i]) : nullptr, isPluginOwner ? pobj : nullptr);
			}

			if (ImGui::BeginPopupContextItem(std::string("##context" + items[i]).c_str())) {
				itemMenuOpened = true;

				if ((hasPluginExtendedPreview || !isPluginOwner) && !isImg3D && (isBuf ? ImGui::Selectable("Edit") : ImGui::Selectable("Preview"))) {
					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Open(items[i], imgSize.x, imgSize.y, tex, m_data->Objects.IsCubeMap(items[i]), m_data->Objects.IsRenderTexture(items[i]) ? m_data->Objects.GetRenderTexture(tex) : nullptr, m_data->Objects.IsAudio(items[i]) ? m_data->Objects.GetSoundBuffer(items[i]) : nullptr, isBuf ? m_data->Objects.GetBuffer(items[i]) : nullptr, isPluginOwner ? pobj : nullptr);
				}

				bool hasPluginPreview = isPluginOwner && pobj->Owner->HasObjectPreview(pobj->Type);
				if (oItem->IsCube) {
					m_cubePrev.Draw(tex);
					ImGui::Image((void*)(intptr_t)m_cubePrev.GetTexture(), ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH), ImVec2(0, 1), ImVec2(1, 0));
				} else if (!isBuf && !isImg3D && !isPluginOwner)
					ImGui::Image((void*)(intptr_t)tex, ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH), ImVec2(0, 1), ImVec2(1, 0));
				else if (hasPluginPreview)
					pobj->Owner->ShowObjectPreview(pobj->Type, pobj->Data, pobj->ID);

				ImGui::Separator();

				if (oItem->Image != nullptr || isImg3D) {
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

				if (oItem->RT != nullptr || oItem->Image != nullptr || (oItem->IsTexture && !oItem->IsKeyboardTexture) || isImg3D || (isPluginOwner && pobj->Owner->HasObjectProperties(pobj->Type))) {
					if (ImGui::Selectable("Properties"))
						((ed::PropertyUI*)m_ui->Get(ViewID::Properties))->Open(items[i], m_data->Objects.GetObjectManagerItem(items[i]));
				}

				if (oItem->Sound != nullptr) {
					bool isMuted = oItem->SoundMuted;
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
									} else {
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
								} else if (pitem->Type == ed::PipelineItem::ItemType::Model) {
									pipe::Model* mitem = (pipe::Model*)pitem->Data;

									if (mitem->InstanceBuffer == m_data->Objects.GetBuffer(items[i])) {
										for (auto& mesh : mitem->Data->Meshes)
											gl::CreateVAO(mesh.VAO, mesh.VBO, pdata->InputLayout, mesh.EBO);
										mitem->InstanceBuffer = nullptr;
									}
								} else if (pitem->Type == ed::PipelineItem::ItemType::VertexBuffer) {
									pipe::VertexBuffer* vbitem = (pipe::VertexBuffer*)pitem->Data;

									if (vbitem->Buffer == m_data->Objects.GetBuffer(items[i])) {
										vbitem->Buffer = nullptr;
										glDeleteVertexArrays(1, &vbitem->VAO);
										vbitem->VAO = 0;
									}
								}
							}
						}
					}

					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Close(items[i]);

					PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
					if (props->CurrentItemName() == fullItemText)
						props->Open(nullptr);
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
			
			bool hasKBTexture = m_data->Objects.HasKeyboardTexture();
			if (hasKBTexture) {
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::Selectable("Create KeyboardTexture")) m_ui->CreateKeyboardTexture();
			if (hasKBTexture) {
				ImGui::PopStyleVar();
				ImGui::PopItemFlag();
			}

			m_data->Plugins.ShowContextItems("objects");

			ImGui::EndPopup();
		}
	}
}