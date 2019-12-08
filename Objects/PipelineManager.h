#pragma once
#include <vector>
#include "../Options.h"
#include "PipelineItem.h"

namespace ed
{
	class ProjectParser;

	class PipelineManager
	{
	public:

		PipelineManager(ProjectParser* project);
		~PipelineManager();

		void Clear();

		bool AddItem(const char* owner, const char* name, PipelineItem::ItemType type, void* data);
		bool AddPluginItem(char* owner, const char* name, const char* type, void* data, IPlugin* plugin);
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

		ProjectParser* m_project;
		std::vector<PipelineItem*> m_items;
	};
}