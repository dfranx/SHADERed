#include "PropertyUI.h"
#include "UIHelper.h"
#include "../ImGUI/imgui.h"
#include "../Objects/Names.h"

#include <Shlwapi.h>

namespace ed
{
	void PropertyUI::OnEvent(const ml::Event & e)
	{}
	void PropertyUI::Update(float delta)
	{
		if (m_current != nullptr) {
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

			ImGui::Text(m_current->Name);
			ImGui::PopStyleColor();

			ImGui::Columns(2, "##content_columns");
			ImGui::SetColumnWidth(0, ImGui::GetWindowSize().x * 0.3f);

			ImGui::Separator();
			
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
				else memcpy(m_current->Name, m_itemName, PIPELINE_ITEM_NAME_LENGTH);
			}
			ImGui::NextColumn();

			ImGui::Separator();

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

			if (m_current->Type == ed::PipelineItem::ItemType::ShaderPass) {
				ed::pipe::ShaderPass* item = reinterpret_cast<ed::pipe::ShaderPass*>(m_current->Data);

				/* vertex shader path */
				ImGui::Text("VS Path:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-40);
				ImGui::InputText("##pui_vspath", item->VSPath, 512);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("...##pui_vsbtn", ImVec2(-1, 0))) {
					std::string file = UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle());
					if (file.size() != 0) {
						file = m_data->Parser.GetRelativePath(file);
						strcpy(item->VSPath, file.c_str());
					}
				}
				ImGui::NextColumn();

				ImGui::Separator();

				/* vertex shader entry */
				ImGui::Text("VS Entry:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				ImGui::InputText("##pui_vsentry", item->VSEntry, 32);
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				/* TODO: create function for "path" property items... */
				/* pixel shader path */
				ImGui::Text("PS Path:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-40);
				ImGui::InputText("##pui_pspath", item->PSPath, 512);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("...##pui_psbtn", ImVec2(-1, 0))) {
					std::string file = UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle());
					if (file.size() != 0) {
						file = m_data->Parser.GetRelativePath(file);
						strcpy(item->PSPath, file.c_str());
					}
				}
				ImGui::NextColumn();

				ImGui::Separator();

				/* pixel shader entry */
				ImGui::Text("PS Entry:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				ImGui::InputText("##pui_psentry", item->PSEntry, 32);
				ImGui::PopItemWidth();
			}
			else if (m_current->Type == ed::PipelineItem::ItemType::Geometry) {
				ed::pipe::GeometryItem* item = reinterpret_cast<ed::pipe::GeometryItem*>(m_current->Data);

				/* position */
				ImGui::Text("Position:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				UIHelper::CreateInputFloat3("##pui_geopos", item->Position);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* scale */
				ImGui::Text("Scale:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				UIHelper::CreateInputFloat3("##pui_geoscale", item->Scale);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* scale */
				ImGui::Text("Rotation:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				UIHelper::CreateInputFloat3("##pui_georota", item->Rotation);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* topology type */
				ImGui::Text("Topology:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				ImGui::Combo("##pui_geotopology", reinterpret_cast<int*>(&item->Topology), TOPOLOGY_ITEM_NAMES, _ARRAYSIZE(TOPOLOGY_ITEM_NAMES));
				ImGui::PopItemWidth();
			}
			else if (m_current->Type == PipelineItem::ItemType::BlendState) {
				pipe::BlendState* data = (pipe::BlendState*)m_current->Data;
				D3D11_BLEND_DESC* desc = &data->State.Info;
				bool changed = false;

				// alpha to coverage
				ImGui::Text("Alpha to coverage:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##cui_alphacov", (bool*)(&desc->AlphaToCoverageEnable))) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// source blend factor
				ImGui::Text("Source blend factor:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateBlendCombo("##cui_srcblend", desc->RenderTarget[0].SrcBlend)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// operator
				ImGui::Text("Blend operator:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateBlendOperatorCombo("##cui_blendop", desc->RenderTarget[0].BlendOp)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// destination blend factor
				ImGui::Text("Destination blend factor:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateBlendCombo("##cui_destblend", desc->RenderTarget[0].DestBlend)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// source alpha blend factor
				ImGui::Text("Source alpha blend factor:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateBlendCombo("##cui_srcalphablend", desc->RenderTarget[0].SrcBlendAlpha)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// operator
				ImGui::Text("Blend operator:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateBlendOperatorCombo("##cui_blendopalpha", desc->RenderTarget[0].BlendOpAlpha)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// destination alpha blend factor
				ImGui::Text("Destination blend factor:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateBlendCombo("##cui_destalphablend", desc->RenderTarget[0].DestBlendAlpha)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// blend factor
				ml::Color blendFactor = data->State.GetBlendFactor();
				ImGui::Text("Custom blend factor:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				UIHelper::CreateInputColor("##cui_blendfactor", blendFactor);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				data->State.SetBlendFactor(blendFactor);

				if (changed)
					data->State.Create(*m_data->GetOwner());
			}
			else if (m_current->Type == PipelineItem::ItemType::DepthStencilState) {
				pipe::DepthStencilState* data = (pipe::DepthStencilState*)m_current->Data;
				D3D11_DEPTH_STENCIL_DESC* desc = &data->State.Info;
				bool changed = false;

				// depth enable
				ImGui::Text("Depth test:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##cui_depth", (bool*)(&desc->DepthEnable))) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// depth function
				ImGui::Text("Depth function:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateComparisonFunctionCombo("##cui_depthop", desc->DepthFunc)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// stencil enable
				ImGui::Text("Stencil test:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##cui_stencil", (bool*)(&desc->StencilEnable))) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// front face function
				ImGui::Text("Stencil front face function:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateComparisonFunctionCombo("##cui_ffunc", desc->FrontFace.StencilFunc)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// front face pass operation
				ImGui::Text("Stencil front face pass:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateStencilOperationCombo("##cui_fpass", desc->FrontFace.StencilPassOp)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// front face fail operation
				ImGui::Text("Stencil front face fail:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateStencilOperationCombo("##cui_ffail", desc->FrontFace.StencilFailOp)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// back face function
				ImGui::Text("Stencil back face function:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateComparisonFunctionCombo("##cui_bfunc", desc->BackFace.StencilFunc)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// back face pass operation
				ImGui::Text("Stencil back face pass:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateStencilOperationCombo("##cui_bpass", desc->BackFace.StencilPassOp)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// back face fail operation
				ImGui::Text("Stencil back face fail:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateStencilOperationCombo("##cui_bfail", desc->BackFace.StencilFailOp)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// stencil reference
				ImGui::Text("Stencil reference:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				ImGui::InputInt("##cui_sref", (int*)&data->StencilReference); // imgui uint input??
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				if (changed)
					data->State.Create(*m_data->GetOwner());
			}
			else if (m_current->Type == PipelineItem::ItemType::RasterizerState) {
				pipe::RasterizerState* data = (pipe::RasterizerState*)m_current->Data;
				D3D11_RASTERIZER_DESC* desc = &data->State.Info;
				bool changed = false;

				// enable/disable wireframe rendering
				bool isWireframe = desc->FillMode == D3D11_FILL_WIREFRAME;
				ImGui::Text("Wireframe:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##cui_wireframe", (bool*)(&isWireframe))) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				desc->FillMode = isWireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;

				ImGui::Separator();

				// cull mode
				ImGui::Text("Cull mode:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (UIHelper::CreateCullModeCombo("##cui_cull", desc->CullMode)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// front face == counter clockwise order
				ImGui::Text("Counter clockwise:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##cui_ccw", (bool*)(&desc->FrontCounterClockwise))) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// depth bias
				ImGui::Text("Depth bias:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt("##cui_depthbias", &desc->DepthBias)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// depth bias clamp
				ImGui::Text("Depth bias clamp:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##cui_depthbiasclamp", &desc->DepthBiasClamp)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// slope scaled depth bias
				ImGui::Text("Slope scaled depth bias:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##cui_slopebias", &desc->SlopeScaledDepthBias)) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// depth clip
				ImGui::Text("Depth clip:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##cui_depthclip", (bool*)(&desc->DepthClipEnable))) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				ImGui::Separator();

				// antialiasing
				ImGui::Text("Antialiasing:");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::Checkbox("##cui_aa", (bool*)(&desc->AntialiasedLineEnable))) changed = true;
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				if (changed)
					data->State.Create(*m_data->GetOwner());
			}
			else if (m_current->Type == ed::PipelineItem::ItemType::OBJModel) {
				ed::pipe::OBJModel* item = reinterpret_cast<ed::pipe::OBJModel*>(m_current->Data);

				/* position */
				ImGui::Text("Position:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				UIHelper::CreateInputFloat3("##pui_geopos", item->Position);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* scale */
				ImGui::Text("Scale:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				UIHelper::CreateInputFloat3("##pui_geoscale", item->Scale);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* rotation */
				ImGui::Text("Rotation:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				UIHelper::CreateInputFloat3("##pui_georota", item->Rotation);
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();
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
		if (item != nullptr) memcpy(m_itemName, item->Name, PIPELINE_ITEM_NAME_LENGTH);

		m_current = item;
	}
}