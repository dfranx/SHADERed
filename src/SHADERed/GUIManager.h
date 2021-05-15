#pragma once
#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/ShaderVariableContainer.h>
#include <SHADERed/Objects/CommandLineOptionParser.h>
#include <SHADERed/Objects/SPIRVParser.h>
#include <SHADERed/Objects/WebAPI.h>
#include <SHADERed/UI/Tools/NotificationSystem.h>
#include <ImGuiColorTextEdit/TextEditor.h>
#include <SHADERed/Engine/Timer.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

class ImFont;

namespace ed {
	class InterfaceManager;
	class CreateItemUI;
	class UIView;
	class Settings;

	enum class ViewID {
		Preview,
		Pinned,
		Code,
		Output,
		Objects,
		Pipeline,
		Properties,
		PixelInspect,
		Profiler,
		DebugWatch,
		DebugValues,
		DebugFunctionStack,
		DebugBreakpointList,
		DebugVectorWatch,
		DebugAuto,
		DebugImmediate,
		DebugGeometryOutput,
		DebugTessControlOutput,
		Options,
		ObjectPreview,
		FrameAnalysis,
	};

	class GUIManager {
	public:
		GUIManager(ed::InterfaceManager* objs, SDL_Window* wnd, SDL_GLContext* gl);
		~GUIManager();

		void OnEvent(const SDL_Event& e);
		void Update(float delta);
		void Render();

		void Destroy();

		UIView* Get(ViewID view);
		inline SDL_Window* GetSDLWindow() { return m_wnd; }

		void ResetWorkspace();
		void SaveSettings();
		void LoadSettings();

		void CreateNewShaderPass();
		void CreateNewTexture();
		void CreateNewTexture3D();
		inline void CreateNewCubemap() { m_isCreateCubemapOpened = true; }
		void CreateNewAudio();
		inline void CreateNewRenderTexture() { m_isCreateRTOpened = true; }
		inline void CreateNewBuffer() { m_isCreateBufferOpened = true; }
		inline void CreateNewImage() { m_isCreateImgOpened = true; }
		inline void CreateNewImage3D() { m_isCreateImg3DOpened = true; }
		inline void CreateNewCameraSnapshot() { m_isRecordCameraSnapshotOpened = true; }
		inline void CreateKeyboardTexture() { m_isCreateKBTxtOpened = true; }
		void CreateNewComputePass();
		void CreateNewAudioPass();

		inline bool IsPerformanceMode() { return m_performanceMode; }
		inline void SetPerformanceMode(bool mode) { m_perfModeFake = mode; }
		inline bool IsMinimalMode() { return m_minimalMode; }
		inline void SetMinimalMode(bool mode) { m_minimalMode = mode; }
		inline bool IsFocusMode() { return m_focusMode; }
		inline void SetFocusMode(bool mode) { m_focusMode = mode; }

		void AddNotification(int id, const char* text, const char* btnText, std::function<void(int, IPlugin1*)> fn, IPlugin1* plugin = nullptr);

		void StopDebugging();

		int AreYouSure();

		bool Save();
		void SaveAsProject(bool restoreCached = false, std::function<void(bool)> handle = nullptr, std::function<void()> preHandle = nullptr);
		void Open(const std::string& file);

		void SetCommandLineOptions(CommandLineOptionParser& options);

		void SavePreviewToFile();

	private:
		void m_renderPopups(float delta);

		void m_setupShortcuts();

		void m_autoUniforms(ShaderVariableContainer& vars, SPIRVParser& spv, std::vector<std::string>& uniformList);
		void m_deleteUnusedUniforms(ShaderVariableContainer& vars, const std::vector<std::string>& spv);

		void m_addProjectToRecents(const std::string& file);

		void m_createNewProject();

		void m_tooltip(const std::string& str);
		void m_renderToolbar();
		ImFont* m_iconFontLarge;

		CommandLineOptionParser* m_cmdArguments;

		std::vector<std::string> m_recentProjects;

		int m_width, m_height;

		void m_renderOptions();
		bool m_optionsOpened;
		int m_optGroup;
		UIView* m_options;
		UIView* m_browseOnline;
		UIView* m_objectPrev;
		UIView* m_geometryOutput;
		UIView* m_frameAnalysis;
		UIView* m_tessControlOutput;

		std::string m_cachedFont;
		int m_cachedFontSize;
		bool m_fontNeedsUpdate;

		bool m_minimalMode;
		bool m_focusMode, m_focusModeTemp;
		void m_renderFocusMode();
		void m_renderTextEditorFocusMode(const std::string& name, void* item, ShaderStage stage);

		void m_renderDAPMode(float delta);

		bool m_cacheProjectModified;
		bool m_recompiledAll;

		bool m_isCreateItemPopupOpened, m_isCreateRTOpened,
			m_isCreateCubemapOpened,
			m_isAboutOpen, m_isCreateBufferOpened, m_isCreateImgOpened,
			m_isInfoOpened, m_isCreateImg3DOpened, m_isRecordCameraSnapshotOpened,
			m_isIncompatPluginsOpened, m_isCreateKBTxtOpened, m_isBrowseOnlineOpened;

		Settings* m_settingsBkp;
		std::map<std::string, KeyboardShortcuts::Shortcut> m_shortcutsBkp;

		bool m_exportAsCPPOpened;
		bool m_savePreviewPopupOpened;
		bool m_wasPausedPrior;
		float m_savePreviewTime, m_savePreviewCachedTime, m_savePreviewTimeDelta;
		int m_savePreviewFrameIndex, m_savePreviewCachedFIndex;
		int m_savePreviewSupersample;
		bool m_savePreviewWASD[4];
		glm::vec4 m_savePreviewMouse;
		std::string m_previewSavePath;
		glm::ivec2 m_previewSaveSize;

		bool m_isChangelogOpened;
		std::string m_changelogText, m_changelogBlogLink;
		void m_checkChangelog();

		std::string m_saveAsOldFile;
		bool m_saveAsReset;
		bool m_saveAsRestoreCache;
		std::function<void(bool)> m_saveAsHandle;
		std::function<void()> m_saveAsPreHandle;


		bool m_expcppError;
		int m_expcppBackend;
		bool m_expcppCmakeFiles;
		bool m_expcppCmakeModules;
		bool m_expcppImage;
		bool m_expcppMemoryShaders;
		bool m_expcppCopyImages;
		char m_expcppProjectName[64];
		std::string m_expcppSavePath;

		bool m_tipOpened;
		int m_tipIndex, m_tipCount;
		std::string m_tipTitle, m_tipText;

		void m_splashScreenRender();
		void m_splashScreenLoad();
		bool m_splashScreen;
		unsigned int m_splashScreenIcon, m_splashScreenText;
		unsigned int m_sponsorDigitalOcean, m_sponsorEmbark;
		unsigned int m_splashScreenFrame;
		eng::Timer m_splashScreenTimer;
		bool m_splashScreenLoaded;

		bool m_savePreviewSeq;
		float m_savePreviewSeqDuration;
		int m_savePreviewSeqFPS;

		bool m_performanceMode, m_perfModeFake;
		eng::Timer m_perfModeClock;

		std::string m_uiIniFile;

		std::string* m_cubemapPathPtr;

		std::string m_selectedTemplate;
		std::vector<std::string> m_templates;
		void m_loadTemplateList();

		NotificationSystem m_notifs;

		std::vector<UIView*> m_views, m_debugViews;
		CreateItemUI* m_createUI;

		TextEditor m_kbInfo;

		InterfaceManager* m_data;
		SDL_Window* m_wnd;
		SDL_GLContext* m_gl;
	};
}