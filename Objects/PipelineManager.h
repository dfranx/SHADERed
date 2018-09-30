#pragma once
#include <vector>
#include "PipelineItem.h"

#define PIPELINE_ITEM_NAME_LENGTH 64

namespace ed
{
	class PipelineManager
	{
	public:
		struct Item
		{
			char Name[PIPELINE_ITEM_NAME_LENGTH];
			PipelineItem Type;
			void* Data;
		};

		PipelineManager(ml::Window* wnd);
		~PipelineManager();

		void Clear();

		void Add(const char* name, PipelineItem type, void* data);
		void Remove(const char* name);
		bool Has(const char* name);
		Item& Get(const char* name);
		inline std::vector<Item>& GetList() { return m_items; }

		void New();

	private:
		ml::Window* m_wnd;
		std::vector<Item> m_items;
	};
}