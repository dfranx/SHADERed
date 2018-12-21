#include "PipelineManager.h"
#include "ProjectParser.h"
#include "../Options.h"
#include <MoonLight/Base/Topology.h>
#include <MoonLight/Base/GeometryFactory.h>

namespace ed
{
	PipelineManager::PipelineManager(ml::Window* wnd, ProjectParser* project)
	{
		m_wnd = wnd;
		m_project = project;
	}
	PipelineManager::~PipelineManager()
	{
		Clear();
	}
	void PipelineManager::Clear()
	{
		for (int i = 0; i < m_items.size(); i++) {
			// delete pass' child items and their data
			auto pass = (ed::pipe::ShaderPass*)m_items[i]->Data;
			for (auto passItem : pass->Items) {
				delete passItem->Data;
				delete passItem;
			}
			pass->Items.clear();

			// delete passes and their data
			delete m_items[i]->Data;
			delete m_items[i];
		}
		m_items.clear();
	}
	void PipelineManager::AddItem(const char * owner, const char * name, PipelineItem::ItemType type, void * data)
	{
		if (type == PipelineItem::ItemType::ShaderPass) {
			AddPass(name, (pipe::ShaderPass*)data);
			return;
		}

		for (auto item : m_items) {
			if (strcmp(item->Name, owner) != 0)
				continue;

			auto pass = (ed::pipe::ShaderPass*)item->Data;

			pass->Items.push_back(new PipelineItem{ "\0", type, data });
			memcpy(pass->Items.at(m_items.size() - 1)->Name, name, strlen(name));
		}
	}
	void PipelineManager::AddPass(const char * name, pipe::ShaderPass* data)
	{
		m_items.push_back(new PipelineItem{ "\0", PipelineItem::ItemType::ShaderPass, data });
		memcpy(m_items.at(m_items.size() - 1)->Name, name, strlen(name));
	}
	void PipelineManager::Remove(const char* name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i]->Name, name) == 0) {
				delete m_items[i]->Data;
				m_items[i]->Data = nullptr;
				m_items.erase(m_items.begin() + i);
				break;
			}
	}
	bool PipelineManager::Has(const char * name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i]->Name, name) == 0)
				return true;
		return false;
	}
	PipelineItem* PipelineManager::Get(const char* name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i]->Name, name) == 0)
				return m_items[i];
	}
	void PipelineManager::New()
	{
		Clear();

		m_project->ResetProjectDirectory();
		m_project->OpenTemplate();
	}
}