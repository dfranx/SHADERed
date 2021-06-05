#include <SHADERed/Objects/SPIRVParser.h>
#include <spvgentwo/Spv.h>
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

		for (size_t j = 0; j < ret.size(); j++) {
			if (ret[j] == 0) {
				ret.resize(j);
				break;
			}
		}

		return ret;
	}

	void SPIRVParser::Parse(const std::vector<unsigned int>& ir, bool trimFunctionNames)
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

		BarrierUsed = false;
		LocalSizeX = 1;
		LocalSizeY = 1;
		LocalSizeZ = 1;

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

			spv_word wordCount = ((opcodeData & (~spvgentwo::spv::OpCodeMask)) >> spvgentwo::spv::WordCountShift) - 1;
			spvgentwo::spv::Op opcode = (spvgentwo::spv::Op)(opcodeData & spvgentwo::spv::OpCodeMask);

			switch (opcode) {
			case spvgentwo::spv::Op::OpName: {
				spv_word loc = ir[++i];
				spv_word stringLength = wordCount - 1;

				names[loc] = spvReadString(ir.data(), stringLength, ++i);
			} break;
			case spvgentwo::spv::Op::OpLine: {
				++i; // skip file
				lastOpLine = ir[++i];

				if (!curFunc.empty() && Functions[curFunc].LineStart == -1)
					Functions[curFunc].LineStart = lastOpLine;
			} break;
			case spvgentwo::spv::Op::OpTypeStruct: {
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
			case spvgentwo::spv::Op::OpMemberName: {
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
			case spvgentwo::spv::Op::OpFunction: {
				spv_word type = ir[++i];
				spv_word loc = ir[++i];

				curFunc = names[loc];

				if (trimFunctionNames) {
					size_t args = curFunc.find_first_of('(');
					if (args != std::string::npos)
						curFunc = curFunc.substr(0, args);
				}

				fetchType(Functions[curFunc].ReturnType, type);
				Functions[curFunc].LineStart = -1;
			} break;
			case spvgentwo::spv::Op::OpFunctionEnd: {
				Functions[curFunc].LineEnd = lastOpLine;
				lastOpLine = -1;
				curFunc = "";
			} break;
			case spvgentwo::spv::Op::OpVariable: {
				spv_word type = ir[++i];
				spv_word loc = ir[++i];

				std::string varName = names[loc];

				if (curFunc.empty()) {
					spvgentwo::spv::StorageClass sType = (spvgentwo::spv::StorageClass)ir[++i];
					if (sType == spvgentwo::spv::StorageClass::Uniform || sType == spvgentwo::spv::StorageClass::UniformConstant) {
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
					} else if (varName.size() > 0 && varName[0] != 0) {
						Variable glob;
						glob.Name = varName;
						fetchType(glob, type);

						Globals.push_back(glob);
					}
				} else {
					Variable loc;
					loc.Name = varName;
					fetchType(loc, type);
					Functions[curFunc].Locals.push_back(loc);
				}
			} break;
			case spvgentwo::spv::Op::OpFunctionParameter: {
				spv_word type = ir[++i];
				spv_word loc = ir[++i];

				Variable arg;
				arg.Name = names[loc];
				fetchType(arg, type);
				Functions[curFunc].Arguments.push_back(arg);
			} break;
			case spvgentwo::spv::Op::OpTypePointer: {
				spv_word loc = ir[++i];
				++i; // skip storage class
				spv_word type = ir[++i];

				pointers[loc] = type;
			} break;
			case spvgentwo::spv::Op::OpTypeBool: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Bool, 0);
			} break;
			case spvgentwo::spv::Op::OpTypeInt: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Int, 0);
			} break;
			case spvgentwo::spv::Op::OpTypeFloat: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Float, 0);
			} break;
			case spvgentwo::spv::Op::OpTypeVector: {
				spv_word loc = ir[++i];
				spv_word comp = ir[++i];
				spv_word compcount = ir[++i];

				spv_word val = (compcount & 0x00FFFFFF) | (((spv_word)types[comp].first) << 24);

				types[loc] = std::make_pair(ValueType::Vector, val);
			} break;
			case spvgentwo::spv::Op::OpTypeMatrix: {
				spv_word loc = ir[++i];
				spv_word comp = ir[++i];
				spv_word compcount = ir[++i];

				spv_word val = (compcount & 0x00FFFFFF) | (types[comp].second & 0xFF000000);

				types[loc] = std::make_pair(ValueType::Matrix, val);
			} break;
			case spvgentwo::spv::Op::OpExecutionMode: {
				++i; // skip
				spvgentwo::spv::ExecutionMode execMode = (spvgentwo::spv::ExecutionMode)ir[++i];

				if (execMode == spvgentwo::spv::ExecutionMode::LocalSize) {
					LocalSizeX = ir[++i];
					LocalSizeY = ir[++i];
					LocalSizeZ = ir[++i];
				}
			} break;

			case spvgentwo::spv::Op::OpControlBarrier:
			case spvgentwo::spv::Op::OpMemoryBarrier:
			case spvgentwo::spv::Op::OpNamedBarrierInitialize: {
				BarrierUsed = true;
			} break;

			case spvgentwo::spv::Op::OpSNegate: case spvgentwo::spv::Op::OpFNegate:
			case spvgentwo::spv::Op::OpIAdd: case spvgentwo::spv::Op::OpFAdd:
			case spvgentwo::spv::Op::OpISub: case spvgentwo::spv::Op::OpFSub:
			case spvgentwo::spv::Op::OpIMul: case spvgentwo::spv::Op::OpFMul:
			case spvgentwo::spv::Op::OpUDiv: case spvgentwo::spv::Op::OpSDiv:
			case spvgentwo::spv::Op::OpFDiv: case spvgentwo::spv::Op::OpUMod:
			case spvgentwo::spv::Op::OpSRem: case spvgentwo::spv::Op::OpSMod:
			case spvgentwo::spv::Op::OpFRem: case spvgentwo::spv::Op::OpFMod:
			case spvgentwo::spv::Op::OpVectorTimesScalar:
			case spvgentwo::spv::Op::OpMatrixTimesScalar:
			case spvgentwo::spv::Op::OpVectorTimesMatrix:
			case spvgentwo::spv::Op::OpMatrixTimesVector:
			case spvgentwo::spv::Op::OpMatrixTimesMatrix:
			case spvgentwo::spv::Op::OpOuterProduct:
			case spvgentwo::spv::Op::OpDot:
			case spvgentwo::spv::Op::OpIAddCarry:
			case spvgentwo::spv::Op::OpISubBorrow:
			case spvgentwo::spv::Op::OpUMulExtended:
			case spvgentwo::spv::Op::OpSMulExtended:
				ArithmeticInstCount++;
				break;

				
			case spvgentwo::spv::Op::OpShiftRightLogical:
			case spvgentwo::spv::Op::OpShiftRightArithmetic:
			case spvgentwo::spv::Op::OpShiftLeftLogical:
			case spvgentwo::spv::Op::OpBitwiseOr:
			case spvgentwo::spv::Op::OpBitwiseXor:
			case spvgentwo::spv::Op::OpBitwiseAnd:
			case spvgentwo::spv::Op::OpNot:
			case spvgentwo::spv::Op::OpBitFieldInsert:
			case spvgentwo::spv::Op::OpBitFieldSExtract:
			case spvgentwo::spv::Op::OpBitFieldUExtract:
			case spvgentwo::spv::Op::OpBitReverse:
			case spvgentwo::spv::Op::OpBitCount:
				BitInstCount++;
				break;

			case spvgentwo::spv::Op::OpAny: case spvgentwo::spv::Op::OpAll:
			case spvgentwo::spv::Op::OpIsNan: case spvgentwo::spv::Op::OpIsInf:
			case spvgentwo::spv::Op::OpIsFinite: case spvgentwo::spv::Op::OpIsNormal:
			case spvgentwo::spv::Op::OpSignBitSet: case spvgentwo::spv::Op::OpLessOrGreater:
			case spvgentwo::spv::Op::OpOrdered: case spvgentwo::spv::Op::OpUnordered:
			case spvgentwo::spv::Op::OpLogicalEqual: case spvgentwo::spv::Op::OpLogicalNotEqual:
			case spvgentwo::spv::Op::OpLogicalOr: case spvgentwo::spv::Op::OpLogicalAnd:
			case spvgentwo::spv::Op::OpLogicalNot: case spvgentwo::spv::Op::OpSelect:
			case spvgentwo::spv::Op::OpIEqual: case spvgentwo::spv::Op::OpINotEqual:
			case spvgentwo::spv::Op::OpUGreaterThan: case spvgentwo::spv::Op::OpSGreaterThan:
			case spvgentwo::spv::Op::OpUGreaterThanEqual: case spvgentwo::spv::Op::OpSGreaterThanEqual:
			case spvgentwo::spv::Op::OpULessThan: case spvgentwo::spv::Op::OpSLessThan:
			case spvgentwo::spv::Op::OpULessThanEqual: case spvgentwo::spv::Op::OpSLessThanEqual:
			case spvgentwo::spv::Op::OpFOrdEqual: case spvgentwo::spv::Op::OpFUnordEqual:
			case spvgentwo::spv::Op::OpFOrdNotEqual: case spvgentwo::spv::Op::OpFUnordNotEqual:
			case spvgentwo::spv::Op::OpFOrdLessThan: case spvgentwo::spv::Op::OpFUnordLessThan:
			case spvgentwo::spv::Op::OpFOrdGreaterThan: case spvgentwo::spv::Op::OpFUnordGreaterThan:
			case spvgentwo::spv::Op::OpFOrdLessThanEqual: case spvgentwo::spv::Op::OpFUnordLessThanEqual:
			case spvgentwo::spv::Op::OpFOrdGreaterThanEqual: case spvgentwo::spv::Op::OpFUnordGreaterThanEqual:
				LogicalInstCount++;
				break;

			case spvgentwo::spv::Op::OpImageSampleImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleExplicitLod:
			case spvgentwo::spv::Op::OpImageSampleDrefImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleDrefExplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjExplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjDrefImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjDrefExplicitLod:
			case spvgentwo::spv::Op::OpImageFetch: case spvgentwo::spv::Op::OpImageGather:
			case spvgentwo::spv::Op::OpImageDrefGather: case spvgentwo::spv::Op::OpImageRead:
			case spvgentwo::spv::Op::OpImageWrite:
				TextureInstCount++;
				break;

			case spvgentwo::spv::Op::OpDPdx:
			case spvgentwo::spv::Op::OpDPdy:
			case spvgentwo::spv::Op::OpFwidth:
			case spvgentwo::spv::Op::OpDPdxFine:
			case spvgentwo::spv::Op::OpDPdyFine:
			case spvgentwo::spv::Op::OpFwidthFine:
			case spvgentwo::spv::Op::OpDPdxCoarse:
			case spvgentwo::spv::Op::OpDPdyCoarse:
			case spvgentwo::spv::Op::OpFwidthCoarse:
				DerivativeInstCount++;
				break;

			case spvgentwo::spv::Op::OpPhi:
			case spvgentwo::spv::Op::OpLoopMerge:
			case spvgentwo::spv::Op::OpSelectionMerge:
			case spvgentwo::spv::Op::OpLabel:
			case spvgentwo::spv::Op::OpBranch:
			case spvgentwo::spv::Op::OpBranchConditional:
			case spvgentwo::spv::Op::OpSwitch:
			case spvgentwo::spv::Op::OpKill:
			case spvgentwo::spv::Op::OpReturn:
			case spvgentwo::spv::Op::OpReturnValue:
				ControlFlowInstCount++;
				break;
			}

			i = iStart + wordCount + 1;
		}
	}
}