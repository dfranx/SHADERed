#include "ShaderVariableContainer.h"
#include "FunctionVariableManager.h"
#include "SystemVariableManager.h"
#include <iostream>

namespace ed
{
	ShaderVariableContainer::ShaderVariableContainer()
	{
		for (int i = 0; i < CONSTANT_BUFFER_SLOTS; i++)
			m_cachedSize[i] = 0;

		m_dataBlock = nullptr;
		m_dataBlockSize = 0;
	}
	ShaderVariableContainer::~ShaderVariableContainer()
	{
		for (int i = 0; i < m_vars.size(); i++) {
			free(m_vars[i].Data);
			if (m_vars[i].Arguments != nullptr)
				free(m_vars[i].Arguments);
			m_vars[i].Arguments = nullptr;
		}
		for (int i = 0; i < CONSTANT_BUFFER_SLOTS; i++)
			m_cb[i].Release();
		free(m_dataBlock);
	}
	void ShaderVariableContainer::UpdateBuffers(ml::Window* wnd)
	{
		int usedSize[CONSTANT_BUFFER_SLOTS] = { 0 };
		for (auto var : m_vars)
			usedSize[var.Slot] += ed::ShaderVariable::GetSize(var.GetType());

		char* data = m_updateData();
		
		for (int i = 0; i < CONSTANT_BUFFER_SLOTS; i++) {
			if (usedSize[i] == 0) {
				m_cb[i].Release();
				m_cachedSize[i] = 0;
			} else {
				if (usedSize[i] != m_cachedSize[i]) {
					m_cb[i].Create(*wnd, data, (((usedSize[i] / 16) + 1) * 16), ml::Resource::Dynamic | ml::Resource::CPUWrite);
					m_cachedSize[i] = usedSize[i];
				} else {
					D3D11_MAPPED_SUBRESOURCE sub;
					m_cb[i].Map(sub, ml::IResource::MapType::WriteDiscard);
					memcpy(sub.pData, data, usedSize[i]);
					m_cb[i].Unmap();
				}

				// skip the part that we already updated
				data += usedSize[i];
			}
		}
	}
	char* ShaderVariableContainer::m_updateData()
	{
		if (m_dataBlockSize != m_getDataSize()) {
			m_dataBlockSize = m_getDataSize();
			m_dataBlock = (char*)realloc(m_dataBlock, m_dataBlockSize);
		}

		// get the list of all slots
		std::vector<int> slots;
		for (int i = 0; i < m_vars.size(); i++) {
			auto cnt = std::count_if(slots.begin(), slots.end(), [=](auto a) {
				return a == m_vars[i].Slot;
			});

			if (cnt == 0)
				slots.push_back(m_vars[i].Slot);
		}
		std::sort(slots.begin(), slots.end());

		char* data = m_dataBlock;
		while (slots.size() != 0) {
			for (int i = 0; i < m_vars.size(); i++) {
				if (m_vars[i].Slot != slots[0])
					continue;
				
				// update the variable (only if needed)
				SystemVariableManager::Instance().Update(m_vars[i]);
				FunctionVariableManager::Update(m_vars[i]);

				int curSize = ShaderVariable::GetSize(m_vars[i].GetType());
				memcpy(data, m_vars[i].Data, curSize);
				data += curSize;
			}
			slots.erase(slots.begin() + 0);
		}

		return m_dataBlock;
	}

	size_t ShaderVariableContainer::m_getDataSize()
	{
		size_t ret = 0;
		for (auto var : m_vars) ret += ShaderVariable::GetSize(var.GetType());
		return ret;
	}
}