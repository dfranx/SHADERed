#pragma once
#include "UIView.h"
#include "../ImGUI/TextEditor.h"
#include "../Objects/PipelineItem.h"
#include "../Objects/Settings.h"
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>
#include <d3d11.h>
#include <deque>

namespace ed
{
	class CodeEditorUI : public UIView
	{
	public:
		CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = false) : UIView(ui, objects, name, visible), m_selectedItem(-1) {
			m_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(Settings::Instance().Editor.Font, Settings::Instance().Editor.FontSize);
			m_fontFilename = Settings::Instance().Editor.Font;
			m_fontSize = Settings::Instance().Editor.FontSize;
			m_fontNeedsUpdate = false;
			m_savePopupOpen = -1;
			m_focusSID = 0;
			m_focusWindow = false;

			m_setupShortcuts();
		}

		virtual void OnEvent(const ml::Event& e);
		virtual void Update(float delta);

		void SaveAll();

		void OpenVS(PipelineItem item);
		void OpenPS(PipelineItem item);
		void OpenGS(PipelineItem item);

		void RenameShaderPass(const std::string& name, const std::string& newName);

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
		inline void SetCompleteBraces(bool ts) {
			for (TextEditor& editor : m_editor)
				editor.SetCompleteBraces(ts);
		}
		inline void SetHorizontalScrollbar(bool ts) {
			for (TextEditor& editor : m_editor)
				editor.SetHorizontalScroll(ts);
		}
		inline void SetSmartPredictions(bool ts) {
			for (TextEditor& editor : m_editor)
				editor.SetSmartPredictions(ts);
		}
		inline void SetFont(const std::string& filename, int size = 15)
		{
			m_fontNeedsUpdate = m_fontFilename != filename || m_fontSize != size;
			m_fontFilename = filename;
			m_fontSize = size;
		}
		inline void UpdateFont() {
			if (m_fontNeedsUpdate) {
				ImGui::GetIO().Fonts->Clear();
				ImGui::GetIO().Fonts->AddFontDefault();
				m_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(m_fontFilename.c_str(), m_fontSize);
				
				unsigned char* out = nullptr;
				int wid, heig;
				ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&out, &wid, &heig);

				m_fontNeedsUpdate = false;

			}
		}

		void CloseAll();

		std::vector<std::pair<std::string, int>> GetOpenedFiles();


	private:
		class StatsPage
		{
		public:
			StatsPage(InterfaceManager* im) : IsActive(false), Info(nullptr), m_data(im) {}
			~StatsPage() { }

			void Fetch(ed::PipelineItem* item, const std::string& code, int type);
			void Render();

			bool IsActive;
			void* Info;

		private:
			InterfaceManager* m_data;
		};


	private:
		void m_open(PipelineItem item, int shaderTypeID);
		void m_setupShortcuts();

		// font for code editor
		ImFont *m_font;

		// menu bar item actions
		void m_save(int id);
		void m_compile(int id);
		//void m_fetchStats(int id);
		//void m_renderStats(int id);

		std::vector<PipelineItem> m_items;
		std::vector<TextEditor> m_editor;
		std::vector<StatsPage> m_stats;
		std::vector<int> m_shaderTypeId;
		std::deque<bool> m_editorOpen;

		int m_savePopupOpen;
		bool m_fontNeedsUpdate;
		std::string m_fontFilename;
		int m_fontSize;

		bool m_focusWindow;
		int m_focusSID;
		std::string m_focusItem;

		int m_selectedItem;
	};
}