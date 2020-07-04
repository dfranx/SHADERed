#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace ed {
	class SPIRVParser {
	public:
		void Parse(const std::vector<unsigned int>& spv);

		struct Function {
			int LineStart;
			int LineEnd;
			std::vector<std::string> Arguments;
			std::vector<std::string> Locals;
		};
		enum class ValueType
		{
			Void,
			Bool,
			Int,
			Float,
			Vector,
			Matrix,
			Struct,
			Unknown
		};
		struct Variable {
			std::string Name;
			ValueType Type, BaseType;
			int TypeComponentCount;
			std::string TypeName;
		};
		std::unordered_map<std::string, Function> Functions;
		std::unordered_map<std::string, std::vector<Variable>> UserTypes;
		std::vector<Variable> Uniforms;
		std::vector<std::string> Globals;

		int ArithmeticInstCount;
		int BitInstCount;
		int LogicalInstCount;
		int TextureInstCount;
		int DerivativeInstCount;
		int ControlFlowInstCount;
	};
}