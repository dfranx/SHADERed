#pragma once
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderLanguage.h>
#include <SHADERed/Objects/ShaderStage.h>
#include <SHADERed/UI/Tools/StatsPage.h>
#include <SHADERed/UI/UIView.h>
#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <deque>
#include <future>
#include <shared_mutex>

namespace ed {
	class CodeEditorUI : public UIView {
	public:
		CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = false);
		~CodeEditorUI();

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void StopThreads();
		void StopDebugging();
		void CloseAll(PipelineItem* item = nullptr);
		void SaveAll();

		void Open(PipelineItem* item, ed::ShaderStage stage);
		void OpenPluginCode(PipelineItem* item, const char* filepath, int id);
		TextEditor* Get(PipelineItem* item, ed::ShaderStage stage);

		void SetTheme(const TextEditor::Palette& colors);
		void SetFont(const std::string& filename, int size = 15);
		void SetAutoRecompile(bool autorecompile);
		void SetTrackFileChanges(bool track);
		void ApplySettings();

		inline bool HasFocus() { return m_selectedItem != -1; }
		inline bool NeedsFontUpdate() const { return m_fontNeedsUpdate; }
		inline std::pair<std::string, int> GetFont() { return std::make_pair(m_fontFilename, m_fontSize); }
		inline void UpdateFont()
		{
			m_fontNeedsUpdate = false;
			if (ImGui::GetIO().Fonts->Fonts.size() > 1)
				m_font = ImGui::GetIO().Fonts->Fonts[1];
			else
				m_font = ImGui::GetIO().Fonts->Fonts[0];
		}

		void UpdateAutoRecompileItems();

		inline bool TrackedFilesNeedUpdate() { return m_trackUpdatesNeeded > 0; }
		inline void EmptyTrackedFiles() { m_trackUpdatesNeeded = 0; }
		inline std::vector<bool> TrackedNeedsUpdate() { return m_trackedNeedsUpdate; }

		void SetOpenedFilesData(const std::vector<std::string>& data);
		std::vector<std::string> GetOpenedFilesData();
		std::vector<std::pair<std::string, ShaderStage>> GetOpenedFiles();

	private:
		void m_setupShortcuts();
		void m_loadEditorShortcuts(TextEditor* editor);

		TextEditor::LanguageDefinition m_buildLanguageDefinition(IPlugin* plugin, ShaderStage stage, const char* itemType, const char* filePath);
		void m_applyBreakpoints(TextEditor* editor, const std::string& path);

		// code editor font
		ImFont* m_font;

		// menu bar item actions
		void m_save(int id);
		void m_compile(int id);

		std::vector<PipelineItem*> m_items;
		std::vector<TextEditor*> m_editor;
		std::vector<StatsPage> m_stats;
		std::vector<std::string> m_paths;
		std::vector<ShaderStage> m_shaderStage;
		std::deque<bool> m_editorOpen;

		bool m_fontNeedsUpdate;
		std::string m_fontFilename;
		int m_fontSize;

		bool m_focusWindow;
		ShaderStage m_focusStage;
		std::string m_focusItem;

		int m_savePopupOpen;
		int m_selectedItem;

		// auto recompile
		void m_autoRecompiler();
		bool m_autoRecompile;
		std::thread* m_autoRecompileThread;
		std::atomic<bool> m_autoRecompilerRunning, m_autoRecompileRequest;
		std::vector<ed::MessageStack::Message> m_autoRecompileCachedMsgs;
		std::shared_mutex m_autoRecompilerMutex;
		struct AutoRecompilerItemInfo {
			AutoRecompilerItemInfo()
			{
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
		void m_trackWorker();
		std::vector<bool> m_trackedNeedsUpdate;
		bool m_trackFileChanges;
		std::atomic<bool> m_trackerRunning;
		std::atomic<int> m_trackUpdatesNeeded;
		std::vector<std::string> m_trackIgnore;
		std::thread* m_trackThread;
		std::mutex m_trackFilesMutex;
	};
}