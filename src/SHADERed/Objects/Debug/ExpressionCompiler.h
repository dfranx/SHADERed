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

		void SetSPIRV(const std::vector<unsigned int>& spv);
		int Compile(const std::string& expr, const std::string& curFunction);
		void GetSPIRV(std::vector<unsigned int>& outSPV);

		std::vector<std::string> GetVariableList();

	private:
		spvgentwo::Instruction* m_visit(expr::Node* node);
		void m_convert(int op, spvgentwo::Instruction* a, spvgentwo::Instruction* b, spvgentwo::Instruction*& outA, spvgentwo::Instruction*& outB);
		spvgentwo::Instruction* m_simpleConvert(int type, spvgentwo::Instruction* inst);
		int m_getBaseType(int type);
		int m_getCompCount(int type);
		int m_getRowCount(int type);
		int m_getColumnCount(int type);
		bool m_isVector(int type);
		bool m_isMatrix(int type);
		spvgentwo::Instruction* m_constructVector(int baseType, int compCount, std::vector<spvgentwo::Instruction*>& comps);
		spvgentwo::Instruction* m_constructMatrix(int rows, int cols, std::vector<spvgentwo::Instruction*>& comps);
		spvgentwo::Instruction* m_swizzle(spvgentwo::Instruction* vec, const char* field);

	private:
		spvgentwo::HeapAllocator* m_allocator;
		spvgentwo::Grammar* m_grammar;
		spvgentwo::Module* m_module;

		spvgentwo::Function* m_func;

		std::unordered_map<std::string, spvgentwo::Instruction*> m_vars;
		std::unordered_map<std::string, spvgentwo::Instruction*> m_opLoads;

		bool m_error;
	};
}