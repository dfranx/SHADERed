#include "PipelineManager.h"
#include "ProjectParser.h"
#include "Logger.h"
#include "../Options.h"
#include "SystemVariableManager.h"

int strcmpcase(const char* s1, const char* s2)
{
	const unsigned char* p1 = (const unsigned char*)s1;
	const unsigned char* p2 = (const unsigned char*)s2;
	int result;
	if (p1 == p2)
		return 0;
	while ((result = tolower(*p1) - tolower(*p2++)) == 0)
		if (*p1++ == '\0')
			break;
	return result;
}

namespace ed
{
	PipelineManager::PipelineManager(ProjectParser* project)
	{
		m_project = project;
	}
	PipelineManager::~PipelineManager()
	{
		Clear();
	}
	void PipelineManager::Clear()
	{
		Logger::Get().Log("Clearing PipelineManager contents");

		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
				// delete pass' child items and their data
				pipe::ShaderPass* pass = (pipe::ShaderPass*)m_items[i]->Data;
				for (auto& passItem : pass->Items) {
					if (passItem->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem* geo = (pipe::GeometryItem*)passItem->Data;
						glDeleteVertexArrays(1, &geo->VAO);
						glDeleteVertexArrays(1, &geo->VBO);
					}

					delete passItem->Data;
					delete passItem;
				}
				pass->Items.clear();

				glDeleteFramebuffers(1, &pass->FBO);
			}

			// delete passes and their data
			delete m_items[i]->Data;
			delete m_items[i];
		}
		m_items.clear();
	}
	bool PipelineManager::AddItem(const char * owner, const char * name, PipelineItem::ItemType type, void * data)
	{
		if (type == PipelineItem::ItemType::ShaderPass)
			return AddShaderPass(name, (pipe::ShaderPass*)data);
		else if (type == PipelineItem::ItemType::ComputePass)
			return AddComputePass(name, (pipe::ComputePass*)data);
		
		Logger::Get().Log("Adding a pipeline item " + std::string(name) + " to the project");

		for (const auto& item : m_items)
			if (strcmpcase(item->Name, name) == 0) {
				Logger::Get().Log("Item " + std::string(name) + " not added - name already taken", true);
				return false;
			}

		m_project->ModifyProject();

		for (auto& item : m_items) {
			if (strcmp(item->Name, owner) != 0)
				continue;

			pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;

			for (auto& i : pass->Items)
				if (strcmpcase(i->Name, name) == 0) {
					Logger::Get().Log("Item " + std::string(name) + " not added - name already taken", true);
					return false;
				}

			pass->Items.push_back(new PipelineItem("\0", type, data));
			strcpy(pass->Items.at(pass->Items.size() - 1)->Name, name);

			Logger::Get().Log("Item " + std::string(name) + " added to the project");
			
			return true;
		}

		return false;
	}
	bool PipelineManager::AddShaderPass(const char * name, pipe::ShaderPass* data)
	{
		if (Has(name)) {
			Logger::Get().Log("Shader pass " + std::string(name) + " not added - name already taken", true);
			return false;
		}

		m_project->ModifyProject();

		Logger::Get().Log("Added a shader pass " + std::string(name) + " to the project");

		m_items.push_back(new PipelineItem("\0", PipelineItem::ItemType::ShaderPass, data));
		strcpy(m_items.at(m_items.size() - 1)->Name, name);

		return true;
	}
	bool PipelineManager::AddComputePass(const char *name, pipe::ComputePass *data)
	{
		if (Has(name)) {
			Logger::Get().Log("Compute pass " + std::string(name) + " not added - name already taken", true);
			return false;
		}

		m_project->ModifyProject();

		Logger::Get().Log("Added a shader pass " + std::string(name) + " to the project");

		m_items.push_back(new PipelineItem("\0", PipelineItem::ItemType::ComputePass, data));
		strcpy(m_items.at(m_items.size() - 1)->Name, name);

		return true;
	}
	void PipelineManager::Remove(const char* name)
	{
		Logger::Get().Log("Deleting item " + std::string(name));
		
		for (int i = 0; i < m_items.size(); i++)
			if (strcmp(m_items[i]->Name, name) == 0) {
				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)m_items[i]->Data;
					glDeleteFramebuffers(1, &data->FBO);

					// TODO: add this part to m_freeShaderPass method
					for (auto& passItem : data->Items) {
						if (passItem->Type == PipelineItem::ItemType::Geometry) {
							pipe::GeometryItem *geo = (pipe::GeometryItem *)passItem->Data;
							glDeleteVertexArrays(1, &geo->VAO);
							glDeleteVertexArrays(1, &geo->VBO);
						}

						delete passItem->Data;
						delete passItem;
					}
					data->Items.clear();
				}

				delete m_items[i]->Data;
				m_items[i]->Data = nullptr;
				m_items.erase(m_items.begin() + i);
				break;
			} else {
				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)m_items[i]->Data;
					for (int j = 0; j < data->Items.size(); j++) {
						if (strcmp(data->Items[j]->Name, name) == 0) {
							ed::PipelineItem* child = data->Items[j];
							if (child->Type == PipelineItem::ItemType::Geometry) {
								pipe::GeometryItem* geo = (pipe::GeometryItem*)child->Data;
								glDeleteVertexArrays(1, &geo->VAO);
								glDeleteVertexArrays(1, &geo->VBO);
							}

							delete data->Items[j]->Data;
							data->Items[j]->Data = nullptr;
							data->Items.erase(data->Items.begin() + j);
							break;
						}
					}
				}
			}

			
		m_project->ModifyProject();
	}
	bool PipelineManager::Has(const char * name)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (strcmpcase(m_items[i]->Name, name) == 0)
				return true;
			else {
				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)m_items[i]->Data;
					for (int j = 0; j < data->Items.size(); j++)
						if (strcmpcase(data->Items[j]->Name, name) == 0)
							return true;
				}
			}
		}
		return false;
	}
	char* PipelineManager::GetItemOwner(const char* name)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* data = (pipe::ShaderPass*)m_items[i]->Data;
				for (int j = 0; j < data->Items.size(); j++)
					if (strcmp(data->Items[j]->Name, name) == 0)
						return m_items[i]->Name;
			}
		}
		return nullptr;
	}
	PipelineItem* PipelineManager::Get(const char* name)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (strcmp(m_items[i]->Name, name) == 0)
				return m_items[i];
			else {
				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)m_items[i]->Data;
					for (int j = 0; j < data->Items.size(); j++)
						if (strcmp(data->Items[j]->Name, name) == 0)
							return data->Items[j];
				}
			}
		}
		return nullptr;
	}
	void PipelineManager::New(bool openTemplate)
	{
		Logger::Get().Log("Creating a new project from template");

		Clear();

		m_project->ResetProjectDirectory();

		if (openTemplate && m_project->GetTemplate() != "?empty")
			m_project->OpenTemplate();
		
		// reset time, frame index, etc...
		SystemVariableManager::Instance().Reset();
	}
}