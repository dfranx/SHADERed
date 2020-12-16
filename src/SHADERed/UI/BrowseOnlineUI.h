#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class BrowseOnlineUI : public UIView {
	public:
		BrowseOnlineUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true);

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Open();
		void FreeMemory();

	private:
		std::vector<ed::WebAPI::ShaderResult> m_onlineShaders;
		std::vector<GLuint> m_onlineShaderThumbnail;
		bool m_refreshShaderThumbnails, m_refreshPluginThumbnails, m_refreshThemeThumbnails;
		unsigned char* m_queueThumbnailPixelData;
		int m_queueThumbnailWidth, m_queueThumbnailHeight;
		bool m_queueThumbnailNext, m_queueThumbnailProcess;
		int m_queueThumbnailID;

		std::vector<ed::WebAPI::PluginResult> m_onlinePlugins;
		std::vector<GLuint> m_onlinePluginThumbnail;
		std::vector<ed::WebAPI::ThemeResult> m_onlineThemes;
		std::vector<GLuint> m_onlineThemeThumbnail;
		std::vector<std::string> m_onlineInstalledPlugins;
		int m_onlinePage, m_onlineShaderPage, m_onlinePluginPage, m_onlineThemePage;
		bool m_onlineIsShader, m_onlineIsPlugin;
		char m_onlineQuery[256];
		char m_onlineUsername[64];
		bool m_onlineExcludeGodot, m_onlineIncludeCPP, m_onlineIncludeRust;
		void m_onlineSearchShaders();
		void m_onlineSearchPlugins();
		void m_onlineSearchThemes();
	};
}