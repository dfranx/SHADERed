#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Options.h>

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

namespace ed {
	PipelineManager::PipelineManager(ProjectParser* project, PluginManager* plugins)
	{
		m_project = project;
		m_plugins = plugins;
	}
	PipelineManager::~PipelineManager()
	{
		Clear();
	}
	void PipelineManager::Clear()
	{
		Logger::Get().Log("Clearing PipelineManager contents");

		while (m_items.size() > 0)
			Remove(m_items[0]->Name);
	}
	bool PipelineManager::AddItem(const char* owner, const char* name, PipelineItem::ItemType type, void* data)
	{
		if (type == PipelineItem::ItemType::ShaderPass)
			return AddShaderPass(name, (pipe::ShaderPass*)data);
		else if (type == PipelineItem::ItemType::ComputePass)
			return AddComputePass(name, (pipe::ComputePass*)data);
		else if (type == PipelineItem::ItemType::AudioPass)
			return AddAudioPass(name, (pipe::AudioPass*)data);
		else if (type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* pdata = (pipe::PluginItemData*)data; // TODO: memory leak here?
			return AddPluginItem(const_cast<char*>(owner), name, pdata->Type, pdata->PluginData, pdata->Owner);
		}

		if (Has(name)) {
			Logger::Get().Log("Item " + std::string(name) + " not added - name already taken", true);
			return false;
		}

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

			if (item->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pdata = (pipe::PluginItemData*)item->Data;

				pdata->Items.push_back(new PipelineItem("\0", type, data));
				strcpy(pdata->Items.at(pdata->Items.size() - 1)->Name, name);

				pdata->Owner->PipelineItem_AddChild(owner, name, (plugin::PipelineItemType)type, data);

				m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemAdded, (void*)name, nullptr);

				return true;
			} else if (item->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;

				for (auto& i : pass->Items)
					if (strcmpcase(i->Name, name) == 0) {
						Logger::Get().Log("Item " + std::string(name) + " not added - name already taken", true);
						return false;
					}

				pass->Items.push_back(new PipelineItem("\0", type, data));
				strcpy(pass->Items.at(pass->Items.size() - 1)->Name, name);

				Logger::Get().Log("Item " + std::string(name) + " added to the project");

				m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemAdded, (void*)name, nullptr);

