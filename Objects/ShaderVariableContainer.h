#pragma once
#include "ShaderVariable.h"
#include <MoonLight/Base/ConstantBuffer.h>
#include <vector>

namespace ed
{
	class ShaderVariableContainer
	{
	public:
		ShaderVariableContainer();
		~ShaderVariableContainer();

		inline void Add(ShaderVariable* var) { m_vars.push_back(var); }
		void AddCopy(ShaderVariable var);
		void Remove(const char* name);

		void UpdateBuffers(ml::Window* wnd);

		inline ml::ConstantBuffer<char>& GetSlot(int i) { return m_cb[i]; }
		inline void BindVS(int slot) { m_cb[slot].BindVS(slot); }
		inline bool IsSlotUsed(int slot) { assert(slot < CONSTANT_BUFFER_SLOTS); return m_cachedSize[slot] > 0; }
		inline std::vector<ShaderVariable*>& GetVariables() { return m_vars; }

	private:
		char* m_updateData();
		size_t m_getDataSize();

		char* m_dataBlock;
		size_t m_dataBlockSize;

		ml::ConstantBuffer<char> m_cb[CONSTANT_BUFFER_SLOTS];
		int m_cachedSize[CONSTANT_BUFFER_SLOTS];					// last cb size

		std::vector<ShaderVariable*> m_vars;
	};
}