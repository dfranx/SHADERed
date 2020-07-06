#include <SHADERed/Objects/SPIRVParser.h>
#include <spirv/unified1/spirv.hpp>
#include <unordered_map>
#include <functional>

typedef unsigned int spv_word;

namespace ed {
	std::string spvReadString(const unsigned int* data, int length, int& i)
	{
		std::string ret(length * 4, 0);

		for (int j = 0; j < length; j++, i++) {
			ret[j*4+0] = (data[i] & 0x000000FF);
			ret[j*4+1] = (data[i] & 0x0000FF00) >> 8;
			ret[j*4+2] = (data[i] & 0x00FF0000) >> 16;
			ret[j*4+3] = (data[i] & 0xFF000000) >> 24;
		}

		return ret;
	}

	void SPIRVParser::Parse(const std::vector<unsigned int>& ir)
	{
		Functions.clear();
		UserTypes.clear();
		Uniforms.clear();
		Globals.clear();

		ArithmeticInstCount = 0;
		BitInstCount = 0;
		LogicalInstCount = 0;
		TextureInstCount = 0;
		DerivativeInstCount = 0;
		ControlFlowInstCount = 0;

		std::string curFunc = "";
		int lastOpLine = -1;

		std::unordered_map<spv_word, std::string> names;
		std::unordered_map<spv_word, spv_word> pointers;
		std::unordered_map<spv_word, std::pair<ValueType, int>> types;

		std::function<void(Variable&, spv_word)> fetchType = [&](Variable& var, spv_word type) {
			spv_word actualType = type;
			if (pointers.count(type))
				actualType = pointers[type];

			const std::pair<ValueType, int>& info = types[actualType];
			var.Type = info.first;
			
			if (var.Type == ValueType::Struct)
				var.TypeName = names[info.second];
			else if (var.Type == ValueType::Vector || var.Type == ValueType::Matrix) {
				var.TypeComponentCount = info.second & 0x00ffffff;
				var.BaseType = (ValueType)((info.second & 0xff000000) >> 24);
			} 
		};

		for (int i = 5; i < ir.size();) {
			int iStart = i;
			spv_word opcodeData = ir[i];

			spv_word wordCount = ((opcodeData & (~spv::OpCodeMask)) >> spv::WordCountShift) - 1;
			spv_word opcode = (opcodeData & spv::OpCodeMask);

			switch (opcode) {
			case spv::OpName: {
				spv_word loc = ir[++i];
				spv_word stringLength = wordCount - 1;

				names[loc] = spvReadString(ir.data(), stringLength, ++i);
			} break;
			case spv::OpLine: {
				++i; // skip file
				lastOpLine = ir[++i];

				if (!curFunc.empty() && Functions[curFunc].LineStart == -1)
					Functions[curFunc].LineStart = lastOpLine;
			} break;
			case spv::OpTypeStruct: {
				spv_word loc = ir[++i];

				spv_word memCount = wordCount - 1;
				if (UserTypes.count(names[loc]) == 0) {
					std::vector<Variable> mems(memCount);
					for (spv_word j = 0; j < memCount; j++) {
						spv_word type = ir[++i];
						fetchType(mems[j], type);
					}

					UserTypes.insert(std::make_pair(names[loc], mems));
				} else {
					auto& typeInfo = UserTypes[names[loc]];
					for (spv_word j = 0; j < memCount && j < typeInfo.size(); j++) {
						spv_word type = ir[++i];
						fetchType(typeInfo[j], type);
					}
				}

				types[loc] = std::make_pair(ValueType::Struct, loc);
			} break;
			case spv::OpMemberName: {
				spv_word owner = ir[++i];
				spv_word index = ir[++i]; // index

				spv_word stringLength = wordCount - 2;

				auto& typeInfo = UserTypes[names[owner]];

				if (index < typeInfo.size())
					typeInfo[index].Name = spvReadString(ir.data(), stringLength, ++i);
				else {
					typeInfo.resize(index + 1);
					typeInfo[index].Name = spvReadString(ir.data(), stringLength, ++i);
				}
			} break;
			case spv::OpFunction: {
				++i; // skip type
				spv_word loc = ir[++i];

				curFunc = names[loc];
				size_t args = curFunc.find_first_of('(');
				if (args != std::string::npos)
					curFunc = curFunc.substr(0, args);

				Functions[curFunc].LineStart = -1;
			} break;
			case spv::OpFunctionEnd: {
				Functions[curFunc].LineEnd = lastOpLine;
				lastOpLine = -1;
				curFunc = "";
			} break;
			case spv::OpVariable: {
				spv_word type = ir[++i]; // skip type
				spv_word loc = ir[++i];

				std::string varName = names[loc];

				if (curFunc.empty()) {
					spv::StorageClass sType = (spv::StorageClass)ir[++i];
					if (sType == spv::StorageClassUniform || sType == spv::StorageClassUniformConstant) {
						Variable uni;
						uni.Name = varName;
						fetchType(uni, type);

						if (uni.Name.size() == 0 || uni.Name[0] == 0) {
							if (UserTypes.count(uni.TypeName) > 0) {
								const std::vector<Variable>& mems = UserTypes[uni.TypeName];
								for (const auto& mem : mems)
									Uniforms.push_back(mem);
							}
						} else
							Uniforms.push_back(uni);
					} else if (varName.size() > 0 && varName[0] != 0)
						Globals.push_back(varName);
				} else
					Functions[curFunc].Locals.push_back(varName);
			} break;
			case spv::OpFunctionParameter: {
				++i; // skip type
				spv_word loc = ir[++i];

				std::string varName = names[loc];
				Functions[curFunc].Arguments.push_back(varName);
			} break;
			case spv::OpTypePointer: {
				spv_word loc = ir[++i];
				++i; // skip storage class
				spv_word type = ir[++i];

				pointers[loc] = type;
			} break;
			case spv::OpTypeBool: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Bool, 0);
			} break;
			case spv::OpTypeInt: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Int, 0);
			} break;
			case spv::OpTypeFloat: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Float, 0);
			} break;
			case spv::OpTypeVector: {
				spv_word loc = ir[++i];
				spv_word comp = ir[++i];
				spv_word compcount = ir[++i];

				spv_word val = (compcount & 0x00FFFFFF) | (((spv_word)types[comp].first) << 24);

				types[loc] = std::make_pair(ValueType::Vector, val);
			} break;
			case spv::OpTypeMatrix: {
				spv_word loc = ir[++i];
				spv_word comp = ir[++i];
				spv_word compcount = ir[++i];

				spv_word val = (compcount & 0x00FFFFFF) | (types[comp].second & 0xFF000000);

				types[loc] = std::make_pair(ValueType::Matrix, val);
			} break;

			case spv::OpSNegate: case spv::OpFNegate:
			case spv::OpIAdd: case spv::OpFAdd:
			case spv::OpISub: case spv::OpFSub:
			case spv::OpIMul: case spv::OpFMul:
			case spv::OpUDiv: case spv::OpSDiv:
			case spv::OpFDiv: case spv::OpUMod:
			case spv::OpSRem: case spv::OpSMod:
			case spv::OpFRem: case spv::OpFMod:
			case spv::OpVectorTimesScalar:
			case spv::OpMatrixTimesScalar:
			case spv::OpVectorTimesMatrix:
			case spv::OpMatrixTimesVector:
			case spv::OpMatrixTimesMatrix:
			case spv::OpOuterProduct:
			case spv::OpDot:
			case spv::OpIAddCarry:
			case spv::OpISubBorrow:
			case spv::OpUMulExtended:
			case spv::OpSMulExtended:
				ArithmeticInstCount++;
				break;

				
			case spv::OpShiftRightLogical:
			case spv::OpShiftRightArithmetic:
			case spv::OpShiftLeftLogical:
			case spv::OpBitwiseOr:
			case spv::OpBitwiseXor:
			case spv::OpBitwiseAnd:
			case spv::OpNot:
			case spv::OpBitFieldInsert:
			case spv::OpBitFieldSExtract:
			case spv::OpBitFieldUExtract:
			case spv::OpBitReverse:
			case spv::OpBitCount:
				BitInstCount++;
				break;

			case spv::OpAny: case spv::OpAll:
			case spv::OpIsNan: case spv::OpIsInf:
			case spv::OpIsFinite: case spv::OpIsNormal:
			case spv::OpSignBitSet: case spv::OpLessOrGreater:
			case spv::OpOrdered: case spv::OpUnordered:
			case spv::OpLogicalEqual: case spv::OpLogicalNotEqual:
			case spv::OpLogicalOr: case spv::OpLogicalAnd:
			case spv::OpLogicalNot: case spv::OpSelect:
			case spv::OpIEqual: case spv::OpINotEqual:
			case spv::OpUGreaterThan: case spv::OpSGreaterThan:
			case spv::OpUGreaterThanEqual: case spv::OpSGreaterThanEqual:
			case spv::OpULessThan: case spv::OpSLessThan:
			case spv::OpULessThanEqual: case spv::OpSLessThanEqual:
			case spv::OpFOrdEqual: case spv::OpFUnordEqual:
			case spv::OpFOrdNotEqual: case spv::OpFUnordNotEqual:
			case spv::OpFOrdLessThan: case spv::OpFUnordLessThan:
			case spv::OpFOrdGreaterThan: case spv::OpFUnordGreaterThan:
			case spv::OpFOrdLessThanEqual: case spv::OpFUnordLessThanEqual:
			case spv::OpFOrdGreaterThanEqual: case spv::OpFUnordGreaterThanEqual:
				LogicalInstCount++;
				break;

			case spv::OpImageSampleImplicitLod:
			case spv::OpImageSampleExplicitLod:
			case spv::OpImageSampleDrefImplicitLod:
			case spv::OpImageSampleDrefExplicitLod:
			case spv::OpImageSampleProjImplicitLod:
			case spv::OpImageSampleProjExplicitLod:
			case spv::OpImageSampleProjDrefImplicitLod:
			case spv::OpImageSampleProjDrefExplicitLod:
			case spv::OpImageFetch: case spv::OpImageGather:
			case spv::OpImageDrefGather: case spv::OpImageRead:
			case spv::OpImageWrite:
				TextureInstCount++;
				break;

			case spv::OpDPdx:
			case spv::OpDPdy:
			case spv::OpFwidth:
			case spv::OpDPdxFine:
			case spv::OpDPdyFine:
			case spv::OpFwidthFine:
			case spv::OpDPdxCoarse:
			case spv::OpDPdyCoarse:
			case spv::OpFwidthCoarse:
				DerivativeInstCount++;
				break;

			case spv::OpPhi:
			case spv::OpLoopMerge:
			case spv::OpSelectionMerge:
			case spv::OpLabel:
			case spv::OpBranch:
			case spv::OpBranchConditional:
			case spv::OpSwitch:
			case spv::OpKill:
			case spv::OpReturn:
			case spv::OpReturnValue:
				ControlFlowInstCount++;
				break;
			}

			i = iStart + wordCount + 1;
		}
	}
}