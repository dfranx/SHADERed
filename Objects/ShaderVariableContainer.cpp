#include "ShaderVariableContainer.h"
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
		for (int i = 0; i < m_vars.size(); i++)
			free(m_vars[i].Data);
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
		memset(data, 0, m_getDataSize());
		
		for (int i = 0; i < CONSTANT_BUFFER_SLOTS; i++) {
			if (usedSize[i] == 0)
				m_cb[i].Release();
			else {
				if (usedSize[i] > m_cachedSize[i]) {
					std::cout << "size: " << m_getDataSize() << std::endl;
					for (int i = 0; i < m_getDataSize(); i++) {
						std::cout << (int)(data[i]) << ' ';
					}
					std::cout << std::endl << std::endl;

					char* c = (char*)calloc(64, 64);

					m_cb[i].Create(*wnd, &c, (((usedSize[i] / 16) + 1) * 16));
					m_cachedSize[i] = usedSize[i];
				}// else m_cb[i].Update(&data);

				// skip certain part
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


				if (m_vars[i].System != ed::SystemShaderVariable::None) {
					// we are using some system value so now is the right time to update its value
					// converting is a must when using matrices

					DirectX::XMFLOAT4X4 rawData;

					switch (m_vars[i].System) {
						case ed::SystemShaderVariable::View:
							DirectX::XMStoreFloat4x4(&rawData, SystemVariableManager::Instance().GetViewMatrix());
							memcpy(m_vars[i].Data, &rawData, sizeof(DirectX::XMFLOAT4X4));
							break;
						case ed::SystemShaderVariable::Projection:
							DirectX::XMStoreFloat4x4(&rawData, SystemVariableManager::Instance().GetProjectionMatrix());
							memcpy(m_vars[i].Data, &rawData, sizeof(DirectX::XMFLOAT4X4));
							break;
						case ed::SystemShaderVariable::ViewProjection:
							DirectX::XMStoreFloat4x4(&rawData, DirectX::XMMatrixTranspose(SystemVariableManager::Instance().GetViewProjectionMatrix()));
							memcpy(m_vars[i].Data, &rawData, sizeof(DirectX::XMFLOAT4X4));
							break;
						case ed::SystemShaderVariable::ViewportSize:
							DirectX::XMFLOAT2 raw = SystemVariableManager::Instance().GetViewportSize();
							memcpy(m_vars[i].Data, &raw, sizeof(DirectX::XMFLOAT2));
							break;
					}
				}

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