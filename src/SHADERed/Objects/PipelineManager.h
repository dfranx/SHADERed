#pragma once
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/PluginManager.h>
#include <SHADERed/Options.h>
#include <vector>

namespace ed {
	class ProjectParser;

	class PipelineManager {
	public:
		PipelineManager(ProjectParser* project, PluginManager* plugins);
		~PipelineManager();

		void Clear();

		bool AddItem(const char* owner, const char* name, PipelineItem::ItemType type, void* data);
		bool AddPluginItem(char* owner, const char* name, const char* type, void* data, IPlugin1* plugin);
		bool AddShaderPass(const char* name, ed::pipe::ShaderPass* data);
		bool AddComputePass(const char* name, pipe::ComputePass* data);
		bool AddAudioPass(const char* name, pipe::AudioPass* data);
		void Remove(const char* name);
		bool Has(const char* name);
		PipelineItem* Get(const char* name);
		char* GetItemOwner(const char* name);
		inline std::vector<PipelineItem*>& GetList() { return m_items; }

		void New(bool openTemplate = true);

		void FreeData(void* data, PipelineItem::ItemType type);

	private:
		PluginManager* m_plugins;
		ProjectParser* m_project;
		std::vector<PipelineItem*> m_items;
	};
}