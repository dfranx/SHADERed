#include "PipelineManager.h"
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
	void PipelineManager::Add(const std::string & name, PipelineItem type, void * data)
	{
		m_items.push_back({ name, type, data });
	}
	void PipelineManager::Remove(const std::string & name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i].Name == name) {
				delete m_items[i].Data;
				m_items.erase(m_items.begin() + i);
				break;
			}
	}
	PipelineManager::Item& PipelineManager::Get(const std::string & name)
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i].Name == name)
				return m_items[i];
	}
	void PipelineManager::New()
	{
		Clear();

		Add("Vertex Shader", PipelineItem::ShaderFile, new ed::pipe::ShaderItem());
		ed::pipe::ShaderItem* vertexShader = reinterpret_cast<ed::pipe::ShaderItem*>(Get("Vertex Shader").Data);
		vertexShader->Type = ed::pipe::ShaderItem::VertexShader;
		memcpy(vertexShader->FilePath, "vertex.hlsl\0", strlen("vertex.hlsl\0"));

		Add("Pixel Shader", PipelineItem::ShaderFile, new ed::pipe::ShaderItem());
		ed::pipe::ShaderItem* pixelShader = reinterpret_cast<ed::pipe::ShaderItem*>(Get("Pixel Shader").Data);
		pixelShader->Type = ed::pipe::ShaderItem::PixelShader;
		memcpy(pixelShader->FilePath, "pixel.hlsl\0", strlen("pixel.hlsl\0"));

		Add("Box", PipelineItem::Geometry, new ed::pipe::GeometryItem());
		ed::pipe::GeometryItem* boxGeometry = reinterpret_cast<ed::pipe::GeometryItem*>(Get("Box").Data);
		boxGeometry->Geometry = ml::GeometryFactory::CreateCube(1,1,1, *m_wnd);
		boxGeometry->Position = DirectX::XMFLOAT3(0, 0, 0);
		boxGeometry->Rotation = DirectX::XMFLOAT3(0, 0, 0);
		boxGeometry->Scale = DirectX::XMFLOAT3(1, 1, 1);
	}
}