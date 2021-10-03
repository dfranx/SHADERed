#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/GUIManager.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/Options.h>
#include <SHADERed/UI/PixelInspectUI.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/Icons.h>
#include <SHADERed/UI/PinnedUI.h>
#include <SHADERed/UI/PipelineUI.h>
#include <SHADERed/UI/PreviewUI.h>
#include <SHADERed/UI/PropertyUI.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <algorithm>

#define HARRAYSIZE(a) (sizeof(a) / sizeof(*a))
#define PIPELINE_SHADER_PASS_INDENT Settings::Instance().CalculateSize(95)
#define PIPELINE_ITEM_INDENT Settings::Instance().CalculateSize(105)
#define BUTTON_ICON_SIZE ImVec2(Settings::Instance().CalculateSize(22.5f), 0)

namespace ed {
	void PipelineUI::OnEvent(const SDL_Event& e)
	{
	}
	void PipelineUI::Update(float delta)
	{
		std::vector<ed::PipelineItem*>& items = m_data->Pipeline.GetList();

		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		ImGui::BeginChild("##object_scroll_container", containerSize);

		if (items.size() == 0)
			ImGui::TextWrapped("Right click on this window (or go to Create menu in the menu bar) to create a shader pass.");

		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
		for (int i = 0; i < items.size(); i++) {
			m_renderItemUpDown(items[i]->Type == PipelineItem::ItemType::PluginItem ? (pipe::PluginItemData*)items[i]->Data : nullptr, items, i);

			if (items[i]->Type == PipelineItem::ItemType::ShaderPass) {

				m_addShaderPass(items[i]);
				if (m_renderItemContext(items, i)) {
					i--;
					continue;
				}

				ed::pipe::ShaderPass* data = (ed::pipe::ShaderPass*)items[i]->Data;

				bool showItems = true;
				for (int j = 0; j < m_expandList.size(); j++)
					if (m_expandList[j] == data) {
						showItems = false;
						break;
					}

				if (showItems) {
					for (int j = 0; j < data->Items.size(); j++) {
						m_renderItemUpDown(nullptr, data->Items, j);

						if (!data->Active)
							ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
						m_addItem(data->Items[j]);
						if (!data->Active)
							ImGui::PopStyleVar();

						if (m_renderItemContext(data->Items, j)) {
							j--;
							continue;
						}
					}
				}
			} else if (items[i]->Type == PipelineItem::ItemType::ComputePass) {
				m_addComputePass(items[i]);
				if (m_renderItemContext(items, i)) {
					i--;
					continue;
				}
			} else if (items[i]->Type == PipelineItem::ItemType::AudioPass) {
				m_addAudioPass(items[i]);
				if (m_renderItemContext(items, i)) {
					i--;
					continue;
				}
			} else if (items[i]->Type == PipelineItem::ItemType::PluginItem) {
				m_addPluginItem(items[i]);
				if (m_renderItemContext(items, i)) {
					i--;
					continue;
				}

				pipe::PluginItemData* pdata = (pipe::PluginItemData*)items[i]->Data;
				for (int j = 0; j < pdata->Items.size(); j++) {
					m_renderItemUpDown(pdata, pdata->Items, j);
					m_addItem(pdata->Items[j]);
					if (m_renderItemContext(pdata->Items, j)) {
						j--;
						continue;
					}
				}
			}
		}
		ImGui::PopStyleVar();

		ImGui::EndChild();

		if (!m_itemMenuOpened && ImGui::BeginPopupContextItem("##context_main_pipeline")) {
			if (ImGui::Selectable("Create Shader Pass")) m_ui->CreateNewShaderPass();
			if (ImGui::Selectable("Create Compute Pass")) m_ui->CreateNewComputePass();
			if (ImGui::Selectable("Create Audio Pass")) m_ui->CreateNewAudioPass();
			m_data->Plugins.ShowContextItems("pipeline");
			ImGui::EndPopup();
		}
		m_itemMenuOpened = false;

		// various popups
		if (m_isComputeDebugOpen) {
			ImGui::OpenPopup("Debug compute shader");
			m_isComputeDebugOpen = false;
		}
		if (m_isVarManagerOpened) {
			ImGui::OpenPopup("Variable Manager##pui_shader_variables");
			m_isVarManagerOpened = false;
		}
		if (m_isCreateViewOpened) {
			ImGui::OpenPopup("Create Item##pui_create_item");
			m_isCreateViewOpened = false;
		}
		if (m_isChangeVarsOpened) {
			ImGui::OpenPopup("Change Variables##pui_render_variables");
			m_isChangeVarsOpened = false;
		}
		if (m_isMacroManagerOpened) {
			ImGui::OpenPopup("Shader Macros##pui_shader_macros");
			m_isMacroManagerOpened = false;
		}
		if (m_isInpLayoutManagerOpened) {
			ImGui::OpenPopup("Input layout##pui_input_layout");
			m_isInpLayoutManagerOpened = false;
		}
		if (m_isResourceManagerOpened) {
			ImGui::OpenPopup("Resource manager##pui_res_manager");
			m_isResourceManagerOpened = false;
		}
		if (m_isConfirmDeleteOpened) {
			ImGui::OpenPopup("Delete##pui_item_delete");
			m_isConfirmDeleteOpened = false;
		}

		// Compute shader debugger
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(210)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Debug compute shader")) {
			ImGui::Text("Global thread ID:");
			ImGui::SameLine();
			if (ImGui::InputScalarN("##dbg_thread_id", ImGuiDataType_U32, (void*)m_thread, 3, 0, (const void*)1)) {
				m_thread[0] = std::min<int>(m_thread[0], m_localSizeX * m_groupsX - 1);
				m_thread[1] = std::min<int>(m_thread[1], m_localSizeY * m_groupsY - 1);
				m_thread[2] = std::min<int>(m_thread[2], m_localSizeZ * m_groupsZ - 1);
			}
			
			int localInvocationIndex = (m_thread[2] % m_localSizeZ) * m_localSizeX * m_localSizeZ +
										(m_thread[1] % m_localSizeY) * m_localSizeX +
										(m_thread[0] % m_localSizeX);

			if (m_computeLang == ed::ShaderLanguage::HLSL) {
				ImGui::Text("SV_GroupID -> uint3(%d, %d, %d)", m_thread[0] / m_localSizeX, m_thread[1] / m_localSizeY, m_thread[2] / m_localSizeZ);
				ImGui::Text("SV_GroupThreadID -> uint3(%d, %d, %d)", m_thread[0] % m_localSizeX, m_thread[1] % m_localSizeY, m_thread[2] % m_localSizeZ);
				ImGui::Text("SV_DispatchThreadID -> uint3(%d, %d, %d)", m_thread[0], m_thread[1], m_thread[2]);
				ImGui::Text("SV_GroupIndex -> %d", localInvocationIndex);
			} else {
				ImGui::Text("gl_NumWorkGroups -> uvec3(%d, %d, %d)", m_groupsX, m_groupsY, m_groupsZ);
				ImGui::Text("gl_WorkGroupID -> uvec3(%d, %d, %d)", m_thread[0] / m_localSizeX, m_thread[1] / m_localSizeY, m_thread[2] / m_localSizeZ);
				ImGui::Text("gl_LocalInvocationID -> uvec3(%d, %d, %d)", m_thread[0] % m_localSizeX, m_thread[1] % m_localSizeY, m_thread[2] % m_localSizeZ);
				ImGui::Text("gl_GlobalInvocationID -> uvec3(%d, %d, %d)", m_thread[0], m_thread[1], m_thread[2]);
				ImGui::Text("gl_LocalInvocationIndex -> %d", localInvocationIndex);
			}

			if (ImGui::Button("Cancel")) m_closePopup();
			ImGui::SameLine();
			if (ImGui::Button("Start")) {
				pipe::ComputePass* pass = ((pipe::ComputePass*)m_modalItem->Data);
				// TODO: plugins?

				CodeEditorUI* codeUI = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
				codeUI->StopDebugging();
				codeUI->Open(m_modalItem, ShaderStage::Compute);
				TextEditor* editor = codeUI->Get(m_modalItem, ShaderStage::Compute);
				PluginShaderEditor pluginEditor;
				if (editor == nullptr)
					pluginEditor = codeUI->GetPluginEditor(m_modalItem, ShaderStage::Compute);

				m_data->Debugger.PrepareComputeShader(m_modalItem, m_thread[0], m_thread[1], m_thread[2]);
				((PixelInspectUI*)m_ui->Get(ViewID::PixelInspect))->StartDebugging(editor, pluginEditor, nullptr);

				m_closePopup();
			}
			ImGui::EndPopup();
		}

