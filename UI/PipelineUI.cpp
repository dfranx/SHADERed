#include "PipelineUI.h"
#include "PropertyUI.h"
#include "../GUIManager.h"
#include "../ImGUI/imgui.h"

#define SEMANTIC_LENGTH 16
static const char* FORMAT_NAMES[] = {
	"UNKNOWN\0",
	"R32G32B32A32_TYPELESS\0",
	"R32G32B32A32_FLOAT\0",
	"R32G32B32A32_UINT\0",
	"R32G32B32A32_SINT\0",
	"R32G32B32_TYPELESS\0",
	"R32G32B32_FLOAT\0",
	"R32G32B32_UINT\0",
	"R32G32B32_SINT\0",
	"R16G16B16A16_TYPELESS\0",
	"R16G16B16A16_FLOAT\0",
	"R16G16B16A16_UNORM\0",
	"R16G16B16A16_UINT\0",
	"R16G16B16A16_SNORM\0",
	"R16G16B16A16_SINT\0",
	"R32G32_TYPELESS\0",
	"R32G32_FLOAT\0",
	"R32G32_UINT\0",
	"R32G32_SINT\0",
	"R32G8X24_TYPELESS\0",
	"D32_FLOAT_S8X24_UINT\0",
	"R32_FLOAT_X8X24_TYPELESS\0",
	"X32_TYPELESS_G8X24_UINT\0",
	"R10G10B10A2_TYPELESS\0",
	"R10G10B10A2_UNORM\0",
	"R10G10B10A2_UINT\0",
	"R11G11B10_FLOAT\0",
	"R8G8B8A8_TYPELESS\0",
	"R8G8B8A8_UNORM\0",
	"R8G8B8A8_UNORM_SRGB\0",
	"R8G8B8A8_UINT\0",
	"R8G8B8A8_SNORM\0",
	"R8G8B8A8_SINT\0",
	"R16G16_TYPELESS\0",
	"R16G16_FLOAT\0",
	"R16G16_UNORM\0",
	"R16G16_UINT\0",
	"R16G16_SNORM\0",
	"R16G16_SINT\0",
	"R32_TYPELESS\0",
	"D32_FLOAT\0",
	"R32_FLOAT\0",
	"R32_UINT\0",
	"R32_SINT\0",
	"R24G8_TYPELESS\0",
	"D24_UNORM_S8_UINT\0",
	"R24_UNORM_X8_TYPELESS\0",
	"X24_TYPELESS_G8_UINT\0",
	"R8G8_TYPELESS\0",
	"R8G8_UNORM\0",
	"R8G8_UINT\0",
	"R8G8_SNORM\0",
	"R8G8_SINT\0",
	"R16_TYPELESS\0",
	"R16_FLOAT\0",
	"D16_UNORM\0",
	"R16_UNORM\0",
	"R16_UINT\0",
	"R16_SNORM\0",
	"R16_SINT\0",
	"R8_TYPELESS\0",
	"R8_UNORM\0",
	"R8_UINT\0",
	"R8_SNORM\0",
	"R8_SINT\0",
	"A8_UNORM\0",
	"R1_UNORM\0",
	"R9G9B9E5_SHAREDEXP\0",
	"R8G8_B8G8_UNORM\0",
	"G8R8_G8B8_UNORM\0",
	"BC1_TYPELESS\0",
	"BC1_UNORM\0",
	"BC1_UNORM_SRGB\0",
	"BC2_TYPELESS\0",
	"BC2_UNORM\0",
	"BC2_UNORM_SRGB\0",
	"BC3_TYPELESS\0",
	"BC3_UNORM\0",
	"BC3_UNORM_SRGB\0",
	"BC4_TYPELESS\0",
	"BC4_UNORM\0",
	"BC4_SNORM\0",
	"BC5_TYPELESS\0",
	"BC5_UNORM\0",
	"BC5_SNORM\0",
	"B5G6R5_UNORM\0",
	"B5G5R5A1_UNORM\0",
	"B8G8R8A8_UNORM\0",
	"B8G8R8X8_UNORM\0",
	"R10G10B10_XR_BIAS_A2_UNORM\0",
	"B8G8R8A8_TYPELESS\0",
	"B8G8R8A8_UNORM_SRGB\0",
	"B8G8R8X8_TYPELESS\0",
	"B8G8R8X8_UNORM_SRGB\0",
	"BC6H_TYPELESS\0",
	"BC6H_UF16\0",
	"BC6H_SF16\0",
	"BC7_TYPELESS\0",
	"BC7_UNORM\0",
	"BC7_UNORM_SRGB\0",
	"AYUV\0",
	"Y410\0",
	"Y416\0",
	"NV12\0",
	"P010\0",
	"P016\0",
	"420_OPAQUE\0",
	"YUY2\0",
	"Y210\0",
	"Y216\0",
	"NV11\0",
	"AI44\0",
	"IA44\0",
	"P8\0",
	"A8P8\0",
	"B4G4R4A4_UNORM\0",
	"P208\0",
	"V208\0",
	"V408\0"
};

