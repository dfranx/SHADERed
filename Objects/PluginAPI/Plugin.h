#pragma once

namespace ed
{
	// CreatePlugin(), DestroyPlugin(ptr), GetPluginAPIVersion(), GetPluginName()
	class IPlugin
	{
	public:
		virtual bool Init() = 0;
		virtual void OnEvent(void* e) = 0; // e is &SDL_Event, it is void here so that people don't have to link to SDL if they don't want to
		virtual void Update(float delta) = 0;
		virtual void Destroy() = 0;

		virtual bool HasCustomMenu() = 0;

		/* list: file, newproject, project, createitem, window, custom */
		virtual bool HasMenuItems(const char* name) = 0;
		virtual void ShowMenuItems(const char* name) = 0;

		/* list: pipeline, shaderpass_add, objects */
		virtual bool HasContextItems(const char* name) = 0;
		virtual void ShowContextItems(const char* name) = 0;
	};
}