#pragma once
#include "../Objects/ShaderVariable.h"

namespace ed
{
	class VariableValueEditUI
	{
	public:
		VariableValueEditUI();

		inline void Open(ed::ShaderVariable* var) { m_var = var; }
		inline void Close() { m_var = nullptr; }
		inline ed::ShaderVariable* GetVariable() { return m_var; }

		void Update();

	private:
		void m_drawRegular();
		void m_drawFunction();

		ed::ShaderVariable* m_var;
	};
}