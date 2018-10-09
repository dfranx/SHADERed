#include "RenderEngine.h"
#include "SystemVariableManager.h"
#include <MoonLight/Base/PixelShader.h>
#include <MoonLight/Base/ConstantBuffer.h>

namespace ed
{
	RenderEngine::RenderEngine(ml::Window* wnd, PipelineManager * pipeline) :
		m_pipeline(pipeline),
		m_wnd(wnd),
		m_lastSize(0, 0)
	{}
	RenderEngine::~RenderEngine()
	{
		for (int i = 0; i < m_d3dItems.size(); i++)
			delete m_d3dItems[i];
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

		// update constant buffer
		DirectX::XMMATRIX wvp = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(45)) * DirectX::XMMatrixTranslation(0, 0, 10) *
			DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45), (float)width/height, 0.1f, 1000.0f);
		DirectX::XMFLOAT4X4 projMat;

		DirectX::XMStoreFloat4x4(&projMat, XMMatrixTranspose(wvp));

		ml::ConstantBuffer<DirectX::XMFLOAT4X4> cb;
		cb.Create(*m_wnd, &projMat, sizeof(DirectX::XMMATRIX), 0);
		cb.BindVS();

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
								data->Variables.BindVS(i);
							//else if (data->Type == ed::pipe::ShaderItem::PixelShader)
							//	data->Variables.GetSlot(i)->BindPS(i);
						}
					}
					//cb.BindVS();

					ml::Shader* shader = reinterpret_cast<ml::Shader*>(m_d3dItems[cacheCounter]);
					shader->Bind();
				}
				cacheCounter++;
			} else {
				if (it.Type == ed::PipelineItem::Geometry) {
					ed::pipe::GeometryItem* data = reinterpret_cast<ed::pipe::GeometryItem*>(it.Data);
					m_wnd->SetTopology(data->Topology);
					data->Geometry.Draw();
				}
			}
		}
		
		// restore real render target view
		m_wnd->Bind();
	}
	void RenderEngine::m_cache()
	{
		// check for any changes
		std::vector<ed::PipelineManager::Item>& items = m_pipeline->GetList();

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
				if (items[i].Data == m_items[j].Data)
					found = true;

			if (!found) {
				m_items.insert(m_items.begin() + i, items[i]);
				
				/*
					ITEM CACHING
				*/
				if (m_isCached(items[i])) {
					if (items[i].Type == ed::PipelineItem::ShaderFile) {
						ed::pipe::ShaderItem* data = reinterpret_cast<ed::pipe::ShaderItem*>(items[i].Data);

						ml::Shader* shader = nullptr;
						if (data->Type == ed::pipe::ShaderItem::PixelShader)
							shader = new ml::PixelShader();
						else if (data->Type == ed::pipe::ShaderItem::VertexShader) {
							shader = new ml::VertexShader();

							// bind the input layout
							data->InputLayout.Reset();
							reinterpret_cast<ml::VertexShader*>(shader)->InputSignature = &data->InputLayout;
						}
						
						shader->LoadFromFile(*m_wnd, data->FilePath, data->Entry);

						m_d3dItems.insert(m_d3dItems.begin() + d3dCounter, shader);
					}
				}
			}

			if (m_isCached(items[i]))
				d3dCounter++;
		}

		d3dCounter = 0;

		// check if some item was removed
		for (int i = 0; i < m_items.size(); i++) {
			bool found = false;
			for (int j = 0; j < items.size(); j++)
				if (items[j].Data == m_items[i].Data)
					found = true;

			if (!found) {
				m_items.erase(m_items.begin() + i);
				if (m_isCached(items[i])) {
					delete m_d3dItems[d3dCounter];
					m_d3dItems.erase(m_d3dItems.begin() + d3dCounter);
					d3dCounter--;
				}
			}

			if (m_isCached(items[i]))
				d3dCounter++;
		}

		d3dCounter = 0;

		// check if the order of the items changed
		if (m_items.size() == items.size()) {
			for (int i = 0; i < m_items.size(); i++) {
				// two items at the same position dont match
				if (items[i].Data != m_items[i].Data) {
					int tempD3DCounter = 0;

					// find the real position from original list
					for (int j = 0; j < items.size(); j++) {
						// we found the original position so move the item
						if (items[j].Data == m_items[i].Data) {
							int dest = j > i ? (j - 1) : j;
							m_items.erase(m_items.begin() + i, m_items.begin() + i + 1);
							m_items.insert(m_items.begin() + dest, items[j]);

							int d3dDest = tempD3DCounter > d3dCounter ? (tempD3DCounter - 1) : tempD3DCounter;
							void* d3dCopy = m_d3dItems[d3dCounter];
							m_d3dItems.erase(m_d3dItems.begin() + d3dCounter);
							m_d3dItems.insert(m_d3dItems.begin() + d3dDest, d3dCopy);
						}

						if (m_isCached(m_items[j]))
							tempD3DCounter++;
					}
				}

				if (m_isCached(m_items[i]))
					d3dCounter++;
			}
		}

		// check if any of the cached items has changed some property
		for (int i = 0; i < m_cachedItems.size(); i++) {
			if (m_isCached(m_cachedItems[i])) {
				
				if (m_cachedItems[i].Type == ed::PipelineItem::ShaderFile) {
					ed::pipe::ShaderItem* current = reinterpret_cast<ed::pipe::ShaderItem*>(m_cachedItems[i].Data);
					ed::pipe::ShaderItem* next = reinterpret_cast<ed::pipe::ShaderItem*>(m_items[i].Data);

					// some important property changed - requires recompilation
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

							// bind the input layout
							next->InputLayout.Reset();
							reinterpret_cast<ml::VertexShader*>(shader)->InputSignature = &next->InputLayout;
						}

						bool valid = shader->LoadFromFile(*m_wnd, next->FilePath, next->Entry);
						if (!valid) {
							// TODO: outputUI->Print("Failed to compile the shader! Continuing to run the old shader.");
							delete shader;
						} else {
							delete m_d3dItems[d3dCounter];
							m_d3dItems[d3dCounter] = shader;
						}
					}
				}

				d3dCounter++;
			}
		}
		m_cachedItems = m_items;
	}
}