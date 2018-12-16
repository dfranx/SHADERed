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
		for (int i = 0; i < m_items.size(); i++)
			delete m_items[i].Data;
		m_items.clear();
	}
	void PipelineManager::Add(const char* name, PipelineItem type, void * data)
	{
		m_items.push_back({ "\0", type, data });
		memcpy(m_items.at(m_items.size()-1).Name, name, strlen(name));
	}
	void PipelineManager::Remove(const char* name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i].Name, name) == 0) {
				delete m_items[i].Data;
				m_items[i].Data = nullptr;
				m_items.erase(m_items.begin() + i);
				break;
			}
	}
	bool PipelineManager::Has(const char * name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i].Name, name) == 0)
				return true;
		return false;
	}
	PipelineManager::Item& PipelineManager::Get(const char* name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i].Name, name) == 0)
				return m_items[i];
	}
	PipelineManager::Item* PipelineManager::GetPtr(const char * name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i].Name, name) == 0)
				return &m_items[i];
		return nullptr;
	}
	void PipelineManager::New()
	{
		Clear();

		m_project->ResetProjectDirectory();
		m_project->OpenTemplate();
	}
}