		// Shader Variable manager
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(730), Settings::Instance().CalculateSize(225)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Variable Manager##pui_shader_variables")) {
			m_renderVariableManagerUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}

		// Shader Macro Manager
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(530), Settings::Instance().CalculateSize(175)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Shader Macros##pui_shader_macros")) {
			m_renderMacroManagerUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}

		// Input Layout Manager
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(630), Settings::Instance().CalculateSize(225)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Input layout##pui_input_layout")) {
			m_renderInputLayoutManagerUI();

			if (ImGui::Button("Ok")) {
				// recreate all VAOs
				pipe::ShaderPass* pass = (pipe::ShaderPass*)m_modalItem->Data;
				std::vector<PipelineItem*>& passItems = pass->Items;

				for (auto& pitem : passItems) {
					if (pitem->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem* gitem = (pipe::GeometryItem*)pitem->Data;

						if (gitem->Type == pipe::GeometryItem::GeometryType::ScreenQuadNDC)
							continue;

						BufferObject* bobj = (BufferObject*)gitem->InstanceBuffer;
						if (bobj == nullptr)
							gl::CreateVAO(gitem->VAO, gitem->VBO, pass->InputLayout);
						else
							gl::CreateVAO(gitem->VAO, gitem->VBO, pass->InputLayout, 0, bobj->ID, m_data->Objects.ParseBufferFormat(bobj->ViewFormat));
					} else if (pitem->Type == PipelineItem::ItemType::Model) {
						pipe::Model* mitem = (pipe::Model*)pitem->Data;
						BufferObject* bobj = (BufferObject*)mitem->InstanceBuffer;
						if (bobj == nullptr) {
							for (auto& mesh : mitem->Data->Meshes)
								gl::CreateVAO(mesh.VAO, mesh.VBO, pass->InputLayout, mesh.EBO);
						} else {
							for (auto& mesh : mitem->Data->Meshes)
								gl::CreateVAO(mesh.VAO, mesh.VBO, pass->InputLayout, mesh.EBO, bobj->ID, m_data->Objects.ParseBufferFormat(bobj->ViewFormat));
						}
					} else if (pitem->Type == PipelineItem::ItemType::VertexBuffer) {
						pipe::VertexBuffer* mitem = (pipe::VertexBuffer*)pitem->Data;
						BufferObject* bobj = (BufferObject*)mitem->Buffer;
						if (bobj != nullptr)
							gl::CreateBufferVAO(mitem->VAO, bobj->ID, m_data->Objects.ParseBufferFormat(bobj->ViewFormat));
					}
				}

				m_closePopup();
			}
			ImGui::EndPopup();
		}

		// Create Item
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(175)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Create Item##pui_create_item")) {
			m_createUI.Update(delta);

			if (ImGui::Button("Ok")) {
				if (m_createUI.Create())
					m_closePopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) m_closePopup();
			ImGui::EndPopup();
		}

		// variables to change when rendering specific item
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(400), Settings::Instance().CalculateSize(175)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Change Variables##pui_render_variables")) {
			m_renderChangeVariablesUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}

		// resource manager
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(600), Settings::Instance().CalculateSize(240)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Resource manager##pui_res_manager")) {
			m_renderResourceManagerUI();

			if (ImGui::Button("Ok")) m_closePopup();
			ImGui::EndPopup();
		}

		// confirm delete
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(400), Settings::Instance().CalculateSize(120)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Delete##pui_item_delete", 0, ImGuiWindowFlags_NoResize)) {
			ImGui::Text("Are you sure that you want to delete item \"%s\"?", m_modalItem->Name);

			if (ImGui::Button("Yes")) {
				DeleteItem(m_modalItem);
				m_closePopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
				m_closePopup();
			ImGui::EndPopup();
		}
	}

	void PipelineUI::DeleteItem(PipelineItem* item)
	{
		(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->CloseAll(item);

		PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)));
		PreviewUI* prev = (reinterpret_cast<PreviewUI*>(m_ui->Get(ViewID::Preview)));
		
		// check if it is opened in property viewer/picked
		if (props->HasItemSelected() && props->CurrentItemName() == item->Name)
			props->Close();
		if (prev->IsPicked(item))
			prev->Pick(nullptr);

		// or their children
		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;
			for (const auto& child : pass->Items) {
				if (props->HasItemSelected() && props->CurrentItemName() == child->Name)
					props->Close();
				if (prev->IsPicked(child))
					prev->Pick(nullptr);
			}
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* pass = (pipe::PluginItemData*)item->Data;
			for (const auto& child : pass->Items) {
				if (props->HasItemSelected() && props->CurrentItemName() == child->Name)
					props->Close();
				if (prev->IsPicked(child))
					prev->Pick(nullptr);
			}
		}

		// tell pipeline to remove this item
		m_data->Messages.ClearGroup(item->Name);
		m_data->Renderer.RemoveItemVariableValues(item);
		m_data->Pipeline.Remove(item->Name);
	}

	void PipelineUI::m_renderItemUpDown(pipe::PluginItemData* owner, std::vector<ed::PipelineItem*>& items, int index)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button(std::string(UI_ICON_ARROW_UP "##U" + std::string(items[index]->Name)).c_str(), BUTTON_ICON_SIZE)) {
			if (index != 0) {
				m_data->Parser.ModifyProject();

				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)));
				std::string oldPropertyItemName = "";
				if (props->HasItemSelected())
					oldPropertyItemName = props->CurrentItemName();

				if (owner != nullptr)
					owner->Owner->PipelineItem_MoveUp(owner->PluginData, owner->Type, items[index]->Name);

				ed::PipelineItem* temp = items[index - 1];
				items[index - 1] = items[index];
				items[index] = temp;

				if (props->HasItemSelected()) {
					if (oldPropertyItemName == items[index - 1]->Name)
						props->Open(items[index - 1]);
					if (oldPropertyItemName == items[index]->Name)
						props->Open(items[index]);
				}
			}
		}
		ImGui::SameLine(0, 0);

		if (ImGui::Button(std::string(UI_ICON_ARROW_DOWN "##D" + std::string(items[index]->Name)).c_str(), BUTTON_ICON_SIZE)) {
			if (index != items.size() - 1) {
				m_data->Parser.ModifyProject();

				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)));
				std::string oldPropertyItemName = "";
				if (props->HasItemSelected())
					oldPropertyItemName = props->CurrentItemName();

				if (owner != nullptr)
					owner->Owner->PipelineItem_MoveDown(owner->PluginData, owner->Type, items[index]->Name);

				ed::PipelineItem* temp = items[index + 1];
				items[index + 1] = items[index];
				items[index] = temp;

				if (props->HasItemSelected()) {
					if (oldPropertyItemName == items[index + 1]->Name)
						props->Open(items[index + 1]);
					if (oldPropertyItemName == items[index]->Name)
						props->Open(items[index]);
				}
			}
		}
		ImGui::SameLine(0, 0);

		ImGui::PopStyleColor();
	}
	bool PipelineUI::m_renderItemContext(std::vector<ed::PipelineItem*>& items, int index)
	{
		bool ret = false; // false == we didnt delete an item

		if (ImGui::BeginPopupContextItem(("##context_" + std::string(items[index]->Name)).c_str())) {
			bool isPlugin = items[index]->Type == PipelineItem::ItemType::PluginItem;
			pipe::PluginItemData* pldata = (pipe::PluginItemData*)items[index]->Data;

			bool hasPluginProperties = isPlugin && pldata->Owner->PipelineItem_HasProperties(pldata->Type, pldata->PluginData);
			bool hasPluginAddMenu = isPlugin && pldata->Owner->PipelineItem_CanHaveChildren(pldata->Type, pldata->PluginData);
			bool hasPluginShaders = isPlugin && pldata->Owner->PipelineItem_HasShaders(pldata->Type, pldata->PluginData);
			bool hasPluginContext = isPlugin && pldata->Owner->PipelineItem_HasContext(pldata->Type, pldata->PluginData);

			m_itemMenuOpened = true;
			if (items[index]->Type == PipelineItem::ItemType::ShaderPass || items[index]->Type == PipelineItem::ItemType::ComputePass || items[index]->Type == PipelineItem::ItemType::AudioPass || hasPluginAddMenu) {
				if (!isPlugin && ImGui::Selectable("Recompile"))
					m_data->Renderer.Recompile(items[index]->Name);

				if ((hasPluginAddMenu || items[index]->Type == PipelineItem::ItemType::ShaderPass) && ImGui::BeginMenu("Add")) {
					if ((!isPlugin || pldata->Owner->PipelineItem_CanHaveChild(pldata->Type, pldata->PluginData, plugin::PipelineItemType::Geometry)) && ImGui::MenuItem("Geometry")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::Geometry);
					} else if ((!isPlugin || pldata->Owner->PipelineItem_CanHaveChild(pldata->Type, pldata->PluginData, plugin::PipelineItemType::Model)) && ImGui::MenuItem("3D Model")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::Model);
					} else if ((!isPlugin || pldata->Owner->PipelineItem_CanHaveChild(pldata->Type, pldata->PluginData, plugin::PipelineItemType::VertexBuffer)) && ImGui::MenuItem("Vertex Buffer")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::VertexBuffer);
					} else if ((!isPlugin || pldata->Owner->PipelineItem_CanHaveChild(pldata->Type, pldata->PluginData, plugin::PipelineItemType::RenderState)) && ImGui::MenuItem("Render State")) {
						m_isCreateViewOpened = true;
						m_createUI.SetOwner(items[index]->Name);
						m_createUI.SetType(PipelineItem::ItemType::RenderState);
					}

					if (!isPlugin)
						m_data->Plugins.ShowContextItems("shaderpass_add", (void*)items[index]);
					else if (pldata->Owner->PipelineItem_CanHaveChild(pldata->Type, pldata->PluginData, plugin::PipelineItemType::PluginItem)) {
						pipe::PluginItemData* piData = ((pipe::PluginItemData*)items[index]->Data);

						m_data->Plugins.ShowContextItems("pluginitem_add", (void*)piData->Type, piData->PluginData);
					}

					ImGui::EndMenu();
				}

				if ((hasPluginShaders || !isPlugin) && ImGui::BeginMenu("Edit Code")) {
					// TODO: show "File doesnt exist - want to create it?" msg box
					if (items[index]->Type == PipelineItem::ItemType::ShaderPass) {
						pipe::ShaderPass* passData = (pipe::ShaderPass*)(items[index]->Data);

						if (ImGui::MenuItem("Vertex") && m_data->Parser.FileExists(passData->VSPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Vertex);
						else if (ImGui::MenuItem("Pixel") && m_data->Parser.FileExists(passData->PSPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Pixel);
						else if (passData->GSUsed && ImGui::MenuItem("Geometry") && m_data->Parser.FileExists(passData->GSPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Geometry);
						else if (passData->TSUsed && ImGui::MenuItem("Tessellation control") && m_data->Parser.FileExists(passData->TCSPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::TessellationControl);
						else if (passData->TSUsed && ImGui::MenuItem("Tessellation evaluation") && m_data->Parser.FileExists(passData->TESPath))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::TessellationEvaluation);
						else if (ImGui::MenuItem("All")) {
							if (passData->GSUsed && m_data->Parser.FileExists(passData->GSPath))
								(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Geometry);

							if (passData->TSUsed) {
								if (m_data->Parser.FileExists(passData->TCSPath))
									(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::TessellationControl);
								if (m_data->Parser.FileExists(passData->TESPath))
									(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::TessellationEvaluation);
							}

							if (m_data->Parser.FileExists(passData->PSPath))
								(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Pixel);

							if (m_data->Parser.FileExists(passData->VSPath))
								(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Vertex);
						}
					} else if (items[index]->Type == PipelineItem::ItemType::ComputePass) {
						pipe::ComputePass* passData = (pipe::ComputePass*)(items[index]->Data);

						if (ImGui::MenuItem("Compute Shader") && m_data->Parser.FileExists(passData->Path))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Compute);
					} else if (items[index]->Type == PipelineItem::ItemType::AudioPass) {
						pipe::AudioPass* passData = (pipe::AudioPass*)(items[index]->Data);

						if (ImGui::MenuItem("Audio Shader") && m_data->Parser.FileExists(passData->Path))
							(reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)))->Open(items[index], ShaderStage::Pixel);
					} else if (items[index]->Type == PipelineItem::ItemType::PluginItem) {
						m_data->Plugins.ShowContextItems(pldata->Owner, "editcode", pldata->PluginData);
					}

					ImGui::EndMenu();
				}


				if (!m_data->Renderer.IsPaused()) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}
				if (items[index]->Type == PipelineItem::ItemType::ComputePass && ImGui::MenuItem("Debug")) {
					pipe::ComputePass* pass = (pipe::ComputePass*)items[index]->Data;

					m_isComputeDebugOpen = true;
					m_modalItem = items[index];

					m_localSizeX = m_localSizeY = m_localSizeZ = 1;
					m_groupsX = pass->WorkX;
					m_groupsY = pass->WorkY;
					m_groupsZ = pass->WorkZ;
					m_thread[0] = m_thread[1] = m_thread[2] = 0;
					m_computeLang = ed::ShaderCompiler::GetShaderLanguageFromExtension(pass->Path);

					if (pass->SPV.size() > 0) {
						SPIRVParser parser;
						parser.Parse(pass->SPV);

						m_localSizeX = parser.LocalSizeX;
						m_localSizeY = parser.LocalSizeY;
						m_localSizeZ = parser.LocalSizeZ;
					}
				}
				if (!m_data->Renderer.IsPaused()) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}

				if (!isPlugin && ImGui::MenuItem("Variables")) {
					m_isVarManagerOpened = true;
					m_modalItem = items[index];
				}

				if (items[index]->Type == PipelineItem::ItemType::ShaderPass && ImGui::MenuItem("Input layout")) {
					m_isInpLayoutManagerOpened = true;
					m_modalItem = items[index];
				}

				if (!isPlugin && ImGui::MenuItem("Macros")) {
					m_isMacroManagerOpened = true;
					m_modalItem = items[index];
				}

				if (!isPlugin && ImGui::MenuItem("Resources")) {
					m_isResourceManagerOpened = true;
					m_modalItem = items[index];
				}

			} else if (items[index]->Type == ed::PipelineItem::ItemType::Geometry || items[index]->Type == ed::PipelineItem::ItemType::Model || items[index]->Type == ed::PipelineItem::ItemType::VertexBuffer || items[index]->Type == ed::PipelineItem::ItemType::PluginItem) {
				bool proc = true;
				if (items[index]->Type == ed::PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* plData = (pipe::PluginItemData*)items[index]->Data;
					proc = plData->Owner->PipelineItem_CanChangeVariables(plData->Type, plData->PluginData);
				}

				if (proc && ImGui::MenuItem("Change Variables")) {
					m_isChangeVarsOpened = true;
					m_modalItem = items[index];
				}
			}

			if (hasPluginContext) {
				if (hasPluginAddMenu) ImGui::Separator();
				pldata->Owner->PipelineItem_ShowContext(pldata->Type, pldata->PluginData);
				ImGui::Separator();
			}

			if ((hasPluginProperties || !isPlugin) && ImGui::Selectable("Properties"))
				(reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties)))->Open(items[index]);

			if (ImGui::Selectable("Delete")) {
				bool requiresPopup = items[index]->Type == PipelineItem::ItemType::ShaderPass || items[index]->Type == PipelineItem::ItemType::ComputePass || items[index]->Type == PipelineItem::ItemType::AudioPass;

				if (requiresPopup) {
					m_isConfirmDeleteOpened = true;
					m_modalItem = items[index];
				} else {
					DeleteItem(items[index]);
					ret = true;
				}
			}

			ImGui::EndPopup();

			return ret;
		}

		return ret;
	}
	void PipelineUI::m_closePopup()
	{
		ImGui::CloseCurrentPopup();
		m_modalItem = nullptr;
	}

	void PipelineUI::m_tooltip(const std::string& text)
	{
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
	void PipelineUI::m_renderVarFlags(ed::ShaderVariable* var, char flags)
	{
		ShaderVariable::ValueType type = var->GetType();

		bool canInvert = type >= ShaderVariable::ValueType::Float2x2 && type <= ShaderVariable::ValueType::Float4x4;
		bool canLastFrame = var->System != ed::SystemShaderVariable::Time && var->System != ed::SystemShaderVariable::IsPicked && var->System != ed::SystemShaderVariable::IsSavingToFile && var->System != ed::SystemShaderVariable::ViewportSize;

		if (var->System == ed::SystemShaderVariable::PluginVariable)
			canLastFrame = canLastFrame || var->PluginSystemVarData.Owner->SystemVariables_HasLastFrame(var->PluginSystemVarData.Name, (plugin::VariableType)var->GetType());

		bool isInvert = var->Flags & (char)ShaderVariable::Flag::Inverse;
		bool isLastFrame = var->Flags & (char)ShaderVariable::Flag::LastFrame;

		if (var->System == ed::SystemShaderVariable::None || !canInvert) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		if (ImGui::Checkbox(("##flaginv" + std::string(var->Name)).c_str(), &isInvert))
			m_data->Parser.ModifyProject();
		m_tooltip("Invert");
		ImGui::SameLine();

		if (!canInvert && var->System != ed::SystemShaderVariable::None) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

		if (!canLastFrame) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		if (ImGui::Checkbox(("##flaglf" + std::string(var->Name)).c_str(), &isLastFrame))
			m_data->Parser.ModifyProject();
		m_tooltip("Use last frame values");

		if (var->System == ed::SystemShaderVariable::None || !canLastFrame) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

		var->Flags = (isInvert * (char)ShaderVariable::Flag::Inverse) | (isLastFrame * (char)ShaderVariable::Flag::LastFrame);
	}
	void PipelineUI::m_renderInputLayoutManagerUI()
	{
		static InputLayoutValue iValueType = InputLayoutValue::Position;
		static char semanticName[32];

		pipe::ShaderPass* itemData = (pipe::ShaderPass*)m_modalItem->Data;

		ImGui::TextWrapped("Add or remove vertex shader inputs.");

		ImGui::BeginChild("##pui_layout_table", ImVec2(0, Settings::Instance().CalculateSize(-25)));
		ImGui::Columns(4);

		ImGui::Text("Controls");
		ImGui::NextColumn();
		ImGui::Text("Location");
		ImGui::NextColumn();
		ImGui::Text("Value");
		ImGui::NextColumn();
		ImGui::Text("HLSL semantic");
		ImGui::NextColumn();

		// TODO: this is only a temprorary fix for non-resizable columns
		static bool isColumnWidthSet = false;
		if (!isColumnWidthSet) {
			ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(80));
			ImGui::SetColumnWidth(1, Settings::Instance().CalculateSize(60));
			isColumnWidthSet = true;
		}

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		std::vector<InputLayoutItem>& els = itemData->InputLayout;

		/* EXISTING INPUTS */
		for (auto& el : els) {
			ImGui::PushID(id);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			/* UP BUTTON */
			if (ImGui::Button(UI_ICON_ARROW_UP, BUTTON_ICON_SIZE) && id != 0) {
				InputLayoutItem temp = els[id - 1];
				els[id - 1] = el;
				els[id] = temp;

				m_data->Parser.ModifyProject();
			}
			ImGui::SameLine(0, 0);
			/* DOWN BUTTON */
			if (ImGui::Button(UI_ICON_ARROW_DOWN, BUTTON_ICON_SIZE) && id != els.size() - 1) {
				InputLayoutItem temp = els[id + 1];
				els[id + 1] = el;
				els[id] = temp;

				m_data->Parser.ModifyProject();
			}
			ImGui::SameLine(0, 0);
			/* DELETE BUTTON */
			if (ImGui::Button(UI_ICON_DELETE, BUTTON_ICON_SIZE)) {
				els.erase(els.begin() + id);

				m_data->Parser.ModifyProject();

				ImGui::PopStyleColor();
				ImGui::PopID();

				continue;
			}

			ImGui::PopStyleColor();
			ImGui::NextColumn();

			/* TYPE */
			ImGui::Text("%d", id);
			ImGui::NextColumn();

			/* VALUE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			const char* valueComboPreview = ATTRIBUTE_VALUE_NAMES[(int)el.Value];
			if (ImGui::BeginCombo("##value", valueComboPreview)) {
				for (int n = 0; n < HARRAYSIZE(ATTRIBUTE_VALUE_NAMES); n++) {
					bool is_selected = (n == (int)el.Value);
					if (ImGui::Selectable(ATTRIBUTE_VALUE_NAMES[n], is_selected)) {
						el.Value = (InputLayoutValue)n;
						m_data->Parser.ModifyProject();
					}
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::NextColumn();

			/* SEMANTIC */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Text("%s (currently not supported)", el.Semantic.c_str());
			ImGui::NextColumn();

			ImGui::PopID();
			id++;
		}

		ImGui::PopStyleColor();

		// widgets for editing "virtual" element - an element that will be added to the list later
		/* ADD BUTTON */
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button(UI_ICON_ADD)) {
			els.push_back({ iValueType, std::string(semanticName) });
			m_data->Parser.ModifyProject();
		}
		ImGui::NextColumn();
		ImGui::PopStyleColor();

		ImGui::Text("%d", (int)els.size());
		ImGui::NextColumn();

		/* TYPE */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Combo("##inputType", reinterpret_cast<int*>(&iValueType), ATTRIBUTE_VALUE_NAMES, HARRAYSIZE(ATTRIBUTE_VALUE_NAMES))) {
			/* Nothing to do here? */
		}
		ImGui::NextColumn();

		/* NAME */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText("##inputName", const_cast<char*>(semanticName), 32);
		ImGui::NextColumn();

		//ImGui::PopItemWidth();

		ImGui::EndChild();
		ImGui::Columns(1);
	}
	void PipelineUI::m_renderVariableManagerUI()
	{
		static ed::ShaderVariable iVariable(ed::ShaderVariable::ValueType::Float1, "var", ed::SystemShaderVariable::None);
		static ed::ShaderVariable::ValueType iValueType = ed::ShaderVariable::ValueType::Float1;
		static bool scrollToBottom = false;

		void* itemData = m_modalItem->Data;
		bool isCompute = m_modalItem->Type == PipelineItem::ItemType::ComputePass;
		bool isAudio = m_modalItem->Type == PipelineItem::ItemType::AudioPass;

		ShaderLanguage vsLang = ShaderCompiler::GetShaderLanguageFromExtension(((pipe::ShaderPass*)itemData)->VSPath);
		bool isGLSL = (vsLang == ShaderLanguage::GLSL) || (vsLang == ShaderLanguage::VulkanGLSL);

		ImGui::TextWrapped("Add or remove variables bound to this shader pass.");

		ImGui::BeginChild("##pui_variable_table", ImVec2(0, Settings::Instance().CalculateSize(-25)));
		ImGui::Columns(5);

		// TODO: this is only a temprorary fix for non-resizable columns
		static bool isColumnWidthSet = false;
		if (!isColumnWidthSet) {
			ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(6 * 20));
			ImGui::SetColumnWidth(3, Settings::Instance().CalculateSize(180));
			isColumnWidthSet = true;
		}

		ImGui::Text("Controls");
		ImGui::NextColumn();
		ImGui::Text("Type");
		ImGui::NextColumn();
		ImGui::Text("Name");
		ImGui::NextColumn();
		ImGui::Text("System");
		ImGui::NextColumn();
		ImGui::Text("Flags");
		ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		std::vector<ed::ShaderVariable*>& els = isCompute ? ((pipe::ComputePass*)itemData)->Variables.GetVariables() : (isAudio ? ((pipe::AudioPass*)itemData)->Variables.GetVariables() : ((pipe::ShaderPass*)itemData)->Variables.GetVariables());

		/* EXISTING VARIABLES */
		for (auto& el : els) {
			ImGui::PushID(id);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			/* UP BUTTON */
			if (ImGui::Button(UI_ICON_ARROW_UP, BUTTON_ICON_SIZE) && id != 0) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
				bool containsCur = pinState->Contains(el->Name);
				bool containsDown = pinState->Contains(els[id - 1]->Name);

				m_data->Parser.ModifyProject();

				// first unpin if it was pinned
				if (containsCur)
					pinState->Remove(el->Name);
				if (containsDown)
					pinState->Remove(els[id - 1]->Name);

				ed::ShaderVariable* temp = els[id - 1];
				els[id - 1] = el;
				els[id] = temp;

				// then pin again if it was previously pinned
				if (containsCur)
					pinState->Add(els[id - 1]);
				if (containsDown)
					pinState->Add(els[id]);
			}
			ImGui::SameLine(0, 0);
			/* DOWN BUTTON */
			if (ImGui::Button(UI_ICON_ARROW_DOWN, BUTTON_ICON_SIZE) && id != els.size() - 1) {
				// check if any of the affected variables are pinned
				PinnedUI* pinState = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
				bool containsCur = pinState->Contains(el->Name);
				bool containsDown = pinState->Contains(els[id + 1]->Name);

				m_data->Parser.ModifyProject();

				// first unpin if it was pinned
				if (containsCur)
					pinState->Remove(el->Name);
				if (containsDown)
					pinState->Remove(els[id + 1]->Name);

				ed::ShaderVariable* temp = els[id + 1];
				els[id + 1] = el;
				els[id] = temp;

				// then pin again if it was previously pinned
				if (containsCur)
					pinState->Add(els[id + 1]);
				if (containsDown)
					pinState->Add(els[id]);
			}
			ImGui::SameLine(0, 0);
			/* DELETE BUTTON */
			if (ImGui::Button(UI_ICON_DELETE, BUTTON_ICON_SIZE)) {
				((PinnedUI*)m_ui->Get(ViewID::Pinned))->Remove(el->Name); // unpin if pinned

				m_data->Parser.ModifyProject();

				if (isCompute)
					((pipe::ComputePass*)itemData)->Variables.Remove(el->Name);
				else if (isAudio)
					((pipe::AudioPass*)itemData)->Variables.Remove(el->Name);
				else {
					// remove item variable values
					auto& itemVarValues = m_data->Renderer.GetItemVariableValues();
					const auto& vars = ((pipe::ShaderPass*)itemData)->Variables.GetVariables();
					ed::ShaderVariable* var = nullptr;

					for (auto v : vars)
						if (strcmp(v->Name, el->Name) == 0) {
							var = v;
							break;
						}

					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Variable == var) {
							itemVarValues.erase(itemVarValues.begin() + k);
							k--;
						}

					// remove actual variable
					((pipe::ShaderPass*)itemData)->Variables.Remove(el->Name);
				}

				ImGui::PopStyleColor();
				ImGui::PopID();
				continue;
			}
			ImGui::SameLine(0, 0);
			/* EDIT & PIN BUTTONS */
			if (el->System == ed::SystemShaderVariable::None) {
				if (ImGui::Button(UI_ICON_EDIT "##inputEdit", BUTTON_ICON_SIZE)) {
					ImGui::PopID();

					ImGui::OpenPopup("Value Edit##pui_shader_value_edit");
					m_valueEdit.Open(el);

					ImGui::PushID(id);
				}
				ImGui::SameLine(0, 0);

				PinnedUI* pinState = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
				if (!pinState->Contains(el->Name)) {
					if (ImGui::Button(UI_ICON_ADD, BUTTON_ICON_SIZE)) {
						pinState->Add(el);
						m_data->Parser.ModifyProject();
					}
					m_tooltip("Pin");
				} else {
					if (ImGui::Button(UI_ICON_REMOVE, BUTTON_ICON_SIZE)) {
						pinState->Remove(el->Name);
						m_data->Parser.ModifyProject();
					}
					m_tooltip("Unpin");
				}

				ImGui::SameLine();
			}

			ImGui::PopStyleColor();
			ImGui::NextColumn();

			/* TYPE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ShaderVariable::ValueType tempType = el->GetType();
			if (ImGui::Combo("##inputType", reinterpret_cast<int*>(&tempType), isGLSL ? VARIABLE_TYPE_NAMES_GLSL : VARIABLE_TYPE_NAMES, HARRAYSIZE(VARIABLE_TYPE_NAMES)))
				if (tempType != el->GetType())
					el->SetType(tempType);
			ImGui::NextColumn();

			/* NAME */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::InputText("##name", const_cast<char*>(el->Name), VARIABLE_NAME_LENGTH)) {
				m_data->Parser.ModifyProject();
			}
			ImGui::NextColumn();

			/* SYSTEM VALUE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			const char* systemComboPreview = el->System == SystemShaderVariable::PluginVariable ? (el->PluginSystemVarData.Name) : (SYSTEM_VARIABLE_NAMES[(int)el->System]);

			if (ImGui::BeginCombo("##system", systemComboPreview)) {
				bool removeFromPins = false;
				for (int n = 0; n < HARRAYSIZE(SYSTEM_VARIABLE_NAMES); n++) {
					bool is_selected = (n == (int)el->System);
					if (n != (int)SystemShaderVariable::PluginVariable) {
						if ((n == 0 || ed::SystemVariableManager::GetType((ed::SystemShaderVariable)n) == el->GetType())
							&& ImGui::Selectable(SYSTEM_VARIABLE_NAMES[n], is_selected)) {
							el->System = (ed::SystemShaderVariable)n;
							m_data->Parser.ModifyProject();
							removeFromPins = true;
						}
					} else {
						bool modified = m_data->Plugins.ShowSystemVariables(&el->PluginSystemVarData, el->GetType());
						if (modified) {
							m_data->Parser.ModifyProject();
							el->System = SystemShaderVariable::PluginVariable;
							removeFromPins = true;
						}
					}
					if (is_selected)
						ImGui::SetItemDefaultFocus();
					if (removeFromPins && n != 0) {
						PinnedUI* pinState = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
						if (pinState->Contains(el->Name))
							pinState->Remove(el->Name);
						removeFromPins = false;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::NextColumn();

			/* FLAGS */
			ImGui::PopStyleColor();
			m_renderVarFlags(el, el->Flags);
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
			ImGui::NextColumn();

			ImGui::PopID();
			id++;
		}

		ImGui::PopStyleColor();

		// render value edit window if needed
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(450), Settings::Instance().CalculateSize(200)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Value Edit##pui_shader_value_edit")) {
			m_valueEdit.Update();

			if (ImGui::Button("Ok")) {
				m_data->Parser.ModifyProject();
				m_valueEdit.Close();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// widgets for editing "virtual" element - an element that will be added to the list later
		/* ADD BUTTON */
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button(UI_ICON_ADD)) {
			// cant have two variables with same name
			bool exists = false;
			for (const auto& el : els)
				if (strcmp(el->Name, iVariable.Name) == 0)
					exists = true;

			// add if it doesnt exist
			if (!exists) {
				if (isCompute)
					((pipe::ComputePass*)itemData)->Variables.AddCopy(iVariable);
				else if (isAudio)
					((pipe::AudioPass*)itemData)->Variables.AddCopy(iVariable);
				else
					((pipe::ShaderPass*)itemData)->Variables.AddCopy(iVariable);

				iVariable = ShaderVariable(ShaderVariable::ValueType::Float1, "var", ed::SystemShaderVariable::None);
				iValueType = ShaderVariable::ValueType::Float1;
				scrollToBottom = true;

				m_data->Parser.ModifyProject();
			}
		}
		ImGui::NextColumn();
		ImGui::PopStyleColor();

		/* TYPE */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Combo("##createVarType", reinterpret_cast<int*>(&iValueType), isGLSL ? VARIABLE_TYPE_NAMES_GLSL : VARIABLE_TYPE_NAMES, HARRAYSIZE(VARIABLE_TYPE_NAMES))) {
			if (iValueType != iVariable.GetType()) {
				ed::ShaderVariable newVariable(iValueType);
				memcpy(newVariable.Name, iVariable.Name, strlen(iVariable.Name));
				newVariable.System = ed::SystemShaderVariable::None;
				memcpy(newVariable.Data, iVariable.Data, std::min<int>(ed::ShaderVariable::GetSize(iVariable.GetType()), ed::ShaderVariable::GetSize(newVariable.GetType())));

				free(iVariable.Data);
				iVariable = newVariable;
			}
		}
		ImGui::NextColumn();

		/* NAME */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText("##createVarName", const_cast<char*>(iVariable.Name), VARIABLE_NAME_LENGTH);
		ImGui::NextColumn();

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		/* SYSTEM */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		const char* inputComboPreview = iVariable.System == SystemShaderVariable::PluginVariable ?
			iVariable.PluginSystemVarData.Name :
			SYSTEM_VARIABLE_NAMES[(int)iVariable.System];
		if (ImGui::BeginCombo("##createVarsystem", inputComboPreview)) {
			for (int n = 0; n < (int)SystemShaderVariable::Count; n++) {
				bool is_selected = (n == (int)iVariable.System);
				if (n != (int)SystemShaderVariable::PluginVariable) {
					if ((n == 0 || ed::SystemVariableManager::GetType((ed::SystemShaderVariable)n) == iVariable.GetType())
						&& ImGui::Selectable(SYSTEM_VARIABLE_NAMES[n], is_selected))
						iVariable.System = (ed::SystemShaderVariable)n;
				} else {
					bool modified = m_data->Plugins.ShowSystemVariables(&iVariable.PluginSystemVarData, iVariable.GetType());
					if (modified) iVariable.System = SystemShaderVariable::PluginVariable;
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::NextColumn();

		/* FLAGS */
		m_renderVarFlags(&iVariable, iVariable.Flags);
		ImGui::NextColumn();

		//ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::EndChild();
	}
	void PipelineUI::m_renderChangeVariablesUI()
	{
		static bool scrollToBottom = false;
		static int shaderTypeSel = 0, shaderVarSel = 0;

		ImGui::TextWrapped("Add variables that you want to change when rendering this item");

		ImGui::BeginChild("##pui_cvar_table", ImVec2(0, Settings::Instance().CalculateSize(-25)));
		ImGui::Columns(2);

		ImGui::Text("Name");
		ImGui::NextColumn();
		ImGui::Text("Value");
		ImGui::NextColumn();

		ImGui::Separator();

		// find this item's owner
		PipelineItem* owner = nullptr;
		pipe::ShaderPass* ownerData = nullptr;
		std::vector<PipelineItem*>& passes = m_data->Pipeline.GetList();
		for (PipelineItem* pass : passes) {
			if (pass->Type != PipelineItem::ItemType::ShaderPass)
				continue;

			ownerData = ((pipe::ShaderPass*)pass->Data);
			std::vector<PipelineItem*>& children = ownerData->Items;
			for (PipelineItem* child : children) {
				if (child == m_modalItem) {
					owner = pass;
					break;
				}
			}
			if (owner != nullptr)
				break;
		}

		std::vector<ShaderVariable*>& vars = ownerData->Variables.GetVariables();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		// render the list
		int id = 0;
		std::vector<RenderEngine::ItemVariableValue> allItems = m_data->Renderer.GetItemVariableValues();
		for (auto& i : allItems) {
			if (i.Item != m_modalItem)
				continue;

			ImGui::PushID(id);

			/* NAME */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Text(i.Variable->Name);
			ImGui::NextColumn();

			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::Button(UI_ICON_EDIT, BUTTON_ICON_SIZE)) {
				ImGui::PopID();

				ImGui::OpenPopup("Value Edit##pui_shader_ivalue_edit");
				m_valueEdit.Open(i.NewValue);

				ImGui::PushID(id);
			}
			ImGui::SameLine();
			if (ImGui::Button(UI_ICON_DELETE, BUTTON_ICON_SIZE)) {
				m_data->Renderer.RemoveItemVariableValue(i.Item, i.Variable);
				m_data->Parser.ModifyProject();
			}
			ImGui::NextColumn();

			ImGui::PopID();
			id++;
		}

		ImGui::PopStyleColor();

		// render value edit window if needed
		ImGui::SetNextWindowSize(ImVec2(450, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Value Edit##pui_shader_ivalue_edit")) {
			m_valueEdit.Update();

			if (ImGui::Button("Ok")) {
				m_data->Parser.ModifyProject();
				m_valueEdit.Close();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		shaderVarSel = std::min<int>(shaderVarSel, vars.size()-1);
		const char* inputComboPreview = vars.size() > 0 ? vars[shaderVarSel]->Name : "-- NONE --";
		if (ImGui::BeginCombo("##itemvarname", inputComboPreview)) {
			for (int n = 0; n < vars.size(); n++) {
				bool is_selected = (n == shaderVarSel);
				if (ImGui::Selectable(vars[n]->Name, is_selected))
					shaderVarSel = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button(UI_ICON_ADD, BUTTON_ICON_SIZE) && vars.size() > 0) {
			bool alreadyAdded = false;
			for (int i = 0; i < allItems.size(); i++)
				if (strcmp(vars[shaderVarSel]->Name, allItems[i].Variable->Name) == 0 && allItems[i].Item == m_modalItem) {
					alreadyAdded = true;
					break;
				}

			if (!alreadyAdded) {
				RenderEngine::ItemVariableValue itemVal(vars[shaderVarSel]);
				itemVal.Item = m_modalItem;

				m_data->Renderer.AddItemVariableValue(itemVal);
				m_data->Parser.ModifyProject();
			}
		}
		ImGui::NextColumn();

		ImGui::EndChild();
		ImGui::Columns(1);
	}
	void PipelineUI::m_renderResourceManagerUI()
	{
		ImGui::TextWrapped("List of all bound textures, images, etc...\n");

		ObjectManager* objs = &m_data->Objects;

		ImGui::BeginChild("##pui_res_ubo_table", ImVec2(0, Settings::Instance().CalculateSize(-25)));

		// SRVs
		{
			ImGui::Text("SRV");
			ImGui::Separator();
			ImGui::Columns(4);

			// TODO: remove this after imgui fixes the table/column system
			static bool isColumnWidthSet = false;
			if (!isColumnWidthSet) {
				ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(100));
				ImGui::SetColumnWidth(1, Settings::Instance().CalculateSize(60));
				ImGui::SetColumnWidth(2, Settings::Instance().CalculateSize(80));

				isColumnWidthSet = true;
			}

			ImGui::Text("Controls");
			ImGui::NextColumn();
			ImGui::Text("Unit");
			ImGui::NextColumn();
			ImGui::Text("Type");
			ImGui::NextColumn();
			ImGui::Text("Name");
			ImGui::NextColumn();

			ImGui::Separator();

			int id = 0;
			std::vector<GLuint>& els = m_data->Objects.GetBindList(m_modalItem);

			/* EXISTING VARIABLES */
			for (const auto& el : els) {
				ImGui::PushID(id);
				ObjectManagerItem* resData = m_data->Objects.GetByTextureID(el);
				if (resData == nullptr)
					resData = m_data->Objects.GetByBufferID(el);
				if (resData == nullptr)
					continue;

				/* CONTROLS */
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				/* UP BUTTON */
				if (ImGui::Button(UI_ICON_ARROW_UP "##s") && id != 0) {
					GLuint temp = els[id - 1];
					els[id - 1] = el;
					els[id] = temp;
					m_data->Parser.ModifyProject();
				}
				ImGui::SameLine(0, 0);
				/* DOWN BUTTON */
				if (ImGui::Button(UI_ICON_ARROW_DOWN "##s") && id != els.size() - 1) {
					GLuint temp = els[id + 1];
					els[id + 1] = el;
					els[id] = temp;
					m_data->Parser.ModifyProject();
				}
				ImGui::PopStyleColor();
				ImGui::NextColumn();

				/* ACTIVE */
				ImGui::Text("%d", id);
				ImGui::NextColumn();

				/* NAME */
				if (resData->Type == ObjectType::Image)
					ImGui::Text("image");
				else if (resData->Type == ObjectType::Image3D)
					ImGui::Text("image3D");
				else if (resData->Type == ObjectType::RenderTexture)
					ImGui::Text("render texture");
				else if (resData->Type == ObjectType::Audio)
					ImGui::Text("audio");
				else if (resData->Type == ObjectType::Buffer)
					ImGui::Text("buffer");
				else if (resData->Type == ObjectType::CubeMap)
					ImGui::Text("cubemap");
				else if (resData->Type == ObjectType::Texture3D)
					ImGui::Text("texture3D");
				else
					ImGui::Text("texture");
				ImGui::NextColumn();

				/* VALUE */
				ImGui::Text("%s", resData->Name.c_str());
				ImGui::NextColumn();

				ImGui::PopID();
				id++;
			}

			ImGui::Columns(1);
		}

		ImGui::NewLine();

		// UAVs
		{
			ImGui::Text("UAVs / UBOs");
			ImGui::Separator();
			ImGui::Columns(4);

			// TODO: remove this after imgui fixes the table/column system
			static bool isColumnWidthSet = false;
			if (!isColumnWidthSet) {
				ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(100));
				ImGui::SetColumnWidth(1, Settings::Instance().CalculateSize(60));
				ImGui::SetColumnWidth(2, Settings::Instance().CalculateSize(80));

				isColumnWidthSet = true;
			}

			ImGui::Text("Controls");
			ImGui::NextColumn();
			ImGui::Text("Unit");
			ImGui::NextColumn();
			ImGui::Text("Type");
			ImGui::NextColumn();
			ImGui::Text("Name");
			ImGui::NextColumn();

			ImGui::Separator();

			int id = 0;
			std::vector<GLuint>& els = m_data->Objects.GetUniformBindList(m_modalItem);

			/* EXISTING VARIABLES */
			for (const auto& el : els) {
				ImGui::PushID(id);
				ObjectManagerItem* resData = m_data->Objects.GetByTextureID(el);
				if (resData == nullptr)
					resData = m_data->Objects.GetByBufferID(el);
				if (resData == nullptr)
					continue;

				/* CONTROLS */
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				/* UP BUTTON */
				if (ImGui::Button(UI_ICON_ARROW_UP "##u") && id != 0) {
					GLuint temp = els[id - 1];
					els[id - 1] = el;
					els[id] = temp;
					m_data->Parser.ModifyProject();
				}
				ImGui::SameLine(0, 0);
				/* DOWN BUTTON */
				if (ImGui::Button(UI_ICON_ARROW_DOWN "##u") && id != els.size() - 1) {
					GLuint temp = els[id + 1];
					els[id + 1] = el;
					els[id] = temp;
					m_data->Parser.ModifyProject();
				}
				ImGui::PopStyleColor();
				ImGui::NextColumn();

				/* ACTIVE */
				ImGui::Text("%d", id);
				ImGui::NextColumn();

				/* NAME */
				if (resData->Type == ObjectType::Image)
					ImGui::Text("image");
				else if (resData->Type == ObjectType::Image3D)
					ImGui::Text("image3D");
				else if (resData->Type == ObjectType::RenderTexture)
					ImGui::Text("render texture");
				else if (resData->Type == ObjectType::Audio)
					ImGui::Text("audio");
				else if (resData->Type == ObjectType::Buffer)
					ImGui::Text("buffer");
				else if (resData->Type == ObjectType::CubeMap)
					ImGui::Text("cubemap");
				else
					ImGui::Text("texture");
				ImGui::NextColumn();

				/* VALUE */
				ImGui::Text("%s", resData->Name.c_str());
				ImGui::NextColumn();


				ImGui::PopID();
				id++;
			}

			ImGui::Columns(1);
		}

		ImGui::EndChild();
	}
	void PipelineUI::m_renderMacroManagerUI()
	{
		static ShaderMacro addMacro = { true, "\0", "\0" };
		static bool scrollToBottom = false;

		ImGui::TextWrapped("Add or remove shader macros.");

		ImGui::BeginChild("##pui_macro_table", ImVec2(0, Settings::Instance().CalculateSize(-25)));
		ImGui::Columns(4);

		ImGui::Text("Controls");
		ImGui::NextColumn();
		ImGui::Text("Active");
		ImGui::NextColumn();
		ImGui::Text("Name");
		ImGui::NextColumn();
		ImGui::Text("Value");
		ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		bool isCompute = m_modalItem->Type == PipelineItem::ItemType::ComputePass;
		bool isAudio = m_modalItem->Type == PipelineItem::ItemType::AudioPass;
		std::vector<ShaderMacro>& els = isCompute ? ((ed::pipe::ComputePass*)m_modalItem->Data)->Macros : (isAudio ? ((ed::pipe::AudioPass*)m_modalItem->Data)->Macros : ((ed::pipe::ShaderPass*)m_modalItem->Data)->Macros);

		/* EXISTING VARIABLES */
		for (auto& el : els) {
			ImGui::PushID(id);

			/* CONTROLS */
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			/* UP BUTTON */
			if (ImGui::Button(UI_ICON_ARROW_UP) && id != 0) {
				ed::ShaderMacro temp = els[id - 1];
				els[id - 1] = el;
				els[id] = temp;
				m_data->Parser.ModifyProject();
			}
			ImGui::SameLine(0, 0);
			/* DOWN BUTTON */
			if (ImGui::Button(UI_ICON_ARROW_DOWN) && id != els.size() - 1) {
				ed::ShaderMacro temp = els[id + 1];
				els[id + 1] = el;
				els[id] = temp;
				m_data->Parser.ModifyProject();
			}
			ImGui::SameLine(0, 0);
			/* DELETE BUTTON */
			if (ImGui::Button(UI_ICON_DELETE)) {
				els.erase(els.begin() + id);
				id--;
				m_data->Parser.ModifyProject();
			}
			ImGui::PopStyleColor();
			ImGui::NextColumn();

			/* ACTIVE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::Checkbox("##pui_mcr_act", &el.Active))
				m_data->Parser.ModifyProject();
			ImGui::NextColumn();

			/* NAME */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::InputText("##mcrName", el.Name, 32))
				m_data->Parser.ModifyProject();
			ImGui::NextColumn();

			/* VALUE */
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::InputText("##mcrVal", el.Value, 512))
				m_data->Parser.ModifyProject();
			ImGui::NextColumn();


			ImGui::PopID();
			id++;
		}

		ImGui::PopStyleColor();

		// widgets for adding a macro
		/* ADD BUTTON */
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		if (ImGui::Button(UI_ICON_ADD)) {
			// cant have two macros with same name
			bool exists = false;
			for (auto& el : els)
				if (strcmp(el.Name, addMacro.Name) == 0)
					exists = true;

			// add if it doesnt exist
			if (!exists) {
				els.push_back(addMacro);
				scrollToBottom = true;
				m_data->Parser.ModifyProject();
			}
		}
		ImGui::NextColumn();
		ImGui::PopStyleColor();
		ImGui::PopItemWidth();

		/* ACTIVE */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::Checkbox("##pui_mcrActAdd", &addMacro.Active);
		ImGui::NextColumn();

		/* NAME */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText("##mcrNameAdd", addMacro.Name, 32);
		ImGui::NextColumn();

		/* VALUE */
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
		ImGui::InputText("##mcrValAdd", addMacro.Value, 512);
		ImGui::NextColumn();

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		ImGui::EndChild();
		ImGui::Columns(1);
	}

	void PipelineUI::m_addShaderPass(ed::PipelineItem* item)
	{
		ed::pipe::ShaderPass* data = (ed::pipe::ShaderPass*)item->Data;

		std::string expandTxt = UI_ICON_UP;
		for (int i = 0; i < m_expandList.size(); i++)
			if (m_expandList[i] == data) {
				expandTxt = UI_ICON_DOWN;
				break;
			}

		expandTxt += "##passexp_" + std::string(item->Name);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::Button(std::string(std::string(data->Active ? UI_ICON_EYE : UI_ICON_EYE_BLOCKED) + "##hide" + std::string(item->Name)).c_str(), BUTTON_ICON_SIZE)) {
			data->Active = !data->Active;
			m_data->Parser.ModifyProject();
		}
		ImGui::SameLine(0, 0);
		if (ImGui::Button(expandTxt.c_str(), BUTTON_ICON_SIZE)) {
			if (expandTxt.find(UI_ICON_DOWN) != std::string::npos) {
				for (int i = 0; i < m_expandList.size(); i++)
					if (m_expandList[i] == data) {
						m_expandList.erase(m_expandList.begin() + i);
						break;
					}
			} else
				m_expandList.push_back(data);
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();

		int ewCount = m_data->Messages.GetGroupErrorAndWarningMsgCount(item->Name);
		if (ewCount > 0)
			ImGui::PushStyleColor(ImGuiCol_Text, ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme)[(int)TextEditor::PaletteIndex::ErrorMessage]);

		if (!data->Active)
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		ImGui::Indent(PIPELINE_SHADER_PASS_INDENT);
		if (ImGui::Selectable(item->Name, false, ImGuiSelectableFlags_AllowDoubleClick))
			if (ImGui::IsMouseDoubleClicked(0)) {
				if (Settings::Instance().General.OpenShadersOnDblClk) {
					if (Settings::Instance().General.UseExternalEditor && m_data->Parser.GetOpenedFile() == "")
						m_ui->SaveAsProject(true);
					else {
						CodeEditorUI* editor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						if (m_data->Parser.FileExists(data->VSPath))
							editor->Open(item, ShaderStage::Vertex);

						if (m_data->Parser.FileExists(data->PSPath))
							editor->Open(item, ShaderStage::Pixel);

						if (data->GSUsed && strlen(data->GSPath) > 0 && m_data->Parser.FileExists(data->GSPath))
							editor->Open(item, ShaderStage::Geometry);

						if (data->TSUsed) {
							if (strlen(data->TCSPath) > 0 && m_data->Parser.FileExists(data->TCSPath))
								editor->Open(item, ShaderStage::TessellationControl);
							if (strlen(data->TESPath) > 0 && m_data->Parser.FileExists(data->TESPath))
								editor->Open(item, ShaderStage::TessellationEvaluation);
						}
					}
				}

				if (Settings::Instance().General.ItemPropsOnDblCLk) {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties));
					props->Open(item);
				}
			}

		if (ImGui::BeginDragDropTarget()) {
			// pipeline item
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PipelineItemPayload")) {
				// TODO: m_data->Pipeline.DuplicateItem() ?
				ed::PipelineItem* dropItem = *(reinterpret_cast<ed::PipelineItem**>(payload->Data));
				bool duplicate = ImGui::GetIO().KeyCtrl;

				// first find a name that is not used
				std::string name = std::string(dropItem->Name);

				if (duplicate) { // change
					// remove numbers at the end of the string
					size_t lastOfLetter = std::string::npos;
					for (size_t j = name.size() - 1; j > 0; j--)
						if (!std::isdigit(name[j])) {
							lastOfLetter = j + 1;
							break;
						}
					if (lastOfLetter != std::string::npos)
						name = name.substr(0, lastOfLetter);

					// add number to the string and check if it already exists
					for (size_t j = 2; /*WE WILL BREAK FROM INSIDE ONCE WE FIND THE NAME*/; j++) {
						std::string newName = name + std::to_string(j);
						bool has = m_data->Pipeline.Has(newName.c_str());

						if (!has) {
							name = newName;
							break;
						}
					}
				}

				// get item owner
				void* itemData = nullptr;
				PipelineItem::ItemType itemType = dropItem->Type;

				// once we found a name, duplicate the properties:
				// duplicate geometry object:
				if (dropItem->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* newData = new pipe::GeometryItem();
					pipe::GeometryItem* origData = (pipe::GeometryItem*)dropItem->Data;

					newData->Position = origData->Position;
					newData->Rotation = origData->Rotation;
					newData->Scale = origData->Scale;
					newData->Size = origData->Size;
					newData->Topology = origData->Topology;
					newData->Type = origData->Type;

					if (newData->Type == pipe::GeometryItem::GeometryType::Cube)
						newData->VAO = eng::GeometryFactory::CreateCube(newData->VBO, newData->Size.x, newData->Size.y, newData->Size.z, data->InputLayout);
					else if (newData->Type == pipe::GeometryItem::Circle) {
						newData->VAO = eng::GeometryFactory::CreateCircle(newData->VBO, newData->Size.x, newData->Size.y, data->InputLayout);
						newData->Topology = GL_TRIANGLE_STRIP;
					} else if (newData->Type == pipe::GeometryItem::Plane)
						newData->VAO = eng::GeometryFactory::CreatePlane(newData->VBO, newData->Size.x, newData->Size.y, data->InputLayout);
					else if (newData->Type == pipe::GeometryItem::Rectangle)
						newData->VAO = eng::GeometryFactory::CreatePlane(newData->VBO, 1, 1, data->InputLayout);
					else if (newData->Type == pipe::GeometryItem::Sphere)
						newData->VAO = eng::GeometryFactory::CreateSphere(newData->VBO, newData->Size.x, data->InputLayout);
					else if (newData->Type == pipe::GeometryItem::Triangle)
						newData->VAO = eng::GeometryFactory::CreateTriangle(newData->VBO, newData->Size.x, data->InputLayout);
					else if (newData->Type == pipe::GeometryItem::ScreenQuadNDC)
						newData->VAO = eng::GeometryFactory::CreateScreenQuadNDC(newData->VBO, data->InputLayout);

					itemData = newData;
				}

				// duplicate Model:
				else if (dropItem->Type == PipelineItem::ItemType::Model) {
					pipe::Model* newData = new pipe::Model();
					pipe::Model* origData = (pipe::Model*)dropItem->Data;

					strcpy(newData->Filename, origData->Filename);
					strcpy(newData->GroupName, origData->GroupName);
					newData->OnlyGroup = origData->OnlyGroup;
					newData->Scale = origData->Scale;
					newData->Position = origData->Position;
					newData->Rotation = origData->Rotation;

					if (strlen(newData->Filename) > 0) {
						std::string objMem = m_data->Parser.LoadProjectFile(newData->Filename);
						eng::Model* mdl = m_data->Parser.LoadModel(newData->Filename);

						bool loaded = mdl != nullptr;
						if (loaded)
							newData->Data = mdl;
						else
							m_data->Messages.Add(ed::MessageStack::Type::Error, item->Name, "Failed to create .obj model " + std::string(item->Name));
					}

					itemData = newData;
				}

				// duplicate VertexBuffer:
				else if (dropItem->Type == PipelineItem::ItemType::VertexBuffer) {
					pipe::VertexBuffer* newData = new pipe::VertexBuffer();
					pipe::VertexBuffer* origData = (pipe::VertexBuffer*)dropItem->Data;

					newData->Scale = origData->Scale;
					newData->Position = origData->Position;
					newData->Rotation = origData->Rotation;
					newData->Topology = origData->Topology;
					newData->Buffer = origData->Buffer;

					if (newData->Buffer != 0)
						gl::CreateBufferVAO(newData->VAO, ((ed::BufferObject*)newData->Buffer)->ID, m_data->Objects.ParseBufferFormat(((ed::BufferObject*)newData->Buffer)->ViewFormat));

					itemData = newData;
				}

				// duplicate RenderState:
				else if (dropItem->Type == PipelineItem::ItemType::RenderState) {
					pipe::RenderState* newData = new pipe::RenderState();
					pipe::RenderState* origData = (pipe::RenderState*)dropItem->Data;
					memcpy(newData, origData, sizeof(pipe::RenderState));

					itemData = newData;
				}

				// duplicate PluginItem:
				else if (dropItem->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* newData = new pipe::PluginItemData();
					pipe::PluginItemData* origData = (pipe::PluginItemData*)dropItem->Data;

					strcpy(newData->Type, origData->Type);
					newData->Items = origData->Items; // even tho this should be empty
					newData->Owner = origData->Owner;

					newData->PluginData = origData->Owner->PipelineItem_CopyData(origData->Type, origData->PluginData);

					itemData = newData;
				}

				if (!duplicate) {
					PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
					if (props->HasItemSelected() && props->CurrentItemName() == dropItem->Name)
						props->Close();

					PreviewUI* prev = ((PreviewUI*)m_ui->Get(ViewID::Preview));
					if (prev->IsPicked(dropItem))
						prev->Pick(nullptr);

					m_data->Pipeline.Remove(dropItem->Name);
				}

				m_data->Pipeline.AddItem(item->Name, name.c_str(), itemType, itemData);
			}
			
			// object
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ObjectPayload")) {
				ed::ObjectManagerItem* dropItem = *(reinterpret_cast<ed::ObjectManagerItem**>(payload->Data));
				m_handleObjectDrop(item, dropItem);
			}
			
			ImGui::EndDragDropTarget();
		}

		ImGui::Unindent(PIPELINE_SHADER_PASS_INDENT);

		if (!data->Active)
			ImGui::PopStyleVar();

		if (ewCount > 0)
			ImGui::PopStyleColor();
	}
	void PipelineUI::m_addComputePass(ed::PipelineItem* item)
	{
		ed::pipe::ComputePass* data = (ed::pipe::ComputePass*)item->Data;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::Button(std::string(std::string(data->Active ? UI_ICON_EYE : UI_ICON_EYE_BLOCKED) + "##hide" + std::string(item->Name)).c_str(), BUTTON_ICON_SIZE)) {
			data->Active = !data->Active;
			m_data->Parser.ModifyProject();
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();

		int ewCount = m_data->Messages.GetGroupErrorAndWarningMsgCount(item->Name);
		if (ewCount > 0)
			ImGui::PushStyleColor(ImGuiCol_Text, ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme)[(int)TextEditor::PaletteIndex::ErrorMessage]);
		else
			ImGui::PushStyleColor(ImGuiCol_Text, ThemeContainer::Instance().GetCustomStyle(Settings::Instance().Theme).ComputePass);
		if (!data->Active)
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);


		ImGui::Indent(PIPELINE_SHADER_PASS_INDENT);
		if (ImGui::Selectable(item->Name, false, ImGuiSelectableFlags_AllowDoubleClick))
			if (ImGui::IsMouseDoubleClicked(0)) {
				if (Settings::Instance().General.OpenShadersOnDblClk) {
					if (Settings::Instance().General.UseExternalEditor && m_data->Parser.GetOpenedFile() == "")
						m_ui->SaveAsProject(true);
					else {
						CodeEditorUI* editor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						if (m_data->Parser.FileExists(data->Path))
							editor->Open(item, ShaderStage::Compute);
					}
				}

				if (Settings::Instance().General.ItemPropsOnDblCLk) {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties));
					props->Open(item);
				}
			}
		ImGui::Unindent(PIPELINE_SHADER_PASS_INDENT);

		if (ImGui::BeginDragDropTarget()) {
			// object
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ObjectPayload")) {
				ed::ObjectManagerItem* dropItem = *(reinterpret_cast<ed::ObjectManagerItem**>(payload->Data));
				m_handleObjectDrop(item, dropItem);
			}

			ImGui::EndDragDropTarget();
		}

		if (!data->Active)
			ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}
	void PipelineUI::m_addAudioPass(ed::PipelineItem* item)
	{
		ed::pipe::AudioPass* data = (ed::pipe::AudioPass*)item->Data;

		int ewCount = m_data->Messages.GetGroupErrorAndWarningMsgCount(item->Name);
		if (ewCount > 0)
			ImGui::PushStyleColor(ImGuiCol_Text, ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme)[(int)TextEditor::PaletteIndex::ErrorMessage]);
		else
			ImGui::PushStyleColor(ImGuiCol_Text, ThemeContainer::Instance().GetCustomStyle(Settings::Instance().Theme).ComputePass);

		ImGui::Indent(PIPELINE_SHADER_PASS_INDENT);
		if (ImGui::Selectable(item->Name, false, ImGuiSelectableFlags_AllowDoubleClick))
			if (ImGui::IsMouseDoubleClicked(0)) {
				if (Settings::Instance().General.OpenShadersOnDblClk) {
					if (Settings::Instance().General.UseExternalEditor && m_data->Parser.GetOpenedFile() == "")
						m_ui->SaveAsProject(true);
					else {
						CodeEditorUI* editor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						if (m_data->Parser.FileExists(data->Path))
							editor->Open(item, ShaderStage::Pixel);
					}
				}

				if (Settings::Instance().General.ItemPropsOnDblCLk) {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties));
					props->Open(item);
				}
			}
		ImGui::Unindent(PIPELINE_SHADER_PASS_INDENT);
		ImGui::PopStyleColor();
	}
	void PipelineUI::m_addPluginItem(ed::PipelineItem* item)
	{
		ed::pipe::PluginItemData* data = (ed::pipe::PluginItemData*)item->Data;

		ImGui::Indent(PIPELINE_SHADER_PASS_INDENT);
		if (ImGui::Selectable(item->Name, false, ImGuiSelectableFlags_AllowDoubleClick))
			if (ImGui::IsMouseDoubleClicked(0)) {
				if (Settings::Instance().General.OpenShadersOnDblClk && data->Owner->PipelineItem_HasShaders(data->Type, data->PluginData)) {
					if (Settings::Instance().General.UseExternalEditor && m_data->Parser.GetOpenedFile() == "")
						m_ui->SaveAsProject(true);
					else {
						CodeEditorUI* editor = (reinterpret_cast<CodeEditorUI*>(m_ui->Get(ViewID::Code)));
						data->Owner->PipelineItem_OpenInEditor(data->Type, data->PluginData);
					}
				}

				if (Settings::Instance().General.ItemPropsOnDblCLk && data->Owner->PipelineItem_HasProperties(data->Type, data->PluginData)) {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties));
					props->Open(item);
				}
			}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PipelineItemPayload")) {
				// TODO: m_data->Pipeline.DuplicateItem() ?
				ed::PipelineItem* dropItem = *(reinterpret_cast<ed::PipelineItem**>(payload->Data));

				if (data->Owner->PipelineItem_CanHaveChild(data->Type, data->PluginData, (plugin::PipelineItemType)dropItem->Type)) {

					bool duplicate = ImGui::GetIO().KeyCtrl;

					// first find a name that is not used
					std::string name = std::string(dropItem->Name);

					if (duplicate) { // change
						// remove numbers at the end of the string
						size_t lastOfLetter = std::string::npos;
						for (size_t j = name.size() - 1; j > 0; j--)
							if (!std::isdigit(name[j])) {
								lastOfLetter = j + 1;
								break;
							}
						if (lastOfLetter != std::string::npos)
							name = name.substr(0, lastOfLetter);

						// add number to the string and check if it already exists
						for (size_t j = 2; /*WE WILL BREAK FROM INSIDE ONCE WE FIND THE NAME*/; j++) {
							std::string newName = name + std::to_string(j);
							bool has = m_data->Pipeline.Has(newName.c_str());

							if (!has) {
								name = newName;
								break;
							}
						}
					}

					std::vector<InputLayoutItem> inpLayout = m_data->Plugins.BuildInputLayout(data->Owner, data->Type, data->PluginData);

					// get item owner
					void* itemData = nullptr;

					// once we find a name, duplicate the properties:
					// duplicate geometry object:
					if (dropItem->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem* newData = new pipe::GeometryItem();
						pipe::GeometryItem* origData = (pipe::GeometryItem*)dropItem->Data;

						newData->Position = origData->Position;
						newData->Rotation = origData->Rotation;
						newData->Scale = origData->Scale;
						newData->Size = origData->Size;
						newData->Topology = origData->Topology;
						newData->Type = origData->Type;

						if (newData->Type == pipe::GeometryItem::GeometryType::Cube)
							newData->VAO = eng::GeometryFactory::CreateCube(newData->VBO, newData->Size.x, newData->Size.y, newData->Size.z, inpLayout);
						else if (newData->Type == pipe::GeometryItem::Circle) {
							newData->VAO = eng::GeometryFactory::CreateCircle(newData->VBO, newData->Size.x, newData->Size.y, inpLayout);
							newData->Topology = GL_TRIANGLE_STRIP;
						} else if (newData->Type == pipe::GeometryItem::Plane)
							newData->VAO = eng::GeometryFactory::CreatePlane(newData->VBO, newData->Size.x, newData->Size.y, inpLayout);
						else if (newData->Type == pipe::GeometryItem::Rectangle)
							newData->VAO = eng::GeometryFactory::CreatePlane(newData->VBO, 1, 1, inpLayout);
						else if (newData->Type == pipe::GeometryItem::Sphere)
							newData->VAO = eng::GeometryFactory::CreateSphere(newData->VBO, newData->Size.x, inpLayout);
						else if (newData->Type == pipe::GeometryItem::Triangle)
							newData->VAO = eng::GeometryFactory::CreateTriangle(newData->VBO, newData->Size.x, inpLayout);
						else if (newData->Type == pipe::GeometryItem::ScreenQuadNDC)
							newData->VAO = eng::GeometryFactory::CreateScreenQuadNDC(newData->VBO, inpLayout);

						itemData = newData;
					}

					// duplicate Model:
					else if (dropItem->Type == PipelineItem::ItemType::Model) {
						pipe::Model* newData = new pipe::Model();
						pipe::Model* origData = (pipe::Model*)dropItem->Data;

						strcpy(newData->Filename, origData->Filename);
						strcpy(newData->GroupName, origData->GroupName);
						newData->OnlyGroup = origData->OnlyGroup;
						newData->Scale = origData->Scale;
						newData->Position = origData->Position;
						newData->Rotation = origData->Rotation;

						if (strlen(newData->Filename) > 0) {
							std::string objMem = m_data->Parser.LoadProjectFile(newData->Filename);
							eng::Model* mdl = m_data->Parser.LoadModel(newData->Filename);

							bool loaded = mdl != nullptr;
							if (loaded)
								newData->Data = mdl;
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, item->Name, "Failed to create .obj model " + std::string(item->Name));
						}

						itemData = newData;
					}

					// duplicate VertexBuffer:
					else if (dropItem->Type == PipelineItem::ItemType::VertexBuffer) {
						pipe::VertexBuffer* newData = new pipe::VertexBuffer();
						pipe::VertexBuffer* origData = (pipe::VertexBuffer*)dropItem->Data;

						newData->Scale = origData->Scale;
						newData->Position = origData->Position;
						newData->Rotation = origData->Rotation;
						newData->Topology = origData->Topology;
						newData->Buffer = origData->Buffer;

						if (newData->Buffer != 0)
							gl::CreateBufferVAO(newData->VAO, ((ed::BufferObject*)newData->Buffer)->ID, m_data->Objects.ParseBufferFormat(((ed::BufferObject*)newData->Buffer)->ViewFormat));

						itemData = newData;
					}

					// duplicate RenderState:
					else if (dropItem->Type == PipelineItem::ItemType::RenderState) {
						pipe::RenderState* newData = new pipe::RenderState();
						pipe::RenderState* origData = (pipe::RenderState*)dropItem->Data;
						memcpy(newData, origData, sizeof(pipe::RenderState));

						itemData = newData;
					}

					// duplicate PluginItem:
					else if (dropItem->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* newData = new pipe::PluginItemData();
						pipe::PluginItemData* origData = (pipe::PluginItemData*)dropItem->Data;

						strcpy(newData->Type, origData->Type);
						newData->Items = origData->Items; // even tho this should be empty
						newData->Owner = origData->Owner;
						newData->PluginData = origData->PluginData;

						newData->PluginData = origData->Owner->PipelineItem_CopyData(origData->Type, origData->PluginData);

						itemData = newData;
					}

					if (!duplicate) {
						PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
						if (props->HasItemSelected() && props->CurrentItemName() == dropItem->Name)
							props->Close();

						PreviewUI* prev = ((PreviewUI*)m_ui->Get(ViewID::Preview));
						if (prev->IsPicked(dropItem))
							prev->Pick(nullptr);

						m_data->Pipeline.Remove(dropItem->Name);
					}

					m_data->Pipeline.AddItem(item->Name, name.c_str(), dropItem->Type, itemData);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Unindent(PIPELINE_SHADER_PASS_INDENT);
	}
	void PipelineUI::m_addItem(ed::PipelineItem* item)
	{
		pipe::PluginItemData* pluginData = (pipe::PluginItemData*)item->Data;
		bool isPluginItem = item->Type == PipelineItem::ItemType::PluginItem;
		bool hasPluginProperties = isPluginItem && pluginData->Owner->PipelineItem_HasProperties(pluginData->Type, pluginData->PluginData);
		bool isPluginPickable = isPluginItem && pluginData->Owner->PipelineItem_IsPickable(pluginData->Type, pluginData->PluginData);

		ImGui::Indent(PIPELINE_ITEM_INDENT);
		if (ImGui::Selectable(item->Name, false, ImGuiSelectableFlags_AllowDoubleClick))
			if (ImGui::IsMouseDoubleClicked(0)) {
				if (Settings::Instance().General.ItemPropsOnDblCLk && (hasPluginProperties || !isPluginItem)) {
					PropertyUI* props = reinterpret_cast<PropertyUI*>(m_ui->Get(ViewID::Properties));
					props->Open(item);
				}

				if (Settings::Instance().General.SelectItemOnDblClk) {
					if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || isPluginPickable) {
						bool proceed = true;
						if (item->Type == PipelineItem::ItemType::Geometry)
							proceed = ((pipe::GeometryItem*)item->Data)->Type != pipe::GeometryItem::GeometryType::Rectangle && ((pipe::GeometryItem*)item->Data)->Type != pipe::GeometryItem::GeometryType::ScreenQuadNDC;

						if (proceed) {
							PreviewUI* props = reinterpret_cast<PreviewUI*>(m_ui->Get(ViewID::Preview));
							props->Pick(item);
							m_data->Renderer.AddPickedItem(item);
						}
					}
				}
			}
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
			ImGui::SetDragDropPayload("PipelineItemPayload", &item, sizeof(ed::PipelineItem**));

			ImGui::Text("%s", item->Name);

			ImGui::EndDragDropSource();
		}
		ImGui::Unindent(PIPELINE_ITEM_INDENT);
	}
	void PipelineUI::m_handleObjectDrop(ed::PipelineItem* pass, ed::ObjectManagerItem* object)
	{
		Logger::Get().Log("Handling an object drop on pipeline item");

		bool isPluginOwner = object->Plugin != nullptr;
		PluginObject* pobj = object->Plugin;

		bool bindAsUAV = false;
		bool bindAsSRV = false;

		if (object->Image != nullptr || object->Image3D != nullptr) {
			// bind image and image3D as UAV to compute passes
			if (pass->Type == PipelineItem::ItemType::ComputePass)
				bindAsUAV = true;
			// but as SRV to shader passes
			else if (pass->Type == PipelineItem::ItemType::ShaderPass)
				bindAsSRV = true;
		} else {
			bool isPluginObjBindable = isPluginOwner && (pobj->Owner->Object_IsBindable(pobj->Type) || pobj->Owner->Object_IsBindableUAV(pobj->Type));
			if (isPluginObjBindable || !isPluginOwner) {
				bool isPluginObjUAV = isPluginOwner && pobj->Owner->Object_IsBindableUAV(pobj->Type);

				if (object->Buffer != nullptr || isPluginObjUAV)
					bindAsUAV = true;
				else
					bindAsSRV = true;
			}
		}

		if (bindAsUAV) {
			int boundID = m_data->Objects.IsUniformBound(object, pass);
			if (boundID == -1)
				m_data->Objects.BindUniform(object, pass);
		} else if (bindAsSRV) {
			int boundID = m_data->Objects.IsBound(object, pass);
			if (boundID == -1)
				m_data->Objects.Bind(object, pass);
		}
	}
}