#pragma once
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/ShaderVariable.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class CreateItemUI : public UIView {
	public:
		CreateItemUI(GUIManager* ui, InterfaceManager* objects, const std::string& name = "", bool visible = false);

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void SetOwner(const char* shaderPass);
		void SetType(PipelineItem::ItemType type);
		bool Create();
		void Reset();

	private:
		void m_autoVariablePopulate(pipe::ShaderPass* pass);

		void m_createFile(const std::string& filen);

		std::vector<std::string> m_groups;
		int m_selectedGroup;

		bool m_errorOccured;

		char m_owner[PIPELINE_ITEM_NAME_LENGTH];
		PipelineItem m_item;

		char* m_dialogPath;
		bool* m_dialogShaderAuto;
		std::string m_dialogShaderType;

		bool m_isShaderFileAuto[3];
	};
}