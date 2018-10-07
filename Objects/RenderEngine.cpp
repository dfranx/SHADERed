#include "RenderEngine.h"
#include <MoonLight/Base/PixelShader.h>
#include <MoonLight/Base/ConstantBuffer.h>

namespace ed
{
	RenderEngine::RenderEngine(ml::Window* wnd, PipelineManager * pipeline) :
		m_pipeline(pipeline),
		m_wnd(wnd),
		m_lastSize(0, 0)
	{}
	void RenderEngine::Render(int width, int height)
	{
		// recreate render texture if size is changed
		if (m_lastSize.x != width || m_lastSize.y != height) {
			m_lastSize = DirectX::XMINT2(width, height);
			m_rt.Create(*m_wnd, m_lastSize, ml::Resource::ShaderResource, true);

			m_rtView.Create(*m_wnd, m_rt);
		}

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

		std::vector<ml::VertexShader> vertexShaders;
		std::vector<ml::PixelShader> pixelShaders;

		// update constant buffer
		DirectX::XMMATRIX wvp = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(45)) * DirectX::XMMatrixTranslation(0, 0, 10) *
			DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45), (float)width/height, 0.1f, 1000.0f);
		DirectX::XMFLOAT4X4 projMat;

		DirectX::XMStoreFloat4x4(&projMat, XMMatrixTranspose(wvp));

		ml::ConstantBuffer<DirectX::XMFLOAT4X4> cb;
		cb.Create(*m_wnd, &projMat, sizeof(DirectX::XMMATRIX), 0);
		cb.BindVS();

		// parse all items! TODOO: CACHE!!
		std::vector<ed::PipelineManager::Item>& items = m_pipeline->GetList();
		
		for (auto it : items) {
			// TODO: cache shaders, input layouts, etc..
			if (it.Type == ed::PipelineItem::ShaderFile) {
				ed::pipe::ShaderItem* data = reinterpret_cast<ed::pipe::ShaderItem*>(it.Data);

				if (data->Type == ed::pipe::ShaderItem::VertexShader) {
					vertexShaders.push_back(ml::VertexShader());
					ml::VertexShader* shader = &vertexShaders.at(vertexShaders.size() - 1);
					shader->InputSignature = &data->InputLayout;
					shader->LoadFromFile(*m_wnd, std::string(data->FilePath), "main");
					shader->Bind();
					m_wnd->SetInputLayout(data->InputLayout);
				}
				else if (data->Type == ed::pipe::ShaderItem::PixelShader) {
					pixelShaders.push_back(ml::PixelShader());
					ml::PixelShader* shader = &pixelShaders.at(pixelShaders.size() - 1);
					shader->LoadFromFile(*m_wnd, std::string(data->FilePath), "main");
					shader->Bind();
				}
			}
			else if (it.Type == ed::PipelineItem::Geometry) {
				ed::pipe::GeometryItem* data = reinterpret_cast<ed::pipe::GeometryItem*>(it.Data);
				m_wnd->SetTopology(data->Topology);
				data->Geometry.Draw();
			}
		}


		// restore real render target view
		m_wnd->Bind();
	}
}