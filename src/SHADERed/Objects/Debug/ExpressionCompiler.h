#pragma once
#include <ShaderExpressionParser/Parser.h>
#include <spvgentwo/Module.h>
#include <spvgentwo/Grammar.h>
#include <spvgentwo/Function.h>
#include <spvgentwo/Instruction.h>
#include <common/HeapAllocator.h>

namespace ed {
	class ExpressionCompiler {
	public:
		ExpressionCompiler();
		~ExpressionCompiler();

		int Compile(const std::string& expr, std::vector<unsigned int>& spv);

		std::vector<std::string> GetVariableList();

	private:
		spvgentwo::Instruction* m_visit(expr::Node* node);
		void m_convert(int op, spvgentwo::Instruction* a, spvgentwo::Instruction* b, spvgentwo::Instruction*& outA, spvgentwo::Instruction*& outB);
		spvgentwo::Instruction* m_simpleConvert(int type, spvgentwo::Instruction* inst);
		int m_getBaseType(int type);
		int m_getCompCount(int type);
		bool m_isVector(int type);
		spvgentwo::Instruction* m_constructVector(int baseType, int compCount, std::vector<spvgentwo::Instruction*>& comps);
		spvgentwo::Instruction* m_swizzle(spvgentwo::Instruction* vec, const char* field);

	private:
		spvgentwo::Function* m_func;
		spvgentwo::Module* m_module;

		std::unordered_map<std::string, spvgentwo::Instruction*> m_vars;
		std::unordered_map<std::string, spvgentwo::Instruction*> m_opLoads;

		bool m_error;
	};
}