#pragma once
#include <vector>
#include "PipelineItem.h"
#include "../Options.h"

namespace ed
{
	class ProjectParser;

	class PipelineManager
	{
	public:
		struct Item
		{
			char Name[PIPELINE_ITEM_NAME_LENGTH];
			PipelineItem Type;
			void* Data;
		};

		PipelineManager(ml::Window* wnd, ProjectParser* project);
		~PipelineManager();

		void Clear();

		void Add(const char* name, PipelineItem type, void* data);
		void Remove(const char* name);
		bool Has(const char* name);
		Item* Get(const char* name);
		inline std::vector<Item*>& GetList() { return m_items; }

		void New();

		inline ml::Window* GetOwner() { return m_wnd; }

	private:
		ml::Window* m_wnd;
		ProjectParser* m_project;
		std::vector<Item*> m_items;
	};
}