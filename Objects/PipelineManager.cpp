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
			pipe::ShaderPass* pass = (pipe::ShaderPass*)m_items[i]->Data;
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
	bool PipelineManager::AddItem(const char * owner, const char * name, PipelineItem::ItemType type, void * data)
	{
		if (type == PipelineItem::ItemType::ShaderPass)
			return AddPass(name, (pipe::ShaderPass*)data);

		for (auto item : m_items)
			if (strcmp(item->Name, name) == 0)
				return false;

		for (auto item : m_items) {
			if (strcmp(item->Name, owner) != 0)
				continue;

			pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;

			for (auto& i : pass->Items)
				if (strcmp(i->Name, name) == 0)
					return false;

			pass->Items.push_back(new PipelineItem{ "\0", type, data });
			strcpy(pass->Items.at(pass->Items.size() - 1)->Name, name);

			return true;
		}

		return false;
	}
	bool PipelineManager::AddPass(const char * name, pipe::ShaderPass* data)
	{
		for (auto& item : m_items)
			if (strcmp(item->Name, name) == 0)
				return false;

		m_items.push_back(new PipelineItem{ "\0", PipelineItem::ItemType::ShaderPass, data });
		strcpy(m_items.at(m_items.size() - 1)->Name, name);

		return true;
	}
	void PipelineManager::Remove(const char* name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i]->Name, name) == 0) {
				delete m_items[i]->Data;
				m_items[i]->Data = nullptr;
				m_items.erase(m_items.begin() + i);
				break;
			} else {
				pipe::ShaderPass* data = (pipe::ShaderPass*)m_items[i]->Data;
				for (int j = 0; j < data->Items.size(); j++) {
					if (strcmp(data->Items[j]->Name, name) == 0) {
						delete data->Items[j]->Data;
						data->Items[j]->Data = nullptr;
						data->Items.erase(data->Items.begin() + j);
						break;
					}
				}
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
		return nullptr;
	}
	void PipelineManager::New(bool openTemplate)
	{
		Clear();

		m_project->ResetProjectDirectory();

		if (openTemplate)
			m_project->OpenTemplate();
	}
}