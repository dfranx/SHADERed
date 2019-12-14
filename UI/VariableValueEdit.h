#pragma once
#include "../Objects/ShaderVariable.h"
#include "../InterfaceManager.h"

namespace ed
{
	class VariableValueEditUI
	{
	public:
		VariableValueEditUI(ed::InterfaceManager* data);

		inline void Open(ed::ShaderVariable* var) { m_var = var; }
		inline void Close() { m_var = nullptr; }
		inline ed::ShaderVariable* GetVariable() { return m_var; }

		bool Update();

	private:
		bool m_drawRegular();
		bool m_drawFunction();

		ed::InterfaceManager* m_data;
		ed::ShaderVariable* m_var;
	};
}