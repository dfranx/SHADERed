#include "CreateItemUI.h"
#include "UIHelper.h"
#include "../Objects/Names.h"
#include "../ImGUI/imgui.h"
#include <MoonLight/Base/GeometryFactory.h>

namespace ed
{
	void CreateItemUI::OnEvent(const ml::Event & e)
	{
	}
	void CreateItemUI::Update(float delta)
	{
		ImGui::Columns(2, 0, false);
		ImGui::SetColumnWidth(0, m_item.Type == PipelineItem::ItemType::BlendState ? 200 : 150);

		ImGui::Text("Name:");
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::InputText("##cui_name", m_item.Name, PIPELINE_ITEM_NAME_LENGTH);
		ImGui::NextColumn();

		if (m_item.Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* data = (pipe::GeometryItem*)m_item.Data;

			// geo type
			ImGui::Text("Geometry type:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Combo("##cui_geotype", reinterpret_cast<int*>(&data->Type), GEOMETRY_NAMES, ARRAYSIZE(GEOMETRY_NAMES));
			ImGui::NextColumn();

			//size
			ImGui::Text("Size:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateInputFloat3("##cui_geosize", data->Size);
			ImGui::NextColumn();
		}
		else if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = (pipe::ShaderPass*)m_item.Data;

			// vs path
			ImGui::Text("Vertex shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-40);
			ImGui::InputText("##cui_spvspath", data->VSPath, 512);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_spvspath", ImVec2(-1, 0))) {
				std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle()));
				if (file.size() != 0)
					strcpy(data->VSPath, file.c_str());
			}
			ImGui::NextColumn();

			// vs entry
			ImGui::Text("Vertex shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_spvsentry", data->VSEntry, 32);
			ImGui::NextColumn();

			// ps path
			ImGui::Text("Pixel shader path:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-40);
			ImGui::InputText("##cui_sppspath", data->PSPath, 512);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##cui_sppspath", ImVec2(-1, 0))) {
				std::string file = m_data->Parser.GetRelativePath(UIHelper::GetOpenFileDialog(m_data->GetOwner()->GetWindowHandle()));
				if (file.size() != 0)
					strcpy(data->PSPath, file.c_str());
			}
			ImGui::NextColumn();

			// vs entry
			ImGui::Text("Pixel shader entry:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##cui_sppsentry", data->PSEntry, 32);
			ImGui::NextColumn();
		}
		else if (m_item.Type == PipelineItem::ItemType::BlendState) {
			pipe::BlendState* data = (pipe::BlendState*)m_item.Data;
			D3D11_BLEND_DESC* desc = &data->State.Info;

			// alpha to coverage
			ImGui::Text("Alpha to coverage:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Checkbox("##cui_alphacov", (bool*)(&desc->AlphaToCoverageEnable));
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// source blend factor
			ImGui::Text("Source blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::DisplayBlendCombo("##cui_srcblend", desc->RenderTarget[0].SrcBlend);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// operator
			ImGui::Text("Blend operator:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::DisplayBlendOperatorCombo("##cui_blendop", desc->RenderTarget[0].BlendOp);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// destination blend factor
			ImGui::Text("Destination blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::DisplayBlendCombo("##cui_destblend", desc->RenderTarget[0].DestBlend);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// source alpha blend factor
			ImGui::Text("Source alpha blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::DisplayBlendCombo("##cui_srcalphablend", desc->RenderTarget[0].SrcBlendAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// operator
			ImGui::Text("Blend operator:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::DisplayBlendOperatorCombo("##cui_blendopalpha", desc->RenderTarget[0].BlendOpAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// destination alpha blend factor
			ImGui::Text("Destination blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::DisplayBlendCombo("##cui_destalphablend", desc->RenderTarget[0].DestBlendAlpha);
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			// blend factor
			ml::Color blendFactor = data->State.GetBlendFactor();
			ImGui::Text("Custom blend factor:");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			UIHelper::CreateInputColor("##cui_blendfactor", blendFactor);
			ImGui::PopItemWidth();
			ImGui::NextColumn();
			data->State.SetBlendFactor(blendFactor);
		}


		ImGui::Columns();
	}
	void CreateItemUI::SetOwner(const char * shaderPass)
	{
		if (shaderPass == nullptr)
			m_owner[0] = 0;
		else
			strcpy(m_owner, shaderPass);
	}
	void CreateItemUI::SetType(PipelineItem::ItemType type)
	{
		m_item.Type = type;

		if (m_item.Data != nullptr)
			delete m_item.Data;

		if (m_item.Type == PipelineItem::ItemType::Geometry) {
			auto allocatedData = new pipe::GeometryItem();
			allocatedData->Type = pipe::GeometryItem::GeometryType::Cube;
			allocatedData->Size = DirectX::XMFLOAT3(1,1,1);
			m_item.Data = allocatedData;
		}
		else if (m_item.Type == PipelineItem::ItemType::ShaderPass)
			m_item.Data = new pipe::ShaderPass();
		else if (m_item.Type == PipelineItem::ItemType::BlendState) {
			auto allocatedData = new pipe::BlendState();
			allocatedData->State.Info.IndependentBlendEnable = false;
			allocatedData->State.Info.RenderTarget[0].BlendEnable = true;
			m_item.Data = allocatedData;
		}
	}
	bool CreateItemUI::Create()
	{
		if (strlen(m_item.Name) < 2)
			return false;

		if (m_item.Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = new pipe::ShaderPass();
			pipe::ShaderPass* origData = (pipe::ShaderPass*)m_item.Data;

			strcpy(data->PSEntry, origData->PSEntry);
			strcpy(data->PSPath, origData->PSPath);
			strcpy(data->VSEntry, origData->VSEntry);
			strcpy(data->VSPath, origData->VSPath);

			return m_data->Pipeline.AddPass(m_item.Name, data);
		} else if (m_owner[0] != 0) {
			if (m_item.Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)m_item.Data;

				data->Type = origData->Type;
				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->Geometry = ml::GeometryFactory::CreateCube(origData->Size.x, origData->Size.y, origData->Size.z, *m_data->GetOwner());
				data->Position = DirectX::XMFLOAT3(0, 0, 0);
				data->Rotation = DirectX::XMFLOAT3(0, 0, 0);
				data->Scale = DirectX::XMFLOAT3(1, 1, 1);
				data->Size = origData->Size;
				data->Topology = ml::Topology::TriangleList;

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
			else if (m_item.Type == PipelineItem::ItemType::BlendState) {
				pipe::BlendState* data = new pipe::BlendState();
				pipe::BlendState* origData = (pipe::BlendState*)m_item.Data;

				data->State.Info = origData->State.Info;
				data->State.SetBlendFactor(origData->State.GetBlendFactor());
				data->State.Create(*m_data->GetOwner());

				return m_data->Pipeline.AddItem(m_owner, m_item.Name, m_item.Type, data);
			}
		}

		return false;
	}
}