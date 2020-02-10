#include "DebugInformation.h"
#include <ShaderDebugger/HLSLCompiler.h>
#include <ShaderDebugger/HLSLLibrary.h>
#include <ShaderDebugger/GLSLCompiler.h>
#include <ShaderDebugger/GLSLLibrary.h>

#include <glm/gtc/type_ptr.hpp>

namespace ed
{
	DebugInformation::DebugInformation()
	{
		m_pixel = nullptr;
		m_lang = ed::ShaderLanguage::HLSL;
		m_stage = sd::ShaderType::Vertex;
		m_entry = "";

		m_args.capacity = 0;
		m_args.data = nullptr;
	}
	bool DebugInformation::SetSource(ed::ShaderLanguage lang, sd::ShaderType stage, const std::string& entry, const std::string& src)
	{
		bool ret = false;

		m_lang = lang;
		m_stage = stage;
		m_entry = entry;

		if (lang == ed::ShaderLanguage::HLSL) {
			ret = DebugEngine.SetSource<sd::HLSLCompiler>(stage, src, entry, 0, sd::HLSL::Library());
			
			if (ret) {
				if (stage == sd::ShaderType::Vertex) {
					bv_variable svPosition = sd::Common::create_float4(DebugEngine.GetProgram());

					DebugEngine.SetSemanticValue("SV_VertexID", bv_variable_create_int(0));
					DebugEngine.SetSemanticValue("SV_Position", svPosition);

					bv_variable_deinitialize(&svPosition);
				}
				else if (stage == sd::ShaderType::Pixel) {
					DebugEngine.SetSemanticValue("SV_IsFrontFace", bv_variable_create_uchar(0));
				}
			}
		} else {
			ret = DebugEngine.SetSource<sd::GLSLCompiler>(stage, src, entry, 0, sd::GLSL::Library());

			// TODO: check if built-in variables have been redeclared
			// TODO: add other variables too
			if (ret) {
				if (stage == sd::ShaderType::Vertex) {
					DebugEngine.AddGlobal("gl_VertexID");
					DebugEngine.SetGlobalValue("gl_VertexID", bv_variable_create_int(0));

					DebugEngine.AddGlobal("gl_Position");
					DebugEngine.SetGlobalValue("gl_Position", "vec4", glm::vec4(0.0f));
				}
				else if (stage == sd::ShaderType::Pixel) {
					DebugEngine.AddGlobal("gl_FragCoord");
					DebugEngine.SetGlobalValue("gl_FragCoord", "vec4", glm::vec4(0.0f));

					DebugEngine.AddGlobal("gl_FrontFacing");
					DebugEngine.SetGlobalValue("gl_FrontFacing", bv_variable_create_uchar(0));
				}
			}
		}

		return ret;
	}
	void DebugInformation::InitEngine(PixelInformation& pixel, int id)
	{
		m_pixel = &pixel;

		pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

		/* UNIFORMS */
		const auto& globals = DebugEngine.GetCompiler()->GetGlobals();
		const auto& passUniforms = pass->Variables.GetVariables();
		for (const auto& glob : globals) {
			if (glob.Storage == sd::Variable::StorageType::Uniform) {
				for (ShaderVariable* var : passUniforms) {
					if (strcmp(glob.Name.c_str(), var->Name) == 0) {
						ShaderVariable::ValueType valType = var->GetType();

						bv_variable varValue;
						switch (valType) {
						case ShaderVariable::ValueType::Boolean1: varValue = bv_variable_create_uchar(var->AsBoolean()); break;
						case ShaderVariable::ValueType::Boolean2: varValue = sd::Common::create_bool2(DebugEngine.GetProgram(), glm::make_vec2<bool>(var->AsBooleanPtr())); break;
						case ShaderVariable::ValueType::Boolean3: varValue = sd::Common::create_bool3(DebugEngine.GetProgram(), glm::make_vec3<bool>(var->AsBooleanPtr())); break;
						case ShaderVariable::ValueType::Boolean4: varValue = sd::Common::create_bool4(DebugEngine.GetProgram(), glm::make_vec4<bool>(var->AsBooleanPtr())); break;
						case ShaderVariable::ValueType::Integer1: varValue = bv_variable_create_int(var->AsInteger()); break;
						case ShaderVariable::ValueType::Integer2: varValue = sd::Common::create_int2(DebugEngine.GetProgram(), glm::make_vec2<int>(var->AsIntegerPtr())); break;
						case ShaderVariable::ValueType::Integer3: varValue = sd::Common::create_int3(DebugEngine.GetProgram(), glm::make_vec3<int>(var->AsIntegerPtr())); break;
						case ShaderVariable::ValueType::Integer4: varValue = sd::Common::create_int4(DebugEngine.GetProgram(), glm::make_vec4<int>(var->AsIntegerPtr())); break;
						case ShaderVariable::ValueType::Float1: varValue = bv_variable_create_float(var->AsFloat()); break;
						case ShaderVariable::ValueType::Float2: varValue = sd::Common::create_float2(DebugEngine.GetProgram(), glm::make_vec2<float>(var->AsFloatPtr())); break;
						case ShaderVariable::ValueType::Float3: varValue = sd::Common::create_float3(DebugEngine.GetProgram(), glm::make_vec3<float>(var->AsFloatPtr())); break;
						case ShaderVariable::ValueType::Float4: varValue = sd::Common::create_float4(DebugEngine.GetProgram(), glm::make_vec4<float>(var->AsFloatPtr())); break;
						case ShaderVariable::ValueType::Float2x2: varValue = sd::Common::create_mat(DebugEngine.GetProgram(), m_lang == ed::ShaderLanguage::GLSL ? "mat2" : "float2x2", new sd::Matrix(glm::make_mat2x2(var->AsFloatPtr()), 2, 2)); break;
						case ShaderVariable::ValueType::Float3x3: varValue = sd::Common::create_mat(DebugEngine.GetProgram(), m_lang == ed::ShaderLanguage::GLSL ? "mat3" : "float3x3", new sd::Matrix(glm::make_mat3x3(var->AsFloatPtr()), 3, 3)); break;
						case ShaderVariable::ValueType::Float4x4: varValue = sd::Common::create_mat(DebugEngine.GetProgram(), m_lang == ed::ShaderLanguage::GLSL ? "mat4" : "float4x4", new sd::Matrix(glm::make_mat4x4(var->AsFloatPtr()), 4, 4)); break;
						}
						DebugEngine.SetGlobalValue(glob.Name, varValue);

						bv_variable_deinitialize(&varValue);

						break;
					}
				}
			}
		}

		/* INPUTS */
		// look for arguments when using HLSL
		if (m_lang == ed::ShaderLanguage::HLSL) {
			const auto& funcs = DebugEngine.GetCompiler()->GetFunctions();
			std::vector<sd::Variable> args;

			for (const auto& f : funcs)
				if (f.Name == m_entry) {
					args = f.Arguments;
					break;
				}
			if (m_args.data != nullptr) {
				bv_stack_delete(&m_args);
				m_args.data = nullptr;
			}

			m_args = bv_stack_create();

			// TODO: init
		}
		// look for global variables when using GLSL
		else {
			for (const auto& glob : globals) {
				if (glob.Storage == sd::Variable::StorageType::In) {
					if (glob.InputSlot < pass->InputLayout.size()) {
						const InputLayoutItem& item = pass->InputLayout[glob.InputSlot];

						bv_variable varValue = bv_variable_create_object(bv_program_get_object_info(DebugEngine.GetProgram(), glob.Type.c_str()));
						bv_object* obj = bv_variable_get_object(varValue);

						glm::vec4 inpValue(m_pixel->Vertex[id].Position, 1.0f);
						switch (item.Value) {
						case InputLayoutValue::Normal: inpValue = glm::vec4(m_pixel->Vertex[id].Normal, 1.0f); break;
						case InputLayoutValue::Texcoord: inpValue = glm::vec4(m_pixel->Vertex[id].TexCoords, 1.0f, 1.0f); break;
						case InputLayoutValue::Tangent: inpValue = glm::vec4(m_pixel->Vertex[id].Tangent, 1.0f); break;
						case InputLayoutValue::Binormal: inpValue = glm::vec4(m_pixel->Vertex[id].Binormal, 1.0f); break;
						case InputLayoutValue::Color: inpValue = m_pixel->Vertex[id].Color; break;
						}

						for (int p = 0; p < obj->type->props.name_count; p++)
							obj->prop[p] = bv_variable_create_float(inpValue[p]); // TODO: cast to int, etc...

						DebugEngine.SetGlobalValue(glob.Name, varValue);

						bv_variable_deinitialize(&varValue);
					}
				}
			}
		}
	}
	void DebugInformation::Fetch(int id)
	{
		// update built-in values, etc..
		if (m_stage == sd::ShaderType::Vertex) {
			if (m_lang == ed::ShaderLanguage::HLSL) {
				DebugEngine.SetSemanticValue("SV_VertexID", bv_variable_create_int(m_pixel->VertexID + id));

				// TODO: apply semantic value
			}
			else
				DebugEngine.SetGlobalValue("gl_VertexID", bv_variable_create_int(m_pixel->VertexID + id));
		}

		DebugEngine.Execute(m_entry, &m_args);

		if (m_stage == sd::ShaderType::Vertex) {
			if (m_lang == ed::ShaderLanguage::HLSL) {
				// TODO: HLSL
			} else {
				m_pixel->VertexShaderOutput[id]["gl_Position"] = bv_variable_copy(*DebugEngine.GetGlobalValue("gl_Position"));

				// GLSL output variables are globals
				const auto& globals = DebugEngine.GetCompiler()->GetGlobals();
				for (const auto& glob : globals)
					if (glob.Storage == sd::Variable::StorageType::Out)
						m_pixel->VertexShaderOutput[id][glob.Name] = bv_variable_copy(*DebugEngine.GetGlobalValue(glob.Name));
			}
		}
		else if (m_stage == sd::ShaderType::Pixel) {

		}
	}
}