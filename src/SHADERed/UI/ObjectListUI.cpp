#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/UI/ObjectListUI.h>
#include <SHADERed/UI/ObjectPreviewUI.h>
#include <SHADERed/UI/PropertyUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <misc/ImFileDialog.h>

#define IMAGE_CONTEXT_WIDTH Settings::Instance().CalculateSize(150)

namespace ed {
	void ObjectListUI::OnEvent(const SDL_Event& e)
	{
	}
	void ObjectListUI::Update(float delta)
	{
		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		bool itemMenuOpened = false;
		std::vector<ObjectManagerItem*>& items = m_data->Objects.GetObjects();
		const std::vector<PipelineItem*>& passes = m_data->Pipeline.GetList();

		ImGui::BeginChild("##object_scroll_container", containerSize);

		if (items.size() == 0)
			ImGui::TextWrapped("Right click here or go to Project -> Create menu in the menu bar to create an item.");

		for (int i = 0; i < items.size(); i++) {
			ObjectManagerItem* oItem = items[i];
			bool isPluginOwner = (oItem->Type == ObjectType::PluginObject);

			GLuint tex = oItem->Texture;

			float imgWH = 0.0f;
			if (oItem->Type == ObjectType::RenderTexture) {
				glm::ivec2 rtSize = m_data->Objects.GetRenderTextureSize(oItem);
				imgWH = (float)rtSize.y / rtSize.x;
			} else if (oItem->Type == ObjectType::Audio)
				imgWH = 2.0f / 512.0f;
			else if (oItem->Type == ObjectType::CubeMap)
				imgWH = 375.0f / 512.0f;
			else if (oItem->Type == ObjectType::Image)
				imgWH = oItem->Image->Size.y / (float)oItem->Image->Size.x;
			else if (oItem->Type == ObjectType::Image3D)
				imgWH = oItem->Image3D->Size.y / (float)oItem->Image3D->Size.x;
			else if (oItem->Type != ObjectType::PluginObject) 
				imgWH = (float)oItem->TextureSize.y / oItem->TextureSize.x;

			std::string itemText = oItem->Name;
			std::string fullItemText = itemText;

			if (oItem->Type == ObjectType::Texture || oItem->Type == ObjectType::Texture3D || oItem->Type == ObjectType::Audio) {
				size_t lastSlash = oItem->Name.find_last_of("/\\");

				if (lastSlash != std::string::npos && !isPluginOwner)
					itemText = itemText.substr(lastSlash + 1);
			}

			PluginObject* pobj = oItem->Plugin;

			bool isBuf = (oItem->Type == ObjectType::Buffer);
			bool isImg3D = (oItem->Type == ObjectType::Image3D);
			bool hasPluginExtendedPreview = isPluginOwner && pobj->Owner->Object_HasExtendedPreview(pobj->Type);
			
			if (ImGui::Selectable(itemText.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
				// open preview on double click
				if (ImGui::IsMouseDoubleClicked(0) && (hasPluginExtendedPreview || !isPluginOwner) && !isImg3D)
					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Open(oItem);
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				ImGui::SetDragDropPayload("ObjectPayload", &oItem, sizeof(ed::ObjectManagerItem**));

				ImGui::TextUnformatted(itemText.c_str());
				bool hasPluginPreview = isPluginOwner && pobj->Owner->Object_HasPreview(pobj->Type);
				if (oItem->Type == ObjectType::CubeMap) {
					m_cubePrev.Draw(tex);
					ImGui::Image((void*)(intptr_t)m_cubePrev.GetTexture(), ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH), ImVec2(0, 1), ImVec2(1, 0));
				} else if (!isBuf && !isImg3D && !isPluginOwner)
					ImGui::Image((void*)(intptr_t)tex, ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH), ImVec2(0, 1), ImVec2(1, 0));
				else if (hasPluginPreview)
					pobj->Owner->Object_ShowPreview(pobj->Type, pobj->Data, pobj->ID);

				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginPopupContextItem(std::string("##context" + items[i]->Name).c_str())) {
				itemMenuOpened = true;

				if ((hasPluginExtendedPreview || !isPluginOwner) && (isBuf ? ImGui::Selectable("Edit") : ImGui::Selectable("Preview")))
					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Open(oItem);

				bool hasPluginPreview = isPluginOwner && pobj->Owner->Object_HasPreview(pobj->Type);
				if (oItem->Type == ObjectType::CubeMap) {
					m_cubePrev.Draw(tex);
					ImGui::Image((void*)(intptr_t)m_cubePrev.GetTexture(), ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH), ImVec2(0, 1), ImVec2(1, 0));
				} 
				else if (oItem->Type == ObjectType::Texture3D || oItem->Type == ObjectType::Image3D) {
					m_tex3DPrev.Draw(tex, IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH);
					ImGui::Image((void*)(intptr_t)m_tex3DPrev.GetTexture(), ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH));
				}
				else if (!isBuf && !isImg3D && !isPluginOwner)
					ImGui::Image((void*)(intptr_t)tex, ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH) * IMAGE_CONTEXT_WIDTH), ImVec2(0, 1), ImVec2(1, 0));
				else if (hasPluginPreview)
					pobj->Owner->Object_ShowPreview(pobj->Type, pobj->Data, pobj->ID);

				ImGui::Separator();

				if (oItem->Image != nullptr || isImg3D) {
					if (ImGui::BeginMenu(isImg3D ? "Bind UAV/image3D" : "Bind UAV/image2D")) {
						for (int j = 0; j < passes.size(); j++) {
							if (passes[j]->Type == PipelineItem::ItemType::ComputePass) {
								int boundID = m_data->Objects.IsUniformBound(oItem, passes[j]);
								size_t boundItemCount = m_data->Objects.GetUniformBindList(passes[j]).size();
								bool isBound = boundID != -1;
								if (ImGui::MenuItem(passes[j]->Name, ("(" + std::to_string(boundID == -1 ? boundItemCount : boundID) + ")").c_str(), isBound)) {
									if (!isBound)
										m_data->Objects.BindUniform(oItem, passes[j]);
									else
										m_data->Objects.UnbindUniform(oItem, passes[j]);
								}
							}
						}
						ImGui::EndMenu();
					}
				}

				bool isPluginObjBindable = isPluginOwner && (pobj->Owner->Object_IsBindable(pobj->Type) || pobj->Owner->Object_IsBindableUAV(pobj->Type));
				if ((isPluginObjBindable || !isPluginOwner) && ImGui::BeginMenu("Bind")) {
					bool isPluginObjUAV = isPluginOwner && pobj->Owner->Object_IsBindableUAV(pobj->Type);

					if (isBuf || isPluginObjUAV) {
						for (int j = 0; j < passes.size(); j++) {
							int boundID = m_data->Objects.IsUniformBound(oItem, passes[j]);
							size_t boundItemCount = m_data->Objects.GetUniformBindList(passes[j]).size();
							bool isBound = boundID != -1;
							if (ImGui::MenuItem(passes[j]->Name, ("(" + std::to_string(boundID == -1 ? boundItemCount : boundID) + ")").c_str(), isBound)) {
								if (!isBound)
									m_data->Objects.BindUniform(oItem, passes[j]);
								else
									m_data->Objects.UnbindUniform(oItem, passes[j]);
							}
						}
					} else {
						for (int j = 0; j < passes.size(); j++) {
							int boundID = m_data->Objects.IsBound(oItem, passes[j]);
							size_t boundItemCount = m_data->Objects.GetBindList(passes[j]).size();
							bool isBound = boundID != -1;
							if (ImGui::MenuItem(passes[j]->Name, ("(" + std::to_string(boundID == -1 ? boundItemCount : boundID) + ")").c_str(), isBound)) {
								if (!isBound)
									m_data->Objects.Bind(oItem, passes[j]);
								else
									m_data->Objects.Unbind(oItem, passes[j]);
							}
						}
					}
					ImGui::EndMenu();
				}

				bool hasPluginContext = isPluginOwner && pobj->Owner->Object_HasContext(pobj->Type);
				if (hasPluginContext) {
					ImGui::Separator();
					pobj->Owner->Object_ShowContext(pobj->Type, pobj->Data);
					ImGui::Separator();
				}

				if (oItem->Type == ObjectType::RenderTexture || oItem->Type == ObjectType::Image || oItem->Type == ObjectType::Texture || oItem->Type == ObjectType::Texture3D || isImg3D || (isPluginOwner && pobj->Owner->Object_HasProperties(pobj->Type))) {
					if (ImGui::Selectable("Properties"))
						((ed::PropertyUI*)m_ui->Get(ViewID::Properties))->Open(oItem);
				}

				if (oItem->Type == ObjectType::RenderTexture || oItem->Type == ObjectType::Image || oItem->Type == ObjectType::Texture) {
					if (ImGui::Selectable("Save")) {
						ifd::FileDialog::Instance().Save("SaveTextureDlg", "Save", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*");
						m_saveObject = oItem;
					}
				}

				if (oItem->Sound != nullptr) {
					bool isMuted = oItem->SoundMuted;
					if (ImGui::MenuItem("Mute", (const char*)0, &isMuted)) {
						if (isMuted)
							m_data->Objects.Mute(oItem);
						else
							m_data->Objects.Unmute(oItem);
					}
				}

				if (ImGui::Selectable("Delete")) {
					if (oItem->Type == ObjectType::RenderTexture) {
						auto& passes = m_data->Pipeline.GetList();
						for (int j = 0; j < passes.size(); j++) {
							if (passes[j]->Type != PipelineItem::ItemType::ShaderPass)
								continue;

							pipe::ShaderPass* sData = (pipe::ShaderPass*)passes[j]->Data;

							// check if this shader pass has a window bound to it
							bool alreadyUsesWindow = false;
							for (int l = 0; l < MAX_RENDER_TEXTURES; l++)
								if (sData->RenderTextures[l] == m_data->Renderer.GetTexture()) {
									alreadyUsesWindow = true;
									break;
								}

							// move everything
							for (int k = 0; k < MAX_RENDER_TEXTURES; k++) {
								if (tex == sData->RenderTextures[k]) {
									if (sData->RTCount == 1 && k == 0)
										sData->RenderTextures[k] = m_data->Renderer.GetTexture();
									else {
										for (int l = k; l < sData->RTCount - 1; l++)
											sData->RenderTextures[l] = sData->RenderTextures[l + 1];
										sData->RenderTextures[sData->RTCount - 1] = 0;
										sData->RTCount--;
									}
								}
							}
						}
					}

					if (isBuf) {
						auto& passes = m_data->Pipeline.GetList();
						for (int j = 0; j < passes.size(); j++) {
							if (passes[j]->Type != PipelineItem::ItemType::ShaderPass)
								continue;

							pipe::ShaderPass* pdata = (pipe::ShaderPass*)passes[j]->Data;
							for (int k = 0; k < pdata->Items.size(); k++) {
								PipelineItem* pitem = pdata->Items[k];
								if (pitem->Type == ed::PipelineItem::ItemType::Geometry) {
									pipe::GeometryItem* gitem = (pipe::GeometryItem*)pitem->Data;

									if (gitem->InstanceBuffer == (void*)oItem->Buffer)
										gl::CreateVAO(gitem->VAO, gitem->VBO, pdata->InputLayout);
									gitem->InstanceBuffer = nullptr;
								} else if (pitem->Type == ed::PipelineItem::ItemType::Model) {
									pipe::Model* mitem = (pipe::Model*)pitem->Data;

									if (mitem->InstanceBuffer == (void*)oItem->Buffer) {
										for (auto& mesh : mitem->Data->Meshes)
											gl::CreateVAO(mesh.VAO, mesh.VBO, pdata->InputLayout, mesh.EBO);
										mitem->InstanceBuffer = nullptr;
									}
								} else if (pitem->Type == ed::PipelineItem::ItemType::VertexBuffer) {
									pipe::VertexBuffer* vbitem = (pipe::VertexBuffer*)pitem->Data;

									if (vbitem->Buffer == (void*)oItem->Buffer) {
										vbitem->Buffer = nullptr;
										glDeleteVertexArrays(1, &vbitem->VAO);
										vbitem->VAO = 0;
									}
								}
							}
						}
					}

					((ObjectPreviewUI*)m_ui->Get(ViewID::ObjectPreview))->Close(oItem->Name);

					PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
					if (props->CurrentItemName() == fullItemText)
						props->Close();
					m_data->Objects.Remove(oItem->Name);
				}

				ImGui::EndPopup();
			}
		}

		ImGui::Dummy(ImVec2(1, 32));

		ImGui::EndChild();

		if (ifd::FileDialog::Instance().IsDone("SaveTextureDlg")) {
			if (ifd::FileDialog::Instance().HasResult() && m_saveObject)
				m_data->Objects.SaveToFile(m_saveObject, ifd::FileDialog::Instance().GetResult().u8string());
			ifd::FileDialog::Instance().Close();
		}

		if (!itemMenuOpened && ImGui::BeginPopupContextItem("##context_main_objects")) {
			if (ImGui::Selectable("Create Texture")) m_ui->CreateNewTexture();
			if (ImGui::Selectable("Create Texture 3D")) m_ui->CreateNewTexture3D();
			if (ImGui::Selectable("Create Cubemap")) m_ui->CreateNewCubemap();
			if (ImGui::Selectable("Create Render Texture")) m_ui->CreateNewRenderTexture();
			if (ImGui::Selectable("Create Audio")) m_ui->CreateNewAudio();
			if (ImGui::Selectable("Create Buffer")) m_ui->CreateNewBuffer();
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