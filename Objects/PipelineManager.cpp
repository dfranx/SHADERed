#include "PipelineManager.h"
#include <MoonLight/Base/Topology.h>
#include <MoonLight/Base/GeometryFactory.h>

namespace ed
{
	PipelineManager::PipelineManager(ml::Window* wnd)
	{
		m_wnd = wnd;
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
	void PipelineManager::New()
	{
		Clear();

		Add("TriangleList", ed::PipelineItem::PrimitiveTopology, new ed::pipe::PrimitiveTopology());
		ed::pipe::PrimitiveTopology* topology = reinterpret_cast<ed::pipe::PrimitiveTopology*>(Get("TriangleList").Data);
		topology->Type = ml::Topology::TriangleList;
		
		Add("Vertex Shader", PipelineItem::ShaderFile, new ed::pipe::ShaderItem());
		ed::pipe::ShaderItem* vertexShader = reinterpret_cast<ed::pipe::ShaderItem*>(Get("Vertex Shader").Data);
		vertexShader->Type = ed::pipe::ShaderItem::VertexShader;
		memcpy(vertexShader->FilePath, "vertex.hlsl\0", strlen("vertex.hlsl\0"));

		Add("Pixel Shader", PipelineItem::ShaderFile, new ed::pipe::ShaderItem());
		ed::pipe::ShaderItem* pixelShader = reinterpret_cast<ed::pipe::ShaderItem*>(Get("Pixel Shader").Data);
		pixelShader->Type = ed::pipe::ShaderItem::PixelShader;
		memcpy(pixelShader->FilePath, "pixel.hlsl\0", strlen("pixel.hlsl\0"));

		Add("Vertex Layout", ed::PipelineItem::InputLayout, new ed::pipe::InputLayout());
		ed::pipe::InputLayout* inputLayout = reinterpret_cast<ed::pipe::InputLayout*>(Get("Vertex Layout").Data);
		inputLayout->Layout.Add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		inputLayout->Shader = vertexShader;

		Add("Box", PipelineItem::Geometry, new ed::pipe::GeometryItem());
		ed::pipe::GeometryItem* boxGeometry = reinterpret_cast<ed::pipe::GeometryItem*>(Get("Box").Data);
		boxGeometry->Geometry = ml::GeometryFactory::CreateCube(1,1,1, *m_wnd);
		boxGeometry->Position = DirectX::XMFLOAT3(0, 0, 0);
		boxGeometry->Rotation = DirectX::XMFLOAT3(0, 0, 0);
		boxGeometry->Scale = DirectX::XMFLOAT3(1, 1, 1);
	}
}