#include "RenderEngine.h"
#include "PipelineManager.h"
#include "SystemVariableManager.h"
#include <MoonLight/Base/PixelShader.h>
#include <MoonLight/Base/ConstantBuffer.h>

namespace ed
{
	RenderEngine::RenderEngine(ml::Window* wnd, PipelineManager * pipeline, ProjectParser* project, MessageStack* msgs) :
		m_pipeline(pipeline),
		m_project(project),
		m_msgs(msgs),
		m_wnd(wnd),
		m_lastSize(0, 0)
	{}
	RenderEngine::~RenderEngine()
	{
		for (int i = 0; i < m_d3dItems.size(); i++)
			delete m_d3dItems[i];
		for (int i = 0; i < m_cachedItems.size(); i++)
			free(m_cachedItems[i].Data);
	}
	void RenderEngine::Render(int width, int height)
	{
		// recreate render texture if size is changed
		if (m_lastSize.x != width || m_lastSize.y != height) {
			m_lastSize = DirectX::XMINT2(width, height);
			m_rt.Create(*m_wnd, m_lastSize, ml::Resource::ShaderResource, true);

			m_rtView.Create(*m_wnd, m_rt);
		}

		// update system values
		SystemVariableManager::Instance().SetViewportSize(width, height);

		// cache elements
		m_cache();

		// bind and reset render texture
		m_rt.Bind();
		m_rt.Clear();
		m_rt.ClearDepthStencil(1.0f, 0);

		// set viewport and cache old viewport
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = viewport.TopLeftY = 0;
		viewport.Height = height;
		viewport.Width = width;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_wnd->GetDeviceContext()->RSSetViewports(1, &viewport);

		int cacheCounter = 0;
		for (auto it : m_items) {
			if (m_isCached(it)) {
				if (it.Type == ed::PipelineItem::ShaderFile) {
					ed::pipe::ShaderItem* data = reinterpret_cast<ed::pipe::ShaderItem*>(it.Data);

					if (data->Type == ed::pipe::ShaderItem::VertexShader)
						m_wnd->SetInputLayout(data->InputLayout);

					data->Variables.UpdateBuffers(m_wnd);

					for (int i = 0; i < CONSTANT_BUFFER_SLOTS; i++) {
						if (data->Variables.IsSlotUsed(i)) {
							if (data->Type == ed::pipe::ShaderItem::VertexShader)
								data->Variables.GetSlot(i).BindVS(i);
							else if (data->Type == ed::pipe::ShaderItem::PixelShader)
								data->Variables.GetSlot(i).BindPS(i);
						}
					}

					ml::Shader* shader = reinterpret_cast<ml::Shader*>(m_d3dItems[cacheCounter]);
					shader->Bind();
				}
				cacheCounter++;
			} else {
				if (it.Type == ed::PipelineItem::Geometry) {
					ed::pipe::GeometryItem* data = reinterpret_cast<ed::pipe::GeometryItem*>(it.Data);

					SystemVariableManager::Instance().SetGeometryTransform(*data);

					m_wnd->SetTopology(data->Topology);
					data->Geometry.Draw();
				}
			}
		}
		
		// restore real render target view
		m_wnd->Bind();
	}
	void RenderEngine::Recompile(const char * name)
	{
		int d3dCounter = 0;
		for (auto item : m_items) {
			if (strcmp(item.Name, name) == 0) {
				ed::pipe::ShaderItem* shader = (ed::pipe::ShaderItem*)item.Data;
				
				std::string content = m_project->LoadProjectFile(shader->FilePath);
				bool compiled = ((ml::Shader*)m_d3dItems[d3dCounter])->LoadFromMemory(*m_wnd, content.c_str(), content.size(), shader->Entry);

				if (!compiled)
					m_msgs->Add(MessageStack::Type::Error, item.Name, "Failed to compile the shader");
				else
					m_msgs->ClearGroup(item.Name);
			}
			if (m_isCached(item))
				d3dCounter++;
		}
	}
	void RenderEngine::FlushCache()
	{
		for (int i = 0; i < m_d3dItems.size(); i++)
			delete m_d3dItems[i];
		for (int i = 0; i < m_cachedItems.size(); i++)
			free(m_cachedItems[i].Data);
		m_d3dItems.clear();
		m_cachedItems.clear();
		m_items.clear();
	}
	void RenderEngine::m_cache()
	{
		// check for any changes
		std::vector<ed::PipelineManager::Item*>& items = m_pipeline->GetList();

		// check if no major changes were made, if so dont cache for another 0.25s
		if (m_items.size() == items.size()) {
			if (m_cacheTimer.GetElapsedTime() > 0.5f)
				m_cacheTimer.Restart();
			else return;
		}

		int d3dCounter = 0;

		// check if some item was added
		for (int i = 0; i < items.size(); i++) {
			bool found = false;
			for (int j = 0; j < m_items.size(); j++)
				if (items[i]->Data == m_items[j].Data)
					found = true;

			if (!found) {
				m_items.insert(m_items.begin() + i, *items[i]);
				
				/*
					ITEM CACHING
				*/
				if (m_isCached(*items[i])) {
					if (items[i]->Type == ed::PipelineItem::ShaderFile) {
						ed::pipe::ShaderItem* data = reinterpret_cast<ed::pipe::ShaderItem*>(items[i]->Data);

						ml::Shader* shader = nullptr;
						if (data->Type == ed::pipe::ShaderItem::PixelShader)
							shader = new ml::PixelShader();
						else if (data->Type == ed::pipe::ShaderItem::VertexShader) {
							shader = new ml::VertexShader();

							// bind the input layout
							data->InputLayout.Reset();
							reinterpret_cast<ml::VertexShader*>(shader)->InputSignature = &data->InputLayout;
						}

						std::string content = m_project->LoadProjectFile(data->FilePath);
						bool compiled = shader->LoadFromMemory(*m_wnd, content.c_str(), content.size(), data->Entry);

						if (!compiled)
							m_msgs->Add(MessageStack::Type::Error, items[i]->Name, "Failed to compile the shader");
						else
							m_msgs->ClearGroup(items[i]->Name);

						m_d3dItems.insert(m_d3dItems.begin() + d3dCounter, shader);

						// copy actual item contents
						m_cachedItems.insert(m_cachedItems.begin() + d3dCounter, *items[i]);
						m_cachedItems[d3dCounter].Data = malloc(sizeof(ed::pipe::ShaderItem));
						memcpy(m_cachedItems[d3dCounter].Data, data, sizeof(ed::pipe::ShaderItem));
					}
				}
			}

			if (m_isCached(*items[i]))
				d3dCounter++;
		}

		d3dCounter = 0;

		// check if some item was removed
		for (int i = 0; i < m_items.size(); i++) {
			bool found = false;
			for (int j = 0; j < items.size(); j++)
				if (items[j]->Data == m_items[i].Data)
					found = true;

			bool isCached = m_isCached(m_items[i]);

			if (!found) {
				if (isCached) {
					delete m_d3dItems[d3dCounter];
					m_d3dItems.erase(m_d3dItems.begin() + d3dCounter);

					free(m_cachedItems[d3dCounter].Data);
					m_cachedItems.erase(m_cachedItems.begin() + d3dCounter);

					d3dCounter--;
				}
				m_items.erase(m_items.begin() + i);
			}

			if (isCached)
				d3dCounter++;
		}

		d3dCounter = 0;

		// check if the order of the items changed
		if (m_items.size() == items.size()) {
			for (int i = 0; i < m_items.size(); i++) {
				// two items at the same position dont match
				if (items[i]->Data != m_items[i].Data) {
					int tempD3DCounter = 0;

					// find the real position from original list
					for (int j = 0; j < items.size(); j++) {
						// we found the original position so move the item
						if (items[j]->Data == m_items[i].Data) {
							int dest = j > i ? (j - 1) : j;
							m_items.erase(m_items.begin() + i, m_items.begin() + i + 1);
							m_items.insert(m_items.begin() + dest, *items[j]);

							if (m_isCached(m_items[i])) {
								int d3dDest = tempD3DCounter > d3dCounter ? (tempD3DCounter - 1) : tempD3DCounter;

								void* d3dCopy = m_d3dItems[d3dCounter];
								ed::PipelineManager::Item cachedItem = m_cachedItems[d3dCounter];

								m_d3dItems.erase(m_d3dItems.begin() + d3dCounter);
								m_d3dItems.insert(m_d3dItems.begin() + d3dDest, d3dCopy);

								m_cachedItems.erase(m_cachedItems.begin() + d3dCounter);
								m_cachedItems.insert(m_cachedItems.begin() + d3dDest, cachedItem);

								if (tempD3DCounter > d3dCounter)
									d3dCounter--;
							}
						}

						if (m_isCached(m_items[j]))
							tempD3DCounter++;
					}
				}

				if (m_isCached(m_items[i]))
					d3dCounter++;
			}
		}

		d3dCounter = 0;

		// check if any of the cached items has changed some property
		for (int i = 0; i < m_items.size(); i++) {
			if (m_isCached(m_items[i])) {
				
				if (m_items[i].Type == ed::PipelineItem::ShaderFile) {
					ed::pipe::ShaderItem* current = reinterpret_cast<ed::pipe::ShaderItem*>(m_cachedItems[d3dCounter].Data);
					ed::pipe::ShaderItem* next = reinterpret_cast<ed::pipe::ShaderItem*>(m_items[i].Data);
					
					// basic checks
					if (strcmp(next->Entry, current->Entry) != 0 ||
						strcmp(next->FilePath, current->FilePath) != 0 ||
						next->InputLayout.GetInputElements().size() != current->InputLayout.GetInputElements().size() ||
						next->Type != current->Type)
					{
						ml::Shader* shader = nullptr;

						if (next->Type == ed::pipe::ShaderItem::PixelShader)
							shader = new ml::PixelShader();
						else if (next->Type == ed::pipe::ShaderItem::VertexShader) {
							shader = new ml::VertexShader();

							// rebind the input layout
							next->InputLayout.Reset();
							reinterpret_cast<ml::VertexShader*>(shader)->InputSignature = &next->InputLayout;
						}

						std::string content = m_project->LoadProjectFile(next->FilePath);
						bool valid = shader->LoadFromMemory(*m_wnd, content.c_str(), content.size(), next->Entry);

						if (!valid) {
							m_msgs->Add(MessageStack::Type::Error, m_items[i].Name, "Failed to compile the shader.");
							// TODO: outputUI->Print("Failed to compile the shader! Continuing to run the old shader. Rebuilding the input layout.");
							// rebuild the input layout
							content = m_project->LoadProjectFile(current->FilePath);
							reinterpret_cast<ml::VertexShader*>(m_d3dItems[d3dCounter])->LoadFromMemory(*m_wnd, content.c_str(), content.size(), current->Entry);
							delete shader;
						} else {
							m_msgs->ClearGroup(m_items[i].Name);

							delete m_d3dItems[d3dCounter];
							m_d3dItems[d3dCounter] = shader;
						}

						memcpy(current, next, sizeof(ed::pipe::ShaderItem));
					}
				}

				d3dCounter++;
			}
		}
	}
}