#pragma once
#include "UIView.h"
#include "../ImGUI/TextEditor.h"
#include "../Objects/PipelineItem.h"
#include <d3d11.h>
#include <deque>

namespace ed
{
	class CodeEditorUI : public UIView
	{
	public:
		CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = false) : UIView(ui, objects, name, visible), m_selectedItem(-1) {
			m_consolas = ImGui::GetIO().Fonts->AddFontFromFileTTF("consola.ttf", 15.0f);
		}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		void OpenVS(PipelineItem item);
		void OpenPS(PipelineItem item);
		void OpenGS(PipelineItem item);

		inline void SetTheme(const TextEditor::Palette& colors) {
			for (TextEditor& editor : m_editor)
				editor.SetPalette(colors);
		}
		inline void SetTabSize(int ts) {
			for (TextEditor& editor : m_editor)
				editor.SetTabSize(ts);
		}
		inline void SetInsertSpaces(bool ts) {
			for (TextEditor& editor : m_editor)
				editor.SetInsertSpaces(ts);
		}
		inline void SetSmartIndent(bool ts) {
			for (TextEditor& editor : m_editor)
				editor.SetSmartIndent(ts);
		}
		inline void SetHighlightLine(bool ts) {
			for (TextEditor& editor : m_editor)
				editor.SetHighlightLine(ts);
		}
		inline void SetShowLineNumbers(bool ts) {
			for (TextEditor& editor : m_editor)
				editor.SetShowLineNumbers(ts);
		}

		void CloseAll();

		std::vector<std::pair<std::string, int>> GetOpenedFiles();

	private:
		class StatsPage
		{
		public:
			StatsPage() : IsActive(false), Info(nullptr) {}
			~StatsPage()
			{
				
			}
			bool IsActive;

			void* Info;
		};

	private:
		void m_open(PipelineItem item, int shaderTypeID);

		// font for code editor
		ImFont *m_consolas;

		// menu bar item actions
		void m_save(int id);
		void m_compile(int id);
		void m_fetchStats(int id);
		void m_renderStats(int id);

		std::vector<PipelineItem> m_items;
		std::vector<TextEditor> m_editor;
		std::vector<StatsPage> m_stats;
		std::vector<int> m_shaderTypeId;
		std::deque<bool> m_editorOpen;

		int m_selectedItem;
	};
}