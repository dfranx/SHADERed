#pragma once
#include <vector>
#include "PipelineItem.h"

namespace ed
{
	class PipelineManager
	{
	public:
		struct Item
		{
			std::string Name;
			PipelineItem Type;
			void* Data;
		};

		PipelineManager(ml::Window* wnd);
		~PipelineManager();

		void Clear();

		void Add(const std::string& name, PipelineItem type, void* data);
		void Remove(const std::string& name);
		Item& Get(const std::string& name);
		inline std::vector<Item>& GetList() { return m_items; }

		void New();

	private:
		ml::Window* m_wnd;
		std::vector<Item> m_items;
	};
}