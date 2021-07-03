#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/PropertyUI.h>
#include <SHADERed/UI/UIHelper.h>

#include <misc/ImFileDialog.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/common.hpp>

#define BUTTON_SPACE_LEFT Settings::Instance().CalculateSize(-40)
#define REFRESH_BUTTON_SPACE_LEFT Settings::Instance().CalculateSize(-70)
#define HARRAYSIZE(a) (sizeof(a) / sizeof(*a))

namespace ed {
	void PropertyUI::m_init()
	{
		m_current = nullptr;
		m_currentObj = nullptr;
		m_dialogPath = nullptr;
		memset(m_itemName, 0, 64 * sizeof(char));
	}
	void PropertyUI::OnEvent(const SDL_Event& e)
	{
	}
	void PropertyUI::Update(float delta)
	{
		if (m_current != nullptr || m_currentObj != nullptr) {
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
			if (m_current != nullptr) {
				ImGui::Text(m_current->Name);
				ImGui::Text(PIPELINE_ITEM_NAMES[(int)m_current->Type]);
				if (m_current->Type == PipelineItem::ItemType::Geometry) {
					ImGui::SameLine();
					ImGui::Text("(%s)", GEOMETRY_NAMES[(int)((pipe::GeometryItem*)m_current->Data)->Type]);
				} else if (m_current->Type == PipelineItem::ItemType::PluginItem) {
					ImGui::SameLine();
					ImGui::Text("(%s)", ((pipe::PluginItemData*)m_current->Data)->Type);
				}
			} else if (m_currentObj != nullptr) {
				if (IsTexture() || IsTexture3D())
					ImGui::TextUnformatted(std::filesystem::path(m_currentObj->Name).filename().generic_u8string().c_str());
				else
					ImGui::TextUnformatted(m_currentObj->Name.c_str());
				if (IsRenderTexture())
					ImGui::Text("Render Texture");
				else if (IsTexture())
					ImGui::Text("Texture");
				else if (IsImage())
					ImGui::Text("Image");
				else if (IsImage3D())
					ImGui::Text("Image3D");
				else if (IsTexture3D())
					ImGui::Text("Texture3D");
				else if (IsPlugin())
					ImGui::Text(m_currentObj->Plugin->Type);
				else
					ImGui::Text("ObjectManagerItem");
			} else
				ImGui::Text("nullptr");

			ImGui::PopStyleColor();

			ImGui::Columns(2, "##content_columns");

			// TODO: this is only a temprorary fix for non-resizable columns
			static bool isColumnWidthSet = false;
			if (!isColumnWidthSet) {
				ImGui::SetColumnWidth(0, ImGui::GetWindowSize().x * 0.3f);
				isColumnWidthSet = true;
			}

			ImGui::Separator();

			if (m_currentObj == nullptr) {
				ImGui::Text("Name:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(Settings::Instance().CalculateSize(-40));
				ImGui::InputText("##pui_itemname", m_itemName, PIPELINE_ITEM_NAME_LENGTH);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("Ok##pui_itemname", ImVec2(-1, 0))) {
					if (m_data->Pipeline.Has(m_itemName))
						ImGui::OpenPopup("ERROR##pui_itemname_taken");
					else if (strlen(m_itemName) <= 2)
						ImGui::OpenPopup("ERROR##pui_itemname_short");
					else if (m_current != nullptr) {
						if (m_current->Type == PipelineItem::ItemType::PluginItem) {
							pipe::PluginItemData* pdata = (pipe::PluginItemData*)m_current->Data;
							pdata->Owner->PipelineItem_Rename(m_current->Name, m_itemName);
						}

						m_data->Messages.RenameGroup(m_current->Name, m_itemName);
						memcpy(m_current->Name, m_itemName, PIPELINE_ITEM_NAME_LENGTH);
						m_data->Parser.ModifyProject();
					}
				}
				ImGui::NextColumn();
				ImGui::Separator();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("ERROR##pui_itemname_taken", 0, ImGuiWindowFlags_NoResize)) {
				ImGui::Text("There is already an item with name \"%s\"", m_itemName);
				if (ImGui::Button("Ok")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			if (ImGui::BeginPopupModal("ERROR##pui_itemname_short", 0, ImGuiWindowFlags_NoResize)) {
				ImGui::Text("The name must be at least 3 characters long.");
				if (ImGui::Button("Ok")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar();

			if (m_current != nullptr) {
				if (m_current->Type == ed::PipelineItem::ItemType::ShaderPass) {
					ed::pipe::ShaderPass* item = reinterpret_cast<ed::pipe::ShaderPass*>(m_current->Data);

					/* Render Texture */
					ImGui::Text("RT:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					for (item->RTCount = 0; item->RTCount < MAX_RENDER_TEXTURES; item->RTCount++) {
						int i = item->RTCount;
						GLuint rtID = item->RenderTextures[i];
						std::string name = rtID == 0 ? "NULL" : (rtID == m_data->Renderer.GetTexture() ? "Window" : m_data->Objects.GetByTextureID(rtID)->Name);

						ImGui::PushID(i);
						if (ImGui::BeginCombo("##pui_rt_combo", name.c_str())) {
							std::vector<ObjectManagerItem*> rts = m_data->Objects.GetObjects();
							bool windowAlreadyBound = false;
							for (int k = 0; k < MAX_RENDER_TEXTURES; k++) {
								if (item->RenderTextures[k] == 0)
									continue;

								if (!windowAlreadyBound && item->RenderTextures[k] == m_data->Renderer.GetTexture()) {
									windowAlreadyBound = true;
									break;
								}

								for (int j = 0; j < rts.size(); j++) {
									if (rts[j]->Type != ObjectType::RenderTexture || (rts[j]->Type == ObjectType::RenderTexture && item->RenderTextures[k] == rts[j]->Texture)) {
										rts.erase(rts.begin() + j);
										j--;
									}
								}
							}

							// prevent duplicates
							for (int j = 0; j < MAX_RENDER_TEXTURES; j++) {
								if (item->RenderTextures[j] == 0)
									break;

								std::string name = item->RenderTextures[j] == m_data->Renderer.GetTexture() ? "Window" : m_data->Objects.GetByTextureID(item->RenderTextures[j])->Name;

								for (int k = 0; k < rts.size(); k++) {
									if (rts[k]->Name == name) {
										rts.erase(rts.begin() + k);
										k--;
									}
								}
							}

							// null element
							if (i != 0 && rtID != 0) {
								if (ImGui::Selectable("NULL", rtID == 0)) {
									m_data->Parser.ModifyProject();
									item->RenderTextures[i] = 0;
									for (int j = i + 1; j < MAX_RENDER_TEXTURES; j++)
										item->RenderTextures[j] = 0;
								}
							}

							// window element
							if (!windowAlreadyBound && i == 0)
								if (ImGui::Selectable("Window", rtID == m_data->Renderer.GetTexture())) {
									m_data->Parser.ModifyProject();
									item->RenderTextures[i] = m_data->Renderer.GetTexture();
									for (int j = 1; j < MAX_RENDER_TEXTURES; j++) // "Window" RT can only be used solo
										item->RenderTextures[j] = 0;
								}

							// users RTs
							for (int j = 0; j < rts.size(); j++)
								if (rts[j]->Type == ObjectType::RenderTexture) {
									GLuint texID = rts[j]->Texture;
									if (ImGui::Selectable(rts[j]->Name.c_str(), rtID == texID)) {
										m_data->Parser.ModifyProject();
										item->RenderTextures[i] = texID;
									}
								}

							ImGui::EndCombo();
						}
						ImGui::PopID();

						if (item->RenderTextures[i] == 0)
							break;
					}
					ImGui::PopItemWidth();
					ImGui::NextColumn();

					ImGui::Separator();

					/* vertex shader path */
					ImGui::Text("VS Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_vspath", item->VSPath, SHADERED_MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_vsbtn", ImVec2(-1, 0))) {
						m_dialogPath = item->VSPath;
						m_dialogShaderType = "Vertex";
						ifd::FileDialog::Instance().Open("PropertyShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
					}
					ImGui::NextColumn();

					ImGui::Separator();

					/* vertex shader entry */
					ImGui::Text("VS Entry:");
					ImGui::NextColumn();

					if (ShaderCompiler::GetShaderLanguageFromExtension(item->VSPath) != ShaderLanguage::GLSL) {
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_vsentry", item->VSEntry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();

					ImGui::Separator();

					/* pixel shader path */
					ImGui::Text("PS Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					if (ImGui::InputText("##pui_pspath", item->PSPath, SHADERED_MAX_PATH))
						m_data->Parser.ModifyProject();
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_psbtn", ImVec2(-1, 0))) {
						m_dialogPath = item->PSPath;
						m_dialogShaderType = "Pixel";
						ifd::FileDialog::Instance().Open("PropertyShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
					}
					ImGui::NextColumn();
					ImGui::Separator();

					/* pixel shader entry */
					ImGui::Text("PS Entry:");
					ImGui::NextColumn();

					if (ShaderCompiler::GetShaderLanguageFromExtension(item->PSPath) != ShaderLanguage::GLSL) {
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_psentry", item->PSEntry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();
					ImGui::Separator();



					// gs used
					ImGui::Text("GS:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##pui_gsuse", &item->GSUsed))
						m_data->Parser.ModifyProject();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!item->GSUsed) 
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

					// gs path
					ImGui::Text("GS path:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_gspath", item->GSPath, SHADERED_MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_gsbtn", ImVec2(-1, 0))) {
						m_dialogPath = item->GSPath;
						m_dialogShaderType = "Geometry";
						ifd::FileDialog::Instance().Open("PropertyShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
					}
					ImGui::NextColumn();
					ImGui::Separator();

					// gs entry
					ImGui::Text("GS entry:");
					ImGui::NextColumn();
					if (ShaderCompiler::GetShaderLanguageFromExtension(item->GSPath) != ShaderLanguage::GLSL) {
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_gsentry", item->GSEntry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();
					ImGui::Separator();

					if (!item->GSUsed) 
						ImGui::PopItemFlag();


					
					// ts used
					ImGui::Text("TS:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##pui_tsuse", &item->TSUsed))
						m_data->Parser.ModifyProject();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!item->TSUsed) 
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

					// patch vertices
					ImGui::Text("Patch vertices:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::DragInt("##cui_sptspatchverts", &item->TSPatchVertices, 1.0f, 1, m_data->Renderer.GetMaxPatchVertices()))
						item->TSPatchVertices = std::max(1, std::min(m_data->Renderer.GetMaxPatchVertices(), item->TSPatchVertices));
					ImGui::NextColumn();
					ImGui::Separator();

					// tcs path
					ImGui::Text("TCS path:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_tcspath", item->TCSPath, SHADERED_MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_tcsbtn", ImVec2(-1, 0))) {
						m_dialogPath = item->TCSPath;
						m_dialogShaderType = "Tessellation Control";
						ifd::FileDialog::Instance().Open("PropertyShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.tess,.tcs,.tes,.slang,.shader},.*");
					}
					ImGui::NextColumn();
					ImGui::Separator();

					// tcs entry
					ImGui::Text("TCS entry:");
					ImGui::NextColumn();
					if (ShaderCompiler::GetShaderLanguageFromExtension(item->TCSPath) != ShaderLanguage::GLSL) {
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_tcsentry", item->TCSEntry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();
					ImGui::Separator();

					// tes path
					ImGui::Text("TES path:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_tespath", item->TESPath, SHADERED_MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_tesbtn", ImVec2(-1, 0))) {
						m_dialogPath = item->TESPath;
						m_dialogShaderType = "Tessellation Evaluation";
						ifd::FileDialog::Instance().Open("PropertyShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.tess,.tcs,.tes,.slang,.shader},.*");
					}
					ImGui::NextColumn();
					ImGui::Separator();

					// tes entry
					ImGui::Text("TES entry:");
					ImGui::NextColumn();
					if (ShaderCompiler::GetShaderLanguageFromExtension(item->TESPath) != ShaderLanguage::GLSL) {
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_tesentry", item->TESEntry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();

					if (!item->TSUsed) 
						ImGui::PopItemFlag();
				} 
				else if (m_current->Type == ed::PipelineItem::ItemType::ComputePass) {
					ed::pipe::ComputePass* item = reinterpret_cast<ed::pipe::ComputePass*>(m_current->Data);

					/* compute shader path */
					ImGui::Text("Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_cspath", item->Path, SHADERED_MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_csbtn", ImVec2(-1, 0))) {
						m_dialogPath = item->Path;
						m_dialogShaderType = "Compute";
						ifd::FileDialog::Instance().Open("PropertyShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
					}
					ImGui::NextColumn();
					ImGui::Separator();

					/* compute shader entry */
					ImGui::Text("Entry:");
					ImGui::NextColumn();

					if (ShaderCompiler::GetShaderLanguageFromExtension(item->Path) != ShaderLanguage::GLSL) {
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_csentry", item->Entry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();
					ImGui::Separator();

					/* group size */
					ImGui::Text("Group size:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::InputInt3("##pui_csgroupsize", glm::value_ptr(m_cachedGroupSize));
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("OK##pui_csapply", ImVec2(-1, 0))) {
						item->WorkX = std::max<int>(m_cachedGroupSize.x, 1);
						item->WorkY = std::max<int>(m_cachedGroupSize.y, 1);
						item->WorkZ = std::max<int>(m_cachedGroupSize.z, 1);

						m_data->Parser.ModifyProject();
					}
				} 
				else if (m_current->Type == ed::PipelineItem::ItemType::AudioPass) {
					ed::pipe::AudioPass* item = reinterpret_cast<ed::pipe::AudioPass*>(m_current->Data);

					/* audio shader path */
					ImGui::Text("Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_sspath", item->Path, SHADERED_MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_ssbtn", ImVec2(-1, 0))) {
						m_dialogPath = item->Path;
						m_dialogShaderType = "Audio";
						ifd::FileDialog::Instance().Open("PropertyShaderDlg", "Select a shader", "GLSL & HLSL {.glsl,.hlsl,.vert,.vs,.frag,.fs,.geom,.gs,.comp,.cs,.slang,.shader},.*");
					}
				} 
				else if (m_current->Type == ed::PipelineItem::ItemType::Geometry) {
					ed::pipe::GeometryItem* item = reinterpret_cast<ed::pipe::GeometryItem*>(m_current->Data);

					/* position */
					ImGui::Text("Position:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat3("##pui_geopos", glm::value_ptr(item->Position), 0.01f))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* scale */
					ImGui::Text("Scale:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat3("##pui_geoscale", glm::value_ptr(item->Scale), 0.01f))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* rotation */
					ImGui::Text("Rotation:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					glm::vec3 rotaDeg(glm::degrees(item->Rotation.x), glm::degrees(item->Rotation.y), glm::degrees(item->Rotation.z));
					if (ImGui::DragFloat3("##pui_georota", glm::value_ptr(rotaDeg), 0.01f))
						m_data->Parser.ModifyProject();
					rotaDeg = glm::fmod(rotaDeg, glm::vec3(360.0f));
					if (glm::any(glm::lessThan(rotaDeg, glm::vec3(0.0))))
						rotaDeg += glm::vec3(360.0f);
					item->Rotation = glm::vec3(glm::radians(rotaDeg.x), glm::radians(rotaDeg.y), glm::radians(rotaDeg.z));
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* topology type */
					ImGui::Text("Topology:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					int selectedTopology = 0;
					for (selectedTopology = 0; selectedTopology < HARRAYSIZE(TOPOLOGY_ITEM_VALUES); selectedTopology++) {
						if (TOPOLOGY_ITEM_VALUES[selectedTopology] == item->Topology) 
							break;
					}
					if (ImGui::Combo("##pui_geotopology", reinterpret_cast<int*>(&selectedTopology), TOPOLOGY_ITEM_NAMES, HARRAYSIZE(TOPOLOGY_ITEM_NAMES))) {
						item->Topology = TOPOLOGY_ITEM_VALUES[selectedTopology];
						m_data->Parser.ModifyProject();
					}
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instanced */
					ImGui::Text("Instanced:");
					ImGui::NextColumn();

					if (ImGui::Checkbox("##pui_geoinst", &item->Instanced))
						m_data->Parser.ModifyProject();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instance count */
					ImGui::Text("Instance count:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::InputInt("##pui_geoinstcount", &item->InstanceCount))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instance array buffers */
					ImGui::Text("Instance input buffer:");
					ImGui::NextColumn();

					const auto& bufList = m_data->Objects.GetObjects();
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##pui_geo_instancebuf", ((item->InstanceBuffer == nullptr) ? "NULL" : (m_data->Objects.GetByBufferID(((BufferObject*)item->InstanceBuffer)->ID)->Name.c_str())))) {
						// null element
						if (ImGui::Selectable("NULL", item->InstanceBuffer == nullptr)) {
							item->InstanceBuffer = nullptr;

							char* owner = m_data->Pipeline.GetItemOwner(m_current->Name);
							pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

							gl::CreateVAO(item->VAO, item->VBO, ownerData->InputLayout);

							m_data->Parser.ModifyProject();
						}

						for (int i = 0; i < bufList.size(); i++) {
							if (bufList[i]->Type != ObjectType::Buffer)
								continue;

							ed::BufferObject* buf = bufList[i]->Buffer;

							if (ImGui::Selectable(bufList[i]->Name.c_str(), buf == item->InstanceBuffer)) {
								item->InstanceBuffer = buf;
								auto fmtList = m_data->Objects.ParseBufferFormat(buf->ViewFormat);

								char* owner = m_data->Pipeline.GetItemOwner(m_current->Name);
								pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

								gl::CreateVAO(item->VAO, item->VBO, ownerData->InputLayout, 0, buf->ID, fmtList);

								m_data->Parser.ModifyProject();
							}
						}

						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				} 
				else if (m_current->Type == PipelineItem::ItemType::RenderState) {
					pipe::RenderState* data = (pipe::RenderState*)m_current->Data;

					// enable/disable wireframe rendering
					bool isWireframe = data->PolygonMode == GL_LINE;
					ImGui::Text("Wireframe:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_wireframe", (bool*)(&isWireframe)))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();
					data->PolygonMode = isWireframe ? GL_LINE : GL_FILL;

					// cull
					ImGui::Text("Cull:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_cullm", &data->CullFace))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// cull mode
					ImGui::Text("Cull mode:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateCullModeCombo("##cui_culltype", data->CullFaceType))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face == counter clockwise order
					bool isCCW = data->FrontFace == GL_CCW;
					ImGui::Text("Counter clockwise:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_ccw", &isCCW))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();
					data->FrontFace = isCCW ? GL_CCW : GL_CW;

					// depth enable
					ImGui::Text("Depth test:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_depth", &data->DepthTest))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->DepthTest) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					// depth clip
					ImGui::Text("Depth clip:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_depthclip", &data->DepthClamp))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// depth mask
					ImGui::Text("Depth mask:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_depthmask", &data->DepthMask))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// depth function
					ImGui::Text("Depth function:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateComparisonFunctionCombo("##cui_depthop", data->DepthFunction))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// depth bias
					ImGui::Text("Depth bias:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat("##cui_depthbias", &data->DepthBias))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->DepthTest) {
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}

					// blending
					ImGui::Text("Blending:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_blend", &data->Blend))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->Blend) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					// alpha to coverage
					ImGui::Text("Alpha to coverage:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_alphacov", &data->AlphaToCoverage))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// source blend factor
					ImGui::Text("Source blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateBlendCombo("##cui_srcblend", data->BlendSourceFactorRGB))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// operator
					ImGui::Text("Blend operator:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateBlendOperatorCombo("##cui_blendop", data->BlendFunctionColor))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// destination blend factor
					ImGui::Text("Destination blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateBlendCombo("##cui_destblend", data->BlendDestinationFactorRGB))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// source alpha blend factor
					ImGui::Text("Source alpha blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateBlendCombo("##cui_srcalphablend", data->BlendSourceFactorAlpha))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// operator
					ImGui::Text("Alpha blend operator:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateBlendOperatorCombo("##cui_blendopalpha", data->BlendFunctionAlpha))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// destination alpha blend factor
					ImGui::Text("Destination alpha blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateBlendCombo("##cui_destalphablend", data->BlendDestinationFactorAlpha))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// blend factor
					ImGui::Text("Custom blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::ColorEdit4("##cui_blendfactor", glm::value_ptr(data->BlendFactor)))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->Blend) {
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}

					// stencil enable
					ImGui::Text("Stencil test:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::Checkbox("##cui_stencil", &data->StencilTest))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->StencilTest) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					// stencil mask
					ImGui::Text("Stencil mask:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::InputScalar("##cui_stencilmask", ImGuiDataType_U8, (int*) & data->StencilMask))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// stencil reference
					ImGui::Text("Stencil reference:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::InputScalar("##cui_sref", ImGuiDataType_U8, (int*)&data->StencilReference))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face function
					ImGui::Text("Stencil front face function:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateComparisonFunctionCombo("##cui_ffunc", data->StencilFrontFaceFunction))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face pass operation
					ImGui::Text("Stencil front face pass:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateStencilOperationCombo("##cui_fpass", data->StencilFrontFaceOpPass))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face stencil fail operation
					ImGui::Text("Stencil front face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateStencilOperationCombo("##cui_ffail", data->StencilFrontFaceOpStencilFail))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face depth fail operation
					ImGui::Text("Depth front face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateStencilOperationCombo("##cui_fdfail", data->StencilFrontFaceOpDepthFail))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face function
					ImGui::Text("Stencil back face function:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateComparisonFunctionCombo("##cui_bfunc", data->StencilBackFaceFunction))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face pass operation
					ImGui::Text("Stencil back face pass:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateStencilOperationCombo("##cui_bpass", data->StencilBackFaceOpPass))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face stencil fail operation
					ImGui::Text("Stencil back face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateStencilOperationCombo("##cui_bfail", data->StencilBackFaceOpStencilFail))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face depth fail operation
					ImGui::Text("Depth back face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (UIHelper::CreateStencilOperationCombo("##cui_bdfail", data->StencilBackFaceOpDepthFail))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();

					if (!data->StencilTest) {
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}
				} 
				else if (m_current->Type == ed::PipelineItem::ItemType::Model) {
					ed::pipe::Model* item = reinterpret_cast<ed::pipe::Model*>(m_current->Data);

					/* position */
					ImGui::Text("Position:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat3("##pui_geopos", glm::value_ptr(item->Position), 0.01f))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* scale */
					ImGui::Text("Scale:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat3("##pui_geoscale", glm::value_ptr(item->Scale), 0.01f))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* rotation */
					ImGui::Text("Rotation:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					glm::vec3 rotaDeg(glm::degrees(item->Rotation.x), glm::degrees(item->Rotation.y), glm::degrees(item->Rotation.z));
					if (ImGui::DragFloat3("##pui_georota", glm::value_ptr(rotaDeg), 0.01f))
						m_data->Parser.ModifyProject();
					item->Rotation = glm::vec3(glm::radians(rotaDeg.x), glm::radians(rotaDeg.y), glm::radians(rotaDeg.z));
					ImGui::PopItemWidth();
					ImGui::NextColumn();

					/* instanced */
					ImGui::Text("Instanced:");
					ImGui::NextColumn();

					if (ImGui::Checkbox("##pui_mdlinst", &item->Instanced))
						m_data->Parser.ModifyProject();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instance count */
					ImGui::Text("Instance count:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::InputInt("##pui_mdlinstcount", &item->InstanceCount))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instance array buffers */
					ImGui::Text("Instance input buffer:");
					ImGui::NextColumn();

					const auto& bufList = m_data->Objects.GetObjects();
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##pui_mdl_instancebuf", ((item->InstanceBuffer == nullptr) ? "NULL" : (m_data->Objects.GetByBufferID(((BufferObject*)item->InstanceBuffer)->ID)->Name.c_str())))) {
						// null element
						if (ImGui::Selectable("NULL", item->InstanceBuffer == nullptr)) {
							item->InstanceBuffer = nullptr;

							char* owner = m_data->Pipeline.GetItemOwner(m_current->Name);
							pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

							for (auto& mesh : item->Data->Meshes)
								gl::CreateVAO(mesh.VAO, mesh.VBO, ownerData->InputLayout, mesh.EBO);

							m_data->Parser.ModifyProject();
						}

						for (int i = 0; i < bufList.size(); i++) {
							if (bufList[i]->Buffer == nullptr)
								continue;

							BufferObject* buf = bufList[i]->Buffer;

							if (ImGui::Selectable(bufList[i]->Name.c_str(), buf == item->InstanceBuffer)) {
								item->InstanceBuffer = buf;
								auto fmtList = m_data->Objects.ParseBufferFormat(buf->ViewFormat);

								char* owner = m_data->Pipeline.GetItemOwner(m_current->Name);
								pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

								for (auto& mesh : item->Data->Meshes)
									gl::CreateVAO(mesh.VAO, mesh.VBO, ownerData->InputLayout, mesh.EBO, buf->ID, fmtList);

								m_data->Parser.ModifyProject();
							}
						}

						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				} 
				else if (m_current->Type == ed::PipelineItem::ItemType::PluginItem) {
					ImGui::Columns(1);

					pipe::PluginItemData* pdata = (pipe::PluginItemData*)m_current->Data;
					pdata->Owner->PipelineItem_ShowProperties(pdata->Type, pdata->PluginData);
				} 
				else if (m_current->Type == ed::PipelineItem::ItemType::VertexBuffer) {
					ed::pipe::VertexBuffer* item = reinterpret_cast<ed::pipe::VertexBuffer*>(m_current->Data);
					auto& bufList = m_data->Objects.GetObjects();

					/* buffers */
					ImGui::Text("Buffer:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##pui_vb_buffer", ((item->Buffer == nullptr) ? "NULL" : (m_data->Objects.GetByBufferID(((BufferObject*)item->Buffer)->ID)->Name.c_str())))) {
						// null element
						if (ImGui::Selectable("NULL", item->Buffer == nullptr)) {
							item->Buffer = nullptr;

							glDeleteVertexArrays(1, &item->VAO);
							item->VAO = 0;

							m_data->Parser.ModifyProject();
						}

						for (int i = 0; i < bufList.size(); i++) {
							if (bufList[i]->Type != ObjectType::Buffer)
								continue;

							ed::BufferObject* buf = bufList[i]->Buffer;

							if (ImGui::Selectable(bufList[i]->Name.c_str(), buf == item->Buffer)) {
								item->Buffer = buf;
								auto fmtList = m_data->Objects.ParseBufferFormat(buf->ViewFormat);

								char* owner = m_data->Pipeline.GetItemOwner(m_current->Name);
								pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

								gl::CreateBufferVAO(item->VAO, buf->ID, m_data->Objects.ParseBufferFormat(buf->ViewFormat));

								m_data->Parser.ModifyProject();
							}
						}

						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* position */
					ImGui::Text("Position:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat3("##pui_geopos", glm::value_ptr(item->Position), 0.01f))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* scale */
					ImGui::Text("Scale:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat3("##pui_geoscale", glm::value_ptr(item->Scale), 0.01f))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* rotation */
					ImGui::Text("Rotation:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					glm::vec3 rotaDeg(glm::degrees(item->Rotation.x), glm::degrees(item->Rotation.y), glm::degrees(item->Rotation.z));
					if (ImGui::DragFloat3("##pui_georota", glm::value_ptr(rotaDeg), 0.01f))
						m_data->Parser.ModifyProject();
					rotaDeg = glm::fmod(rotaDeg, glm::vec3(360.0f));
					if (glm::any(glm::lessThan(rotaDeg, glm::vec3(0.0))))
						rotaDeg += glm::vec3(360.0f);
					item->Rotation = glm::vec3(glm::radians(rotaDeg.x), glm::radians(rotaDeg.y), glm::radians(rotaDeg.z));
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* topology type */
					ImGui::Text("Topology:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					int selectedTopology = 0;
					for (selectedTopology = 0; selectedTopology < HARRAYSIZE(TOPOLOGY_ITEM_VALUES); selectedTopology++) {
						if (TOPOLOGY_ITEM_VALUES[selectedTopology] == item->Topology)
							break;
					}
					if (ImGui::Combo("##pui_geotopology", reinterpret_cast<int*>(&selectedTopology), TOPOLOGY_ITEM_NAMES, HARRAYSIZE(TOPOLOGY_ITEM_NAMES))) {
						item->Topology = TOPOLOGY_ITEM_VALUES[selectedTopology];
						m_data->Parser.ModifyProject();
					}
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instanced */
					ImGui::Text("Instanced:");
					ImGui::NextColumn();

					if (ImGui::Checkbox("##pui_geoinst", &item->Instanced))
						m_data->Parser.ModifyProject();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instance count */
					ImGui::Text("Instance count:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::InputInt("##pui_geoinstcount", &item->InstanceCount))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* instance array buffers */
					ImGui::Text("Instance input buffer:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##pui_geo_instancebuf", ((item->InstanceBuffer == nullptr) ? "NULL" : (m_data->Objects.GetByBufferID(((BufferObject*)item->InstanceBuffer)->ID)->Name.c_str())))) {
						// null element
						if (ImGui::Selectable("NULL", item->InstanceBuffer == nullptr)) {
							item->InstanceBuffer = nullptr;

							ed::BufferObject* bobj = (BufferObject*)item->Buffer;
							gl::CreateBufferVAO(item->VAO, bobj->ID, m_data->Objects.ParseBufferFormat(bobj->ViewFormat));

							m_data->Parser.ModifyProject();
						}

						for (int i = 0; i < bufList.size(); i++) {
							if (bufList[i]->Type != ObjectType::Buffer)
								continue;

							ed::BufferObject* buf = bufList[i]->Buffer;

							if (ImGui::Selectable(bufList[i]->Name.c_str(), buf == item->InstanceBuffer)) {
								item->InstanceBuffer = buf;
								auto fmtList = m_data->Objects.ParseBufferFormat(buf->ViewFormat);

								ed::BufferObject* bobj = (BufferObject*)item->Buffer;
								gl::CreateBufferVAO(item->VAO, bobj->ID, m_data->Objects.ParseBufferFormat(bobj->ViewFormat), buf->ID, fmtList);

								m_data->Parser.ModifyProject();
							}
						}

						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				} 
			} 
			else if (IsRenderTexture()) {
				ed::RenderTextureObject* m_currentRT = m_currentObj->RT;

				/* FIXED SIZE */
				ImGui::Text("Fixed size:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt2("##prop_rt_fsize", glm::value_ptr(m_currentRT->FixedSize), 1)) {
					m_currentRT->RatioSize = glm::vec2(-1, -1);
					if (m_currentRT->FixedSize.x <= 0)
						m_currentRT->FixedSize.x = 10;
					if (m_currentRT->FixedSize.y <= 0)
						m_currentRT->FixedSize.y = 10;

					m_data->Objects.ResizeRenderTexture(m_currentObj, m_currentRT->FixedSize);
					m_data->Parser.ModifyProject();
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* RATIO SIZE */
				ImGui::Text("Ratio size:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat2("##prop_rt_rsize", glm::value_ptr(m_currentRT->RatioSize), 0.01f)) {
					m_currentRT->FixedSize = glm::ivec2(-1, -1);
					if (m_currentRT->RatioSize.x <= 0)
						m_currentRT->RatioSize.x = 0.01f;
					if (m_currentRT->RatioSize.y <= 0)
						m_currentRT->RatioSize.y = 0.01f;

					glm::ivec2 newSize(m_currentRT->RatioSize.x * m_data->Renderer.GetLastRenderSize().x,
						m_currentRT->RatioSize.y * m_data->Renderer.GetLastRenderSize().y);

					m_data->Objects.ResizeRenderTexture(m_currentObj, newSize);
					m_data->Parser.ModifyProject();
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* FORMAT */
				ImGui::Text("Format:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##pui_format_combo", gl::String::Format(m_currentRT->Format))) {
					int len = (sizeof(FORMAT_NAMES) / sizeof(*FORMAT_NAMES));
					for (int i = 0; i < len; i++) {
						if (ImGui::Selectable(FORMAT_NAMES[i], FORMAT_VALUES[i] == m_currentRT->Format)) {
							m_currentRT->Format = FORMAT_VALUES[i];
							glm::ivec2 wsize(m_data->Renderer.GetLastRenderSize().x, m_data->Renderer.GetLastRenderSize().y);

							m_data->Objects.ResizeRenderTexture(m_currentObj, m_currentRT->CalculateSize(wsize.x, wsize.y));
							m_data->Parser.ModifyProject();
						}
					}

					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* CLEAR? */
				ImGui::Text("Clear:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##pui_gsuse", &m_currentRT->Clear))
					m_data->Parser.ModifyProject();
				ImGui::NextColumn();
				ImGui::Separator();

				/* CLEAR COLOR */
				ImGui::Text("Clear color:");
				ImGui::NextColumn();

				if (!m_currentRT->Clear) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				ImGui::PushItemWidth(-1);
				if (ImGui::ColorEdit4("##prop_rt_color", glm::value_ptr(m_currentRT->ClearColor)))
					m_data->Parser.ModifyProject();
				ImGui::PopItemWidth();

				if (!m_currentRT->Clear) {
					ImGui::PopStyleVar();
					ImGui::PopItemFlag();
				}
			} 
			else if (IsImage()) {
				ed::ImageObject* m_currentImg = m_currentObj->Image;

				/* SIZE */
				ImGui::Text("Size:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt2("##prop_rt_fsize", glm::value_ptr(m_currentImg->Size), 1)) {
					if (m_currentImg->Size.x <= 0)
						m_currentImg->Size.x = 1;
					if (m_currentImg->Size.y <= 0)
						m_currentImg->Size.y = 1;

					m_data->Objects.ResizeImage(m_currentObj, m_currentImg->Size);
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* FORMAT */
				ImGui::Text("Format:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##pui_format_combo", gl::String::Format(m_currentImg->Format))) {
					int len = (sizeof(FORMAT_NAMES) / sizeof(*FORMAT_NAMES));
					for (int i = 0; i < len; i++) {
						if (ImGui::Selectable(FORMAT_NAMES[i], FORMAT_VALUES[i] == m_currentImg->Format)) {
							m_currentImg->Format = FORMAT_VALUES[i];
							glm::ivec2 wsize(m_data->Renderer.GetLastRenderSize().x, m_data->Renderer.GetLastRenderSize().y);

							m_data->Objects.ResizeImage(m_currentObj, m_currentImg->Size);
						}
					}

					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* DATA */
				ImGui::Text("Data:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(REFRESH_BUTTON_SPACE_LEFT);
				if (ImGui::BeginCombo("##pui_texture_combo", m_currentImg->DataPath[0] == 0 ? "CLEAR" : m_currentImg->DataPath)) {
					if (ImGui::Selectable("CLEAR", m_currentImg->DataPath[0] == 0))
						m_currentImg->DataPath[0] = 0;

					
					const std::vector<ObjectManagerItem*>& texData = m_data->Objects.GetObjects();

					for (int i = 0; i < texData.size(); i++)
						if (texData[i]->Type == ObjectType::Texture && ImGui::Selectable(texData[i]->Name.c_str(), texData[i]->Name == m_currentImg->DataPath))
							strcpy(m_currentImg->DataPath, texData[i]->Name.c_str());

					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("REFRESH##prop_img_refresh")) {
					int buttonid = UIHelper::MessageBox_YesNoCancel(m_ui->GetSDLWindow(), "Are you sure that you want to update image's content?");
					
					if (buttonid == 0) {
						GLuint tex = 0;
						glm::ivec2 texSize(0);
						if (m_currentImg->DataPath[0] != 0) {
							ObjectManagerItem* texPtr = m_data->Objects.Get(m_currentImg->DataPath);
							tex = texPtr->Texture;
							texSize = texPtr->TextureSize;
						}

						m_data->Objects.UploadDataToImage(m_currentImg, tex, texSize);
					}
				}
			} 
			else if (IsTexture()) {
				/* texture path */
				ImGui::Text("Path:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::InputText("##pui_texpath", const_cast<char*>(m_currentObj->Name.c_str()), SHADERED_MAX_PATH); // not like it's going to be modified, amirite
				ImGui::PopItemFlag();
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("...##pui_texbtn", ImVec2(-1, 0)))
					ifd::FileDialog::Instance().Open("PropertyTextureDlg", "Select a texture", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
				ImGui::NextColumn();
				ImGui::Separator();

				/* VFLIP */
				ImGui::Text("VFlip:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				bool flipped = m_currentObj->Texture_VFlipped, paramsUpdated = false;
				if (ImGui::Checkbox("##prop_tex_vflip", &flipped)) {
					m_data->Objects.FlipTexture(m_itemName);
					m_data->Parser.ModifyProject();
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* MIN FILTER */
				ImGui::Text("MinFilter:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureMinFilterCombo("##prop_tex_min", m_currentObj->Texture_MinFilter))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* MAG FILTER */
				ImGui::Text("MagFilter:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureMagFilterCombo("##prop_tex_mag", m_currentObj->Texture_MagFilter))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* WRAP S */
				ImGui::Text("Wrap S:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureWrapCombo("##prop_tex_wrap_s", m_currentObj->Texture_WrapS))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* WRAP T */
				ImGui::Text("Wrap T:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureWrapCombo("##prop_tex_wrap_t", m_currentObj->Texture_WrapT))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				if (paramsUpdated) {
					m_data->Objects.UpdateTextureParameters(m_itemName);
					m_data->Parser.ModifyProject();
				}
			} 
			else if (IsImage3D()) {
				ed::Image3DObject* m_currentImg3D = m_currentObj->Image3D;

				/* SIZE */
				ImGui::Text("Size:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt3("##prop_img_fsize", glm::value_ptr(m_currentImg3D->Size), 1)) {
					if (m_currentImg3D->Size.x <= 0)
						m_currentImg3D->Size.x = 1;
					if (m_currentImg3D->Size.y <= 0)
						m_currentImg3D->Size.y = 1;
					if (m_currentImg3D->Size.z <= 0)
						m_currentImg3D->Size.z = 1;

					m_data->Objects.ResizeImage3D(m_currentObj, m_currentImg3D->Size);
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* FORMAT */
				ImGui::Text("Format:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##pui_img_format_combo", gl::String::Format(m_currentImg3D->Format))) {
					int len = (sizeof(FORMAT_NAMES) / sizeof(*FORMAT_NAMES));
					for (int i = 0; i < len; i++) {
						if (ImGui::Selectable(FORMAT_NAMES[i], FORMAT_VALUES[i] == m_currentImg3D->Format)) {
							m_currentImg3D->Format = FORMAT_VALUES[i];
							glm::ivec2 wsize(m_data->Renderer.GetLastRenderSize().x, m_data->Renderer.GetLastRenderSize().y);

							m_data->Objects.ResizeImage3D(m_currentObj, m_currentImg3D->Size);
						}
					}

					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
			} 
			else if (IsTexture3D()) {
				/* texture path */
				ImGui::Text("Path:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::InputText("##pui_tex3dpath", const_cast<char*>(m_currentObj->Name.c_str()), SHADERED_MAX_PATH); // not like it's going to be modified, amirite
				ImGui::PopItemFlag();
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("...##pui_texbtn", ImVec2(-1, 0)))
					ifd::FileDialog::Instance().Open("PropertyTextureDlg", "Select a texture", "DDS file (*.dds){.dds},.*");
				ImGui::NextColumn();
				ImGui::Separator();

				/* MIN FILTER */
				ImGui::Text("MinFilter:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				bool paramsUpdated = false;
				if (UIHelper::CreateTextureMinFilterCombo("##prop_tex3d_min", m_currentObj->Texture_MinFilter))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* MAG FILTER */
				ImGui::Text("MagFilter:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureMagFilterCombo("##prop_tex3d_mag", m_currentObj->Texture_MagFilter))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* WRAP S */
				ImGui::Text("Wrap S:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureWrapCombo("##prop_tex3d_wrap_s", m_currentObj->Texture_WrapS))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* WRAP T */
				ImGui::Text("Wrap T:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureWrapCombo("##prop_tex3d_wrap_t", m_currentObj->Texture_WrapT))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				/* WRAP T */
				ImGui::Text("Wrap R:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateTextureWrapCombo("##prop_tex3d_wrap_r", m_currentObj->Texture_WrapR))
					paramsUpdated = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				if (paramsUpdated) {
					m_data->Objects.UpdateTextureParameters(m_itemName);
					m_data->Parser.ModifyProject();
				}
			} 
			else if (IsPlugin()) {
				ImGui::Columns(1);

				m_currentObj->Plugin->Owner->Object_ShowProperties(m_currentObj->Plugin->Type, m_currentObj->Plugin->Data, m_currentObj->Plugin->ID);
			}

			ImGui::NextColumn();
			ImGui::Separator();
			ImGui::Columns(1);
		} else
			ImGui::TextWrapped("Right click on an item -> Properties");

		
		// file dialogs
		if (ifd::FileDialog::Instance().IsDone("PropertyShaderDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string file = ifd::FileDialog::Instance().GetResult().u8string();
				file = m_data->Parser.GetRelativePath(file);

				strcpy(m_dialogPath, file.c_str());

				m_data->Parser.ModifyProject();

				if (m_data->Parser.FileExists(file)) {
					m_data->Messages.ClearGroup(m_current->Name);
					m_data->Renderer.Recompile(m_current->Name);
				} else
					m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, m_dialogShaderType + " shader file doesnt exist");
			}
			ifd::FileDialog::Instance().Close();
		}
		if (ifd::FileDialog::Instance().IsDone("PropertyTextureDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string file = ifd::FileDialog::Instance().GetResult().u8string();
				file = m_data->Parser.GetRelativePath(file);
				m_data->Objects.ReloadTexture(m_currentObj, file);
			}
			ifd::FileDialog::Instance().Close();
		}
	}
	void PropertyUI::Open(ed::PipelineItem* item)
	{
		if (item != nullptr) {
			if (item->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pldata = (pipe::PluginItemData*)item->Data;
				if (!pldata->Owner->PipelineItem_HasProperties(pldata->Type, pldata->PluginData))
					return; // doesnt support properties
			}

			memcpy(m_itemName, item->Name, PIPELINE_ITEM_NAME_LENGTH);
			m_itemName[PIPELINE_ITEM_NAME_LENGTH] = 0;

			if (item->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* cPass = (pipe::ComputePass*)item->Data;
				m_cachedGroupSize = glm::ivec3(cPass->WorkX, cPass->WorkY, cPass->WorkZ);
			}
		}

		Logger::Get().Log("Opening a pipeline item in the PropertyUI");

		m_current = item;
		m_currentObj = nullptr;

		if (item != nullptr)
			Visible = true;
	}
	void PropertyUI::Open(ObjectManagerItem* obj)
	{
		Logger::Get().Log("Opening an ObjectManager item in the PropertyUI");

		memset(m_itemName, 0, sizeof(m_itemName) / sizeof(m_itemName[0]));

		m_current = nullptr;
		m_currentObj = obj;

		if (obj != nullptr) {
			memcpy(m_itemName, obj->Name.c_str(), obj->Name.size());
			Visible = true;
		}
	}
}