namespace ed
{
	void PipelineUI::OnEvent(const ml::Event & e)
	{}
	void PipelineUI::Update(float delta)
	{
		std::vector<ed::PipelineManager::Item>& items = m_data->Pipeline.GetList();

		for (int i = 0; i < items.size(); i++) {
			m_renderItemUpDown(items, i);
			if (items[i].Type == ed::PipelineItem::ShaderFile)
				m_addShader(items[i].Name, (ed::pipe::ShaderItem*)items[i].Data);
			else
				m_addItem(items[i].Name);
			m_renderItemContext(items, i);
		}



		// Input Layout item manager
		if (m_isLayoutOpened) {
			ImGui::OpenPopup("Item Manager##pui_layout_items");
			m_isLayoutOpened = false;
		}

		ImGui::SetNextWindowSize(ImVec2(600, 175), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Item Manager##pui_layout_items")) {
			m_renderInputLayoutUI();

			if (ImGui::Button("Ok")) m_closePopup();

			ImGui::EndPopup();
		}
	}
	
	void PipelineUI::m_closePopup()
	{
		ImGui::CloseCurrentPopup();
		m_modalItem = nullptr;
	}
	
	void PipelineUI::m_renderItemUpDown(std::vector<ed::PipelineManager::Item>& items, int index)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::ArrowButton(std::string("##U" + std::string(items[index].Name)).c_str(), ImGuiDir_Up)) {
			if (index != 0) {
				ed::PipelineManager::Item temp = items[index - 1];
				items[index - 1] = items[index];
				items[index] = temp;
			}
		}
		ImGui::SameLine(0, 0);

		if (ImGui::ArrowButton(std::string("##D" + std::string(items[index].Name)).c_str(), ImGuiDir_Down)) {
			if (index != items.size() - 1) {
				ed::PipelineManager::Item temp = items[index + 1];
				items[index + 1] = items[index];
				items[index] = temp;
			}
		}
		ImGui::SameLine(0, 0);

		ImGui::PopStyleColor();
	}
	void PipelineUI::m_renderItemContext(std::vector<ed::PipelineManager::Item>& items, int index)
	{
		if (ImGui::BeginPopupContextItem(("##context_" + std::string(items[index].Name)).c_str())) {
			if (items[index].Type == ed::PipelineItem::InputLayout) {
				if (ImGui::Selectable("Items...")) {
					m_isLayoutOpened = true;
					m_modalItem = &items[index];
				}
			} else if (ImGui::Selectable("Properties"))
				(reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")))->Open(&items[index]);

			if (ImGui::Selectable("Delete")) {

				PropertyUI* props = (reinterpret_cast<PropertyUI*>(m_ui->Get("Properties")));
				if (props->HasItemSelected() && props->CurrentItemName() == items[index].Name)
					props->Open(nullptr);

				m_data->Pipeline.Remove(items[index].Name);
			}

			ImGui::EndPopup();
		}
	}
	void PipelineUI::m_renderInputLayoutUI()
	{
		static D3D11_INPUT_ELEMENT_DESC iElement = {
			(char*)calloc(SEMANTIC_LENGTH, sizeof(char)), 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
		};
		static bool scrollToBottom = false;

		ImGui::Text("Add or remove items from vertex input layout.");

		ImGui::BeginChild("##pui_layout_table", ImVec2(0, -25));
		ImGui::Columns(5);

		ImGui::Text("Semantic"); ImGui::NextColumn();
		ImGui::Text("Semantic ID"); ImGui::NextColumn();
		ImGui::Text("Format"); ImGui::NextColumn();
		ImGui::Text("Offset"); ImGui::NextColumn();
		ImGui::Text("Controls"); ImGui::NextColumn();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		int id = 0;
		std::vector<D3D11_INPUT_ELEMENT_DESC>& els = reinterpret_cast<ed::pipe::InputLayout*>(m_modalItem->Data)->Layout.GetInputElements();
		for (auto& el : els) {
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::InputText(("##semantic" + std::to_string(id)).c_str(), const_cast<char*>(el.SemanticName), 10);
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::InputInt(("##semanticID" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.SemanticIndex));
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::Combo(("##formatName" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.Format), FORMAT_NAMES, ARRAYSIZE(FORMAT_NAMES));
			ImGui::NextColumn();
			
			ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
				ImGui::InputInt(("##byteOffset" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&el.AlignedByteOffset));
			ImGui::NextColumn();


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				if (ImGui::Button(("U##" + std::to_string(id)).c_str()) && id != 0) {
					D3D11_INPUT_ELEMENT_DESC temp = els[id - 1];
					els[id - 1] = el;
					els[id] = temp;
				}
				ImGui::SameLine(); if (ImGui::Button(("D##" + std::to_string(id)).c_str()) && id != els.size() - 1) {
					D3D11_INPUT_ELEMENT_DESC temp = els[id + 1];
					els[id + 1] = el;
					els[id] = temp;
				}
				ImGui::SameLine(); if (ImGui::Button(("X##" + std::to_string(id)).c_str()))
					els.erase(els.begin() + id);
			ImGui::PopStyleColor();
			ImGui::NextColumn();

			id++;
		}

		ImGui::PopStyleColor();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputText("##inputSemantic", const_cast<char*>(iElement.SemanticName), SEMANTIC_LENGTH); 
		ImGui::NextColumn();

		if (scrollToBottom) {
			ImGui::SetScrollHere();
			scrollToBottom = false;
		}

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputInt(("##inputSemanticID" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&iElement.SemanticIndex));
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::Combo(("##inputFormatName" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&iElement.Format), FORMAT_NAMES, ARRAYSIZE(FORMAT_NAMES));
		ImGui::NextColumn();

		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			ImGui::InputInt(("##inputByteOffset" + std::to_string(id)).c_str(), reinterpret_cast<int*>(&iElement.AlignedByteOffset));
		ImGui::NextColumn();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-ImGui::GetStyle().FramePadding.x);
			if (ImGui::Button("ADD")) {
				els.push_back(iElement);

				iElement.SemanticName = (char*)calloc(SEMANTIC_LENGTH, sizeof(char));
				iElement.SemanticIndex = 0;
				
				scrollToBottom = true;
			}
		ImGui::NextColumn();
		ImGui::PopStyleColor();
		//ImGui::PopItemWidth();

		ImGui::EndChild();
		ImGui::Columns(1);
	}

	void PipelineUI::m_addShader(const std::string& name, ed::pipe::ShaderItem* data)
	{
		std::string type = "PS";
		if (data->Type == ed::pipe::ShaderItem::VertexShader)
			type = "VS";
		else if (data->Type == ed::pipe::ShaderItem::GeometryShader)
			type = "GS";

		ImGui::Text(type.c_str());
		ImGui::SameLine();

		ImGui::Indent(60);
		ImGui::Selectable(name.c_str());
		ImGui::Unindent(60);
	}
	void PipelineUI::m_addItem(const std::string & name)
	{
		ImGui::Indent(60);
		ImGui::Selectable(name.c_str());
		ImGui::Unindent(60);
	}
}