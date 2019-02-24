#pragma once
#include "PipelineManager.h"
#include "ProjectParser.h"
#include "MessageStack.h"

#include <MoonLight/Base/ShaderResourceView.h>
#include <MoonLight/Base/RenderTexture.h>
#include <MoonLight/Base/GeometryShader.h>
#include <MoonLight/Base/VertexShader.h>
#include <MoonLight/Base/SamplerState.h>
#include <MoonLight/Base/PixelShader.h>
#include <MoonLight/Base/Timer.h>
#include <unordered_map>

namespace ed
{
	class ObjectManager;

	class RenderEngine
	{
	public:
		RenderEngine(ml::Window* wnd, PipelineManager* pipeline, ObjectManager* objects, ProjectParser* project, MessageStack* messages);
		~RenderEngine();

		void Render(int width, int height);
		void Recompile(const char* name);
		void Pick(float sx, float sy);

		void FlushCache();

		inline ml::ShaderResourceView& GetTexture() { return m_rtView; }

		DirectX::XMINT2 GetLastRenderSize() { return m_lastSize; }

	public:
		struct ItemVariableValue
		{
			ItemVariableValue(ed::ShaderVariable* var) { 
				Variable = var;
				OldValue = var->Data;
				NewValue = new ShaderVariable(var->GetType(), var->Name, var->System, var->Slot);
				NewValue->Function = var->Function;
			}
			PipelineItem* Item;
			ed::ShaderVariable* Variable;
			char* OldValue;
			ed::ShaderVariable* NewValue;
		};

		inline std::vector<ItemVariableValue>& GetItemVariableValues() { return m_itemValues; }
		inline void AddItemVariableValue(const ItemVariableValue& item) { m_itemValues.push_back(item); }
		inline void RemoveItemVariableValue(PipelineItem* item, ShaderVariable* var) {
			for (int i = 0; i < m_itemValues.size(); i++)
				if (m_itemValues[i].Item == item && m_itemValues[i].Variable == var) {
					m_itemValues.erase(m_itemValues.begin() + i);
					return;
				}
		}
		inline void RemoveItemVariableValues(PipelineItem* item) {
			for (int i = 0; i < m_itemValues.size(); i++)
				if (m_itemValues[i].Item == item) {
					m_itemValues.erase(m_itemValues.begin() + i);
					i--;
				}
		}

	private:
		PipelineManager* m_pipeline;
		ObjectManager* m_objects;
		ProjectParser* m_project;
		MessageStack* m_msgs;
		ml::Window* m_wnd;

		/* picking */
		bool m_pickAwaiting;
		float m_pickDist;
		DirectX::XMVECTOR m_pickOrigin; // view space
		DirectX::XMVECTOR m_pickDir;
		PipelineItem* m_pick;
		void m_pickItem(PipelineItem* item);

		DirectX::XMINT2 m_lastSize;
		ml::RenderTexture m_rt;

		ml::SamplerState m_sampler;

		ml::ShaderResourceView m_rtView;

		std::vector<ed::PipelineItem*> m_items;
		std::vector<ml::VertexShader*> m_vs;
		std::vector<ml::PixelShader*> m_ps;
		std::vector<ml::GeometryShader*> m_gs;

		std::vector<ItemVariableValue> m_itemValues; // list of all values to apply once we start rendering 

		ml::Timer m_cacheTimer;
		void m_cache();
	};
}