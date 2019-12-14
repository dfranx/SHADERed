#include "PropertyUI.h"
#include "UIHelper.h"
#include "CodeEditorUI.h"
#include "../Engine/GLUtils.h"
#include "../Objects/Logger.h"
#include "../Objects/Names.h"
#include "../Objects/ShaderTranscompiler.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#define BUTTON_SPACE_LEFT -40 * Settings::Instance().DPIScale
#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a))

namespace ed
{
	void PropertyUI::m_init()
	{
		m_current = nullptr;
		m_currentObj = nullptr;
		memset(m_itemName, 0, 64 * sizeof(char));
	}
	void PropertyUI::OnEvent(const SDL_Event& e)
	{}
	void PropertyUI::Update(float delta)
	{
		if (m_current != nullptr || m_currentObj != nullptr) {
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
			if (m_current != nullptr) {
				ImGui::Text(m_current->Name);
				ImGui::Text(PIPELINE_ITEM_NAMES[(int)m_current->Type]);
				if (m_current->Type == PipelineItem::ItemType::Geometry) {
					ImGui::SameLine();
					ImGui::Text(("(" + std::string(GEOMETRY_NAMES[(int)((pipe::GeometryItem*)m_current->Data)->Type]) + ")").c_str());
				} else if (m_current->Type == PipelineItem::ItemType::PluginItem) {
					ImGui::SameLine();
					ImGui::Text(("(" + std::string(((pipe::PluginItemData*)m_current->Data)->Type) + ")").c_str());
				}
			}
			else if (m_currentObj != nullptr) {
				ImGui::Text(m_data->Objects.GetObjectManagerItemName(m_currentObj).c_str());
				if (IsRenderTexture())
					ImGui::Text("Render Texture");
				else if (IsImage())
					ImGui::Text("Image");
				else if (IsImage3D())
					ImGui::Text("Image3D");
				else if (IsPlugin())
					ImGui::Text(m_currentObj->Plugin->Type);
				else
					ImGui::Text("ObjectManagerItem");
			}
			else
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

				ImGui::PushItemWidth(-40);
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
							pdata->Owner->RenamePipelineItem(m_current->Name, m_itemName);
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
						std::string name = rtID == 0 ? "NULL" : (rtID == m_data->Renderer.GetTexture() ? "Window" : m_data->Objects.GetRenderTexture(rtID)->Name);
						
						if (ImGui::BeginCombo(("##pui_rt_combo" + std::to_string(i)).c_str(), name.c_str())) {
							std::vector<std::string> rts = m_data->Objects.GetObjects();
							bool windowAlreadyBound = false;
							for (int k = 0; k < MAX_RENDER_TEXTURES; k++) {
								if (item->RenderTextures[k] == 0)
									continue;

								if (!windowAlreadyBound && item->RenderTextures[k] == m_data->Renderer.GetTexture()) {
									windowAlreadyBound = true;
									break;
								}

								for (int j = 0; j < rts.size(); j++) {
									if (!m_data->Objects.IsRenderTexture(rts[j]) || (m_data->Objects.IsRenderTexture(rts[j]) && item->RenderTextures[k] == m_data->Objects.GetTexture(rts[j]))) {
										rts.erase(rts.begin() + j);
										j--;
									}
								}
							}

							// prevent duplicates
							for (int j = 0; j < MAX_RENDER_TEXTURES; j++) {
								if (item->RenderTextures[j] == 0)
									break;

								std::string name = item->RenderTextures[j] == m_data->Renderer.GetTexture() ? "Window" : m_data->Objects.GetRenderTexture(item->RenderTextures[j])->Name;

								for (int k = 0; k < rts.size(); k++) {
									if (rts[k] == name) {
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
								if (m_data->Objects.IsRenderTexture(rts[j])) {
									GLuint texID = m_data->Objects.GetTexture(rts[j]);
									if (ImGui::Selectable(rts[j].c_str(), rtID == texID)) {
										m_data->Parser.ModifyProject();
										item->RenderTextures[i] = texID;
									}
								}

							ImGui::EndCombo();
						}

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
					ImGui::InputText("##pui_vspath", item->VSPath, MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_vsbtn", ImVec2(-1, 0))) {
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success) {
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->VSPath, file.c_str());

							m_data->Parser.ModifyProject();

							if (m_data->Parser.FileExists(file)) {
								m_data->Messages.ClearGroup(m_current->Name);
								m_data->Renderer.Recompile(m_current->Name);
							}
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Vertex shader file doesnt exist");
						}
					}
					ImGui::NextColumn();

					ImGui::Separator();

					/* vertex shader entry */
					ImGui::Text("VS Entry:");
					ImGui::NextColumn();

					if (ShaderTranscompiler::GetShaderTypeFromExtension(item->VSPath) != ShaderLanguage::GLSL) {
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
					if (ImGui::InputText("##pui_pspath", item->PSPath, MAX_PATH))
						m_data->Parser.ModifyProject();
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_psbtn", ImVec2(-1, 0))) {
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success) {
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->PSPath, file.c_str());

							m_data->Parser.ModifyProject();

							if (m_data->Parser.FileExists(file)) {
								m_data->Messages.ClearGroup(m_current->Name);
								m_data->Renderer.Recompile(m_current->Name);
							}
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Pixel shader file doesnt exist");
						}
					}
					ImGui::NextColumn();

					ImGui::Separator();

					/* pixel shader entry */
					ImGui::Text("PS Entry:");
					ImGui::NextColumn();

					if (ShaderTranscompiler::GetShaderTypeFromExtension(item->PSPath) != ShaderLanguage::GLSL) {
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

					if (!item->GSUsed) ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

					// gs path
					ImGui::Text("GS path:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_gspath", item->GSPath, MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_gsbtn", ImVec2(-1, 0))) {
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success) {
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->GSPath, file.c_str());

							m_data->Parser.ModifyProject();

							if (m_data->Parser.FileExists(file)) {
								m_data->Messages.ClearGroup(m_current->Name);
								m_data->Renderer.Recompile(m_current->Name);
							}
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Geometry shader file doesnt exist");
						}
					}
					ImGui::NextColumn();
					ImGui::Separator();

					// gs entry
					ImGui::Text("GS entry:");
					ImGui::NextColumn();
					if (ShaderTranscompiler::GetShaderTypeFromExtension(item->GSPath) != ShaderLanguage::GLSL) {
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_gsentry", item->GSEntry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();

					if (!item->GSUsed) ImGui::PopItemFlag();
				}
				else if (m_current->Type == ed::PipelineItem::ItemType::ComputePass)
				{
					ed::pipe::ComputePass *item = reinterpret_cast<ed::pipe::ComputePass *>(m_current->Data);

					/* compute shader path */
					ImGui::Text("Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_cspath", item->Path, MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_csbtn", ImVec2(-1, 0)))
					{
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success)
						{
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->Path, file.c_str());

							m_data->Parser.ModifyProject();

							if (m_data->Parser.FileExists(file)) {
								m_data->Messages.ClearGroup(m_current->Name);
								m_data->Renderer.Recompile(m_current->Name);
							}
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Compute shader file doesnt exist");
						}
					}
					ImGui::NextColumn();
					ImGui::Separator();

					/* compute shader entry */
					ImGui::Text("Entry:");
					ImGui::NextColumn();

					if (ShaderTranscompiler::GetShaderTypeFromExtension(item->Path) != ShaderLanguage::GLSL)
					{
						ImGui::PushItemWidth(-1);
						if (ImGui::InputText("##pui_csentry", item->Entry, 32))
							m_data->Parser.ModifyProject();
						ImGui::PopItemWidth();
					}
					else
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
					if (ImGui::Button("OK##pui_csapply", ImVec2(-1, 0)))
					{
						item->WorkX = std::max<int>(m_cachedGroupSize.x, 1);
						item->WorkY = std::max<int>(m_cachedGroupSize.y, 1);
						item->WorkZ = std::max<int>(m_cachedGroupSize.z, 1);

						m_data->Parser.ModifyProject();
					}
				}
				else if (m_current->Type == ed::PipelineItem::ItemType::AudioPass)
				{
					ed::pipe::AudioPass *item = reinterpret_cast<ed::pipe::AudioPass *>(m_current->Data);

					/* audio shader path */
					ImGui::Text("Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::InputText("##pui_sspath", item->Path, MAX_PATH);
					ImGui::PopItemFlag();
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_ssbtn", ImVec2(-1, 0)))
					{
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success)
						{
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->Path, file.c_str());

							m_data->Parser.ModifyProject();

							if (m_data->Parser.FileExists(file)) {
								m_data->Messages.ClearGroup(m_current->Name);
								m_data->Renderer.Recompile(m_current->Name);
							}
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Compute shader file doesnt exist");
						}
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
					if (rotaDeg.x > 360)
						rotaDeg.x = 0;
					if (rotaDeg.x < 0)
						rotaDeg.x = 360;
					if (rotaDeg.y > 360)
						rotaDeg.y = 0;
					if (rotaDeg.y < 0)
						rotaDeg.y = 360;
					if (rotaDeg.z > 360)
						rotaDeg.z = 0;
					if (rotaDeg.z < 0)
						rotaDeg.z = 360;
					item->Rotation = glm::vec3(glm::radians(rotaDeg.x), glm::radians(rotaDeg.y), glm::radians(rotaDeg.z));
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();
					
					/* topology type */
					ImGui::Text("Topology:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					if (ImGui::Combo("##pui_geotopology", reinterpret_cast<int*>(&item->Topology), TOPOLOGY_ITEM_NAMES, HARRAYSIZE(TOPOLOGY_ITEM_NAMES)))
						m_data->Parser.ModifyProject();
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

					const auto& bufList = m_data->Objects.GetItemDataList();
					auto& bufNames = m_data->Objects.GetObjects();
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##pui_geo_instancebuf", ((item->InstanceBuffer == nullptr) ? "NULL" : (m_data->Objects.GetBufferNameByID(((BufferObject*)item->InstanceBuffer)->ID).c_str())))) {
						// null element
						if (ImGui::Selectable("NULL", item->InstanceBuffer == nullptr)) {
							item->InstanceBuffer = nullptr;

							char* owner = m_data->Pipeline.GetItemOwner(m_current->Name);
							pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

							gl::CreateVAO(item->VAO, item->VBO, ownerData->InputLayout);
							
							m_data->Parser.ModifyProject();
						}

						for (int i = 0; i < bufList.size(); i++) {
							if (bufList[i]->Buffer == nullptr)
								continue;

							ed::BufferObject* buf = bufList[i]->Buffer;

							if (ImGui::Selectable(bufNames[i].c_str(), buf == item->InstanceBuffer)) {
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

					if (!data->DepthTest)
					{
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

					if (!data->DepthTest)
					{
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

					if (!data->Blend)
					{
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

					if (!data->Blend)
					{
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

					if (!data->StencilTest)
					{
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					// stencil mask
					ImGui::Text("Stencil mask:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::InputInt("##cui_stencilmask", (int*)& data->StencilMask))
						m_data->Parser.ModifyProject();
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (data->StencilMask < 0)
						data->StencilMask = 0;
					if (data->StencilMask > 255)
						data->StencilMask = 255;

					// stencil reference
					ImGui::Text("Stencil reference:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::InputInt("##cui_sref", (int*)& data->StencilReference))
						m_data->Parser.ModifyProject(); // TODO: imgui uint input??
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (data->StencilReference < 0)
						data->StencilReference = 0;
					if (data->StencilReference > 255)
						data->StencilReference = 255;

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

					if (!data->StencilTest)
					{
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

					const auto& bufList = m_data->Objects.GetItemDataList();
					auto& bufNames = m_data->Objects.GetObjects();
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##pui_mdl_instancebuf", ((item->InstanceBuffer == nullptr) ? "NULL" : (m_data->Objects.GetBufferNameByID(((BufferObject*)item->InstanceBuffer)->ID).c_str())))) {
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

							if (ImGui::Selectable(bufNames[i].c_str(), buf == item->InstanceBuffer)) {
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
					pdata->Owner->ShowPipelineItemProperties(pdata->Type, pdata->PluginData);
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

					m_data->Objects.ResizeRenderTexture(std::string(m_itemName), m_currentRT->FixedSize);
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

					m_data->Objects.ResizeRenderTexture(std::string(m_itemName), newSize);
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

							m_data->Objects.ResizeRenderTexture(std::string(m_itemName), m_currentRT->CalculateSize(wsize.x, wsize.y));
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
				if (ImGui::DragInt2("##prop_rt_fsize", glm::value_ptr(m_currentImg->Size), 1))
				{
					if (m_currentImg->Size.x <= 0)
						m_currentImg->Size.x = 1;
					if (m_currentImg->Size.y <= 0)
						m_currentImg->Size.y = 1;

					m_data->Objects.ResizeImage(std::string(m_itemName), m_currentImg->Size);
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* FORMAT */
				ImGui::Text("Format:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##pui_format_combo", gl::String::Format(m_currentImg->Format)))
				{
					int len = (sizeof(FORMAT_NAMES) / sizeof(*FORMAT_NAMES));
					for (int i = 0; i < len; i++)
					{
						if (ImGui::Selectable(FORMAT_NAMES[i], FORMAT_VALUES[i] == m_currentImg->Format))
						{
							m_currentImg->Format = FORMAT_VALUES[i];
							glm::ivec2 wsize(m_data->Renderer.GetLastRenderSize().x, m_data->Renderer.GetLastRenderSize().y);

							m_data->Objects.ResizeImage(std::string(m_itemName), m_currentImg->Size);
						}
					}

					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
			}
			else if (IsImage3D()) {
				ed::Image3DObject* m_currentImg3D = m_currentObj->Image3D;

				/* SIZE */
				ImGui::Text("Size:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt3("##prop_rt_fsize", glm::value_ptr(m_currentImg3D->Size), 1))
				{
					if (m_currentImg3D->Size.x <= 0)
						m_currentImg3D->Size.x = 1;
					if (m_currentImg3D->Size.y <= 0)
						m_currentImg3D->Size.y = 1;
					if (m_currentImg3D->Size.z <= 0)
						m_currentImg3D->Size.z = 1;

					m_data->Objects.ResizeImage3D(std::string(m_itemName), m_currentImg3D->Size);
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* FORMAT */
				ImGui::Text("Format:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##pui_format_combo", gl::String::Format(m_currentImg3D->Format)))
				{
					int len = (sizeof(FORMAT_NAMES) / sizeof(*FORMAT_NAMES));
					for (int i = 0; i < len; i++)
					{
						if (ImGui::Selectable(FORMAT_NAMES[i], FORMAT_VALUES[i] == m_currentImg3D->Format))
						{
							m_currentImg3D->Format = FORMAT_VALUES[i];
							glm::ivec2 wsize(m_data->Renderer.GetLastRenderSize().x, m_data->Renderer.GetLastRenderSize().y);

							m_data->Objects.ResizeImage3D(std::string(m_itemName), m_currentImg3D->Size);
						}
					}

					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
			}
			else if (IsPlugin()) {
				ImGui::Columns(1);

				m_currentObj->Plugin->Owner->ShowObjectProperties(m_currentObj->Plugin->Type, m_currentObj->Plugin->Data, m_currentObj->Plugin->ID);
			}

			ImGui::NextColumn();
			ImGui::Separator();
			ImGui::Columns(1);
		} else {
			ImGui::TextWrapped("Right click on an item -> Properties");
		}
	}
	void PropertyUI::Open(ed::PipelineItem * item)
	{
		if (item != nullptr) {
			if (item->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pldata = (pipe::PluginItemData*)item->Data;
				if (!pldata->Owner->HasPipelineItemProperties(pldata->Type))
					return; // doesnt support properties
			}

			memcpy(m_itemName, item->Name, PIPELINE_ITEM_NAME_LENGTH);

			if (item->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* cPass = (pipe::ComputePass*)item->Data;
				m_cachedGroupSize = glm::ivec3(cPass->WorkX, cPass->WorkY, cPass->WorkZ);
			}
		}

		Logger::Get().Log("Openning a pipeline item in the PropertyUI");

		m_current = item;
		m_currentObj = nullptr;
	}
	void PropertyUI::Open(const std::string &name, ObjectManagerItem *obj)
	{
		Logger::Get().Log("Openning an ObjectManager item in the PropertyUI");

		memset(m_itemName, 0, PIPELINE_ITEM_NAME_LENGTH);
		memcpy(m_itemName, name.c_str(), name.size());

		m_current = nullptr;
		m_currentObj = obj;
	}
}