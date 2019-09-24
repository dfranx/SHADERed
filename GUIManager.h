#pragma once
#include "Objects/KeyboardShortcuts.h"
#include "Objects/UpdateChecker.h"

#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>
#include <SFML/System/Clock.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>

class ImFont;

namespace ed
{
	class InterfaceManager;
	class CreateItemUI;
	class UIView;
	class Settings;

	enum class ViewID
	{
		Preview,
		Pinned,
		Code,
		Output,
		Objects,
		Pipeline,
		Properties,
		Options,
		ObjectPreview
	};

	class GUIManager
	{
	public:
		GUIManager(ed::InterfaceManager* objs, SDL_Window* wnd, SDL_GLContext* gl);
		~GUIManager();

		void OnEvent(const SDL_Event& e);
		void Update(float delta);
		void Render();

		UIView* Get(ViewID view);
		inline SDL_Window* GetSDLWindow() { return m_wnd; }

		void SaveSettings();
		void LoadSettings();

		void CreateNewShaderPass();
		void CreateNewTexture();
		inline void CreateNewCubemap() { m_isCreateCubemapOpened = true; }
		void CreateNewAudio();
		inline void CreateNewRenderTexture() { m_isCreateRTOpened = true; }
		inline void CreateNewBuffer() { m_isCreateBufferOpened = true; }
		inline void CreateNewImage() { m_isCreateImgOpened = true; }
		void CreateNewComputePass();

		inline bool IsPerformanceMode() { return m_performanceMode; }
		inline void SetPerformanceMode(bool mode) { m_perfModeFake = mode; }

		bool SaveAsProject(bool restoreCached = false);
		void Open(const std::string& file);

	private:
		void m_setupShortcuts();

		void m_imguiHandleEvent(const SDL_Event& e);

		void m_tooltip(const std::string& str);
		void m_renderToolbar();
		ImFont* m_iconFontLarge;

		void m_renderOptions();
		bool m_optionsOpened;
		int m_optGroup;
		UIView* m_options;

		UIView* m_objectPrev;

		std::string m_cachedFont;
		int m_cachedFontSize;
		bool m_fontNeedsUpdate;
		float m_cacheUIScale;

		bool m_isCreateItemPopupOpened, m_isCreateRTOpened,
			m_isCreateCubemapOpened, m_isNewProjectPopupOpened,
			m_isAboutOpen, m_isCreateBufferOpened, m_isCreateImgOpened;

		bool m_isUpdateNotificationOpened;
		sf::Clock m_updateNotifyClock;

		Settings* m_settingsBkp;
		std::map<std::string, KeyboardShortcuts::Shortcut> m_shortcutsBkp;

		bool m_savePreviewPopupOpened;
		bool m_wasPausedPrior;
		float m_savePreviewTime, m_savePreviewCachedTime, m_savePreviewTimeDelta;
		int m_savePreviewFrameIndex, m_savePreviewCachedFIndex;
		bool m_savePreviewWASD[4];
		glm::vec4 m_savePreviewMouse;
		std::string m_previewSavePath;
		glm::ivec2 m_previewSaveSize;
		
		bool m_savePreviewSeq;
		float m_savePreviewSeqDuration;
		int m_savePreviewSeqFPS;

		bool m_performanceMode, m_perfModeFake;

		std::string m_selectedTemplate;
		std::vector<std::string> m_templates;
		void m_loadTemplateList();

		std::vector<UIView*> m_views;
		CreateItemUI* m_createUI;

		UpdateChecker m_updateCheck;

		InterfaceManager* m_data;
		SDL_Window* m_wnd;
		SDL_GLContext* m_gl;
	};
}