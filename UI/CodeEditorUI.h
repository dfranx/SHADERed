#pragma once
#include "UIView.h"
#include <ImGuiColorTextEdit/TextEditor.h>
#include "../Objects/ShaderLanguage.h"
#include "../Objects/PipelineItem.h"
#include "../Objects/Settings.h"
#include "../Objects/Logger.h"
#include <imgui/examples/imgui_impl_sdl.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <deque>
#include <future>
#include <shared_mutex>
#include <ghc/filesystem.hpp>

namespace ed
{
	class CodeEditorUI : public UIView
	{
	public:
		CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = false) : UIView(ui, objects, name, visible), m_selectedItem(-1) {
			Settings& sets = Settings::Instance(); // TODO: do this more often

			if (ghc::filesystem::exists(sets.Editor.Font))
				m_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(sets.Editor.Font, sets.Editor.FontSize);
			else {
				m_font = ImGui::GetIO().Fonts->AddFontDefault();
				Logger::Get().Log("Failed to load editor font", true);
			}

			m_fontFilename = sets.Editor.Font;
			m_fontSize = sets.Editor.FontSize;
			m_fontNeedsUpdate = false;
			m_savePopupOpen = -1;
			m_focusSID = 0;
			m_focusWindow = false;
			m_trackFileChanges = false;
			m_trackThread = nullptr;
			m_autoRecompileThread = nullptr;
			m_autoRecompilerRunning = false;
			m_autoRecompile = false;

			m_setupShortcuts();
		}
		~CodeEditorUI();

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void SaveAll();

		void OpenVS(PipelineItem* item);
		void OpenPS(PipelineItem* item);
		void OpenGS(PipelineItem* item);
		void OpenCS(PipelineItem* item);

		void OpenPluginCode(PipelineItem* item, const char* filepath, int id);

		inline bool HasFocus() { return m_selectedItem != -1; }

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
		inline void SetShowWhitespace(bool wh) {
			for (TextEditor& editor : m_editor)
				editor.SetShowWhitespaces(wh);
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
		
		// TODO: remove unused functions here
		inline bool NeedsFontUpdate() const { return m_fontNeedsUpdate; }
		inline std::pair<std::string, int> GetFont() { return std::make_pair(m_fontFilename, m_fontSize); }
		inline void UpdateFont() { m_fontNeedsUpdate = false; m_font = ImGui::GetIO().Fonts->Fonts[1]; }
		inline void UpdateShortcuts() {
			for (auto& editor : m_editor)
				m_loadEditorShortcuts(&editor);
		}

		void SetAutoRecompile(bool autorecompile);
		void UpdateAutoRecompileItems();


		void SetTrackFileChanges(bool track);
		inline bool TrackedFilesNeedUpdate() { return m_trackUpdatesNeeded > 0; }
		inline void EmptyTrackedFiles() { m_trackUpdatesNeeded = 0; }
		inline std::vector<bool> TrackedNeedsUpdate() { return m_trackedNeedsUpdate; }

		void CloseAll();
		void CloseAllFrom(PipelineItem* item);

		std::vector<std::pair<std::string, int>> GetOpenedFiles();
		std::vector<std::string> GetOpenedFilesData();
		void SetOpenedFilesData(const std::vector<std::string>& data);


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
		void m_open(PipelineItem* item, int shaderTypeID); // TODO: add pointer to the pipelineitem
		void m_setupShortcuts();

		TextEditor::LanguageDefinition m_buildLanguageDefinition(IPlugin* plugin, int sid, const char* itemType, const char* filePath);

		void m_loadEditorShortcuts(TextEditor* editor);

		// font for code editor
		ImFont *m_font;

		// menu bar item actions
		void m_save(int id);
		void m_compile(int id);
		//void m_fetchStats(int id);
		//void m_renderStats(int id);

		std::vector<PipelineItem*> m_items;
		std::vector<TextEditor> m_editor;
		std::vector<StatsPage> m_stats;
		std::vector<std::string> m_paths;
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

		// auto recompile
		std::thread* m_autoRecompileThread;
		void m_autoRecompiler();
		std::atomic<bool> m_autoRecompilerRunning, m_autoRecompileRequest;
		std::vector<ed::MessageStack::Message> m_autoRecompileCachedMsgs;
		bool m_autoRecompile;
		std::shared_mutex m_autoRecompilerMutex;
		struct AutoRecompilerItemInfo
		{
			AutoRecompilerItemInfo() {
				VS = PS = GS = CS = "";
				VS_SLang = PS_SLang = GS_SLang = CS_SLang = ShaderLanguage::GLSL;

				SPass = nullptr;
				CPass = nullptr;
			}
			std::string VS, PS, GS;
			ShaderLanguage VS_SLang, PS_SLang, GS_SLang;
			pipe::ShaderPass* SPass;

			std::string CS;
			ShaderLanguage CS_SLang;
			pipe::ComputePass* CPass;

			std::string AS;
			pipe::AudioPass* APass;

			std::string PluginCode;
			int PluginID;
			pipe::PluginItemData* PluginData;
		};
		std::unordered_map<std::string, AutoRecompilerItemInfo> m_ariiList;

		// all the variables needed for the file change notifications
		std::vector<bool> m_trackedNeedsUpdate;

		bool m_trackFileChanges;
		std::atomic<bool> m_trackerRunning;
		std::atomic<int> m_trackUpdatesNeeded;
		std::vector<std::string> m_trackIgnore;
		std::thread* m_trackThread;
		std::mutex m_trackFilesMutex;
		void m_trackWorker();
	};
}