				return true;
			}
		}

		return false;
	}
	bool PipelineManager::AddPluginItem(char* owner, const char* name, const char* type, void* data, IPlugin1* plugin)
	{
		Logger::Get().Log("Adding a plugin pipeline item " + std::string(name) + " to the project");

		if (Has(name)) {
			Logger::Get().Log("Item " + std::string(name) + " not added - name already taken", true);
			return false;
		}

		m_project->ModifyProject();

		if (owner != nullptr) {
			for (auto& item : m_items) {
				if (strcmp(item->Name, owner) != 0)
					continue;

				pipe::PluginItemData* pdata = new pipe::PluginItemData();
				pdata->PluginData = data;
				pdata->Owner = plugin;
				strcpy(pdata->Type, type);

				PipelineItem* pitem = new PipelineItem("\0", PipelineItem::ItemType::PluginItem, pdata);
				strcpy(pitem->Name, name);

				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)item->Data;
					pass->Items.push_back(pitem);
				} else if (item->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* plPass = (pipe::PluginItemData*)item->Data;
					plPass->Items.push_back(pitem);
					plPass->Owner->PipelineItem_AddChild(owner, pitem->Name, plugin::PipelineItemType::PluginItem, data);
				}

				Logger::Get().Log("Item " + std::string(name) + " added to the project");
				
				m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemAdded, (void*)name, nullptr);

				return true;
			}
		} else {
			pipe::PluginItemData* pdata = new pipe::PluginItemData();
			pdata->PluginData = data;
			pdata->Owner = plugin;
			strcpy(pdata->Type, type);

			PipelineItem* pitem = new PipelineItem("\0", PipelineItem::ItemType::PluginItem, pdata);
			m_items.push_back(pitem);
			strcpy(pitem->Name, name);

			m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemAdded, (void*)name, nullptr);

			return true;
		}

		return false;
	}
	bool PipelineManager::AddShaderPass(const char* name, pipe::ShaderPass* data)
	{
		if (Has(name)) {
			Logger::Get().Log("Shader pass " + std::string(name) + " not added - name already taken", true);
			return false;
		}

		m_project->ModifyProject();

		Logger::Get().Log("Added a shader pass " + std::string(name) + " to the project");

		m_items.push_back(new PipelineItem("\0", PipelineItem::ItemType::ShaderPass, data));
		strcpy(m_items.at(m_items.size() - 1)->Name, name);

		m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemAdded, (void*)name, nullptr);

		return true;
	}
	bool PipelineManager::AddComputePass(const char* name, pipe::ComputePass* data)
	{
		if (Has(name)) {
			Logger::Get().Log("Compute pass " + std::string(name) + " not added - name already taken", true);
			return false;
		}

		m_project->ModifyProject();

		Logger::Get().Log("Added a compute pass " + std::string(name) + " to the project");

		m_items.push_back(new PipelineItem("\0", PipelineItem::ItemType::ComputePass, data));
		strcpy(m_items.at(m_items.size() - 1)->Name, name);

		m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemAdded, (void*)name, nullptr);

		return true;
	}
	bool PipelineManager::AddAudioPass(const char* name, pipe::AudioPass* data)
	{
		if (Has(name)) {
			Logger::Get().Log("Compute pass " + std::string(name) + " not added - name already taken", true);
			return false;
		}

		m_project->ModifyProject();

		Logger::Get().Log("Added a audio pass " + std::string(name) + " to the project");

		m_items.push_back(new PipelineItem("\0", PipelineItem::ItemType::AudioPass, data));
		strcpy(m_items.at(m_items.size() - 1)->Name, name);

		m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemAdded, (void*)name, nullptr);

		return true;
	}
	void PipelineManager::Remove(const char* name)
	{
		Logger::Get().Log("Deleting item " + std::string(name));

		m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemDeleted, (void*)name, nullptr);

		for (int i = 0; i < m_items.size(); i++) {
			if (strcmp(m_items[i]->Name, name) == 0) {
				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)m_items[i]->Data;
					glDeleteFramebuffers(1, &data->FBO);

					// TODO: add this part to m_freeShaderPass method
					for (auto& passItem : data->Items) {
						if (passItem->Type == PipelineItem::ItemType::Geometry) {
							pipe::GeometryItem* geo = (pipe::GeometryItem*)passItem->Data;
							glDeleteVertexArrays(1, &geo->VAO);
							glDeleteVertexArrays(1, &geo->VBO);
						} else if (passItem->Type == PipelineItem::ItemType::PluginItem) {
							pipe::PluginItemData* pdata = (pipe::PluginItemData*)passItem->Data;
							pdata->Owner->PipelineItem_Remove(passItem->Name, pdata->Type, pdata->PluginData);
						} else if (passItem->Type == PipelineItem::ItemType::VertexBuffer) {
							pipe::VertexBuffer* vb = (pipe::VertexBuffer*)passItem->Data;
							glDeleteVertexArrays(1, &vb->VAO);
						} 

						FreeData(passItem->Data, passItem->Type);
						delete passItem;
					}
					data->Items.clear();
				} else if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* pdata = (pipe::PluginItemData*)m_items[i]->Data;
					pdata->Owner->PipelineItem_Remove(m_items[i]->Name, pdata->Type, pdata->PluginData);

					for (auto& passItem : pdata->Items) {
						if (passItem->Type == PipelineItem::ItemType::Geometry) {
							pipe::GeometryItem* geo = (pipe::GeometryItem*)passItem->Data;
							glDeleteVertexArrays(1, &geo->VAO);
							glDeleteVertexArrays(1, &geo->VBO);
						} else if (passItem->Type == PipelineItem::ItemType::PluginItem) {
							pipe::PluginItemData* pldata = (pipe::PluginItemData*)passItem->Data;
							pdata->Owner->PipelineItem_Remove(passItem->Name, pldata->Type, pldata->PluginData);
						} else if (passItem->Type == PipelineItem::ItemType::VertexBuffer) {
							pipe::VertexBuffer* vb = (pipe::VertexBuffer*)passItem->Data;
							glDeleteVertexArrays(1, &vb->VAO);
						} 

						FreeData(passItem->Data, passItem->Type);
						delete passItem;
					}
					pdata->Items.clear();
				}

				FreeData(m_items[i]->Data, m_items[i]->Type);
				m_items[i]->Data = nullptr;
				delete m_items[i];
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
							} else if (child->Type == PipelineItem::ItemType::PluginItem) {
								pipe::PluginItemData* pdata = (pipe::PluginItemData*)child->Data;
								pdata->Owner->PipelineItem_Remove(child->Name, pdata->Type, pdata->PluginData);
							} else if (child->Type == PipelineItem::ItemType::VertexBuffer) {
								pipe::VertexBuffer* vb = (pipe::VertexBuffer*)child->Data;
								glDeleteVertexArrays(1, &vb->VAO);
							} 

							FreeData(child->Data, child->Type);
							child->Data = nullptr;
							delete child;
							data->Items.erase(data->Items.begin() + j);
							break;
						}
					}
				}
				// TODO: clean this up and free some space
				else if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = (pipe::PluginItemData*)m_items[i]->Data;
					for (int j = 0; j < data->Items.size(); j++) {
						if (strcmp(data->Items[j]->Name, name) == 0) {
							ed::PipelineItem* child = data->Items[j];

							if (child->Type == PipelineItem::ItemType::Geometry) {
								pipe::GeometryItem* geo = (pipe::GeometryItem*)child->Data;
								glDeleteVertexArrays(1, &geo->VAO);
								glDeleteVertexArrays(1, &geo->VBO);
							} else if (child->Type == PipelineItem::ItemType::PluginItem) {
								pipe::PluginItemData* pdata = (pipe::PluginItemData*)child->Data;
								pdata->Owner->PipelineItem_Remove(child->Name, pdata->Type, pdata->PluginData);
							} else if (child->Type == PipelineItem::ItemType::VertexBuffer) {
								pipe::VertexBuffer* vb = (pipe::VertexBuffer*)child->Data;
								glDeleteVertexArrays(1, &vb->VAO);
							} 

							FreeData(child->Data, child->Type);
							child->Data = nullptr;
							delete child;
							data->Items.erase(data->Items.begin() + j);
							break;
						}
					}
				}
			}
		}

		m_project->ModifyProject();
	}
	bool PipelineManager::Has(const char* name)
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
				} else if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = (pipe::PluginItemData*)m_items[i]->Data;
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
	void PipelineManager::FreeData(void* data, PipelineItem::ItemType type)
	{
		//TODO: make it type-safe.
		switch (type) {
		case PipelineItem::ItemType::Geometry:
			delete (pipe::GeometryItem*)data;
			break;
		case PipelineItem::ItemType::ShaderPass:
			delete (pipe::ShaderPass*)data;
			break;
		case PipelineItem::ItemType::RenderState:
			delete (pipe::RenderState*)data;
			break;
		case PipelineItem::ItemType::Model:
			delete (pipe::Model*)data;
			break;
		case PipelineItem::ItemType::VertexBuffer:
			delete (pipe::VertexBuffer*)data;
			break;
		case PipelineItem::ItemType::ComputePass:
			delete (pipe::ComputePass*)data;
			break;
		case PipelineItem::ItemType::AudioPass:
			delete (pipe::AudioPass*)data;
			break;
		case PipelineItem::ItemType::PluginItem:
			delete (pipe::PluginItemData*)data;
			break;
		}
		data = nullptr;
	}
}