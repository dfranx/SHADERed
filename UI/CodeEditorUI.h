#pragma once
#include "../Objects/Logger.h"
#include "../Objects/PipelineItem.h"
#include "../Objects/Settings.h"
#include "../Objects/ShaderLanguage.h"
#include "../Objects/ShaderStage.h"
#include "UIView.h"
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

		void SaveAll();

		void Open(PipelineItem* item, ed::ShaderStage stage);
		TextEditor* Get(PipelineItem* item, ed::ShaderStage stage);

		void OpenPluginCode(PipelineItem* item, const char* filepath, int id);

		void SetTheme(const TextEditor::Palette& colors);
		void SetTabSize(int ts);
		void SetInsertSpaces(bool ts);
		void SetSmartIndent(bool ts);
		void SetShowWhitespace(bool wh);
		void SetHighlightLine(bool ts);
		void SetShowLineNumbers(bool ts);
		void SetCompleteBraces(bool ts);
		void SetHorizontalScrollbar(bool ts);
		void SetSmartPredictions(bool ts);
		void SetFunctionTooltips(bool ts);
		void UpdateShortcuts();
		void SetFont(const std::string& filename, int size = 15);

		void StopThreads();

		// TODO: remove unused functions here
		inline bool HasFocus() { return m_selectedItem != -1; }
		inline bool NeedsFontUpdate() const { return m_fontNeedsUpdate; }
		inline std::pair<std::string, int> GetFont() { return std::make_pair(m_fontFilename, m_fontSize); }
		inline void UpdateFont()
		{
			m_fontNeedsUpdate = false;
			m_font = ImGui::GetIO().Fonts->Fonts[1];
		}

		void SetAutoRecompile(bool autorecompile);
		void UpdateAutoRecompileItems();

		void StopDebugging();

		void SetTrackFileChanges(bool track);
		inline bool TrackedFilesNeedUpdate() { return m_trackUpdatesNeeded > 0; }
		inline void EmptyTrackedFiles() { m_trackUpdatesNeeded = 0; }
		inline std::vector<bool> TrackedNeedsUpdate() { return m_trackedNeedsUpdate; }

		void CloseAll();
		void CloseAllFrom(PipelineItem* item);

		std::vector<std::pair<std::string, ShaderStage>> GetOpenedFiles();
		std::vector<std::string> GetOpenedFilesData();
		void SetOpenedFilesData(const std::vector<std::string>& data);

	private:
		// TODO:
		class StatsPage {
		public:
			StatsPage(InterfaceManager* im)
					: IsActive(false)
					, Info(nullptr)
					, m_data(im)
			{
			}
			~StatsPage() { }

			void Fetch(ed::PipelineItem* item, const std::string& code, int type);
			void Render();

			bool IsActive;
			void* Info;

		private:
			InterfaceManager* m_data;
		};

	private:
		void m_setupShortcuts();

		TextEditor::LanguageDefinition m_buildLanguageDefinition(IPlugin* plugin, ShaderStage stage, const char* itemType, const char* filePath);

		void m_applyBreakpoints(TextEditor* editor, const std::string& path);
		void m_loadEditorShortcuts(TextEditor* editor);

		// font for code editor
		ImFont* m_font;

		// menu bar item actions
		void m_save(int id);
		void m_compile(int id);
		//void m_fetchStats(int id);
		//void m_renderStats(int id);

		std::vector<PipelineItem*> m_items;
		std::vector<TextEditor> m_editor; // TODO: use pointers here
		std::vector<StatsPage> m_stats;
		std::vector<std::string> m_paths;
		std::vector<ShaderStage> m_shaderStage;
		std::deque<bool> m_editorOpen;

		int m_savePopupOpen;
		bool m_fontNeedsUpdate;
		std::string m_fontFilename;
		int m_fontSize;

		bool m_focusWindow;
		ShaderStage m_focusStage;
		std::string m_focusItem;

		int m_selectedItem;

		// auto recompile
		std::thread* m_autoRecompileThread;
		void m_autoRecompiler();
		std::atomic<bool> m_autoRecompilerRunning, m_autoRecompileRequest;
		std::vector<ed::MessageStack::Message> m_autoRecompileCachedMsgs;
		bool m_autoRecompile;
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