#pragma once

namespace ed
{
	// CreatePlugin(), DestroyPlugin(ptr), GetPluginAPIVersion(), GetPluginName()
	class IPlugin
	{
	public:
		virtual bool Init() = 0;
		virtual void OnEvent() = 0;
		virtual void Update(float delta) = 0;
		virtual void Destroy() = 0;

		virtual void ShowMenuFileItems() = 0;
		virtual bool HasMenuFileItems() = 0;
	};
}