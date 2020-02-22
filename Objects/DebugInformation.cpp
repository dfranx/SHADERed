#include "DebugInformation.h"
#include "SystemVariableManager.h"
#include <ShaderDebugger/HLSLCompiler.h>
#include <ShaderDebugger/HLSLLibrary.h>
#include <ShaderDebugger/GLSLCompiler.h>
#include <ShaderDebugger/GLSLLibrary.h>
#include <ShaderDebugger/Utils.h>

#include <glm/gtc/type_ptr.hpp>
#include <iomanip>
#include <sstream>

namespace ed
{
	bv_variable interpolateValues(bv_program* prog, const bv_variable& var1, const bv_variable& var2, const bv_variable& var3, glm::vec3 weights)
	{
		// TODO: rather than returning value, use pointers... but do that after BVM recode
		if (bv_type_is_integer(var1.type)) { // integers should be flat, tho...
			int val1 = bv_variable_get_int(var1);
			int val2 = bv_variable_get_int(var2);
			int val3 = bv_variable_get_int(var3);

			int outVal = (val1 + val2 + val3) / 3; // this should never be reached, afaik

			return bv_variable_create_int(outVal);
		}
		else if (var1.type == bv_type_float) {
			float val1 = bv_variable_get_float(var1);
			float val2 = bv_variable_get_float(var2);
			float val3 = bv_variable_get_float(var3);

			float outVal = (val1 * weights.x + val2 * weights.y + val3 * weights.z) / (weights.x + weights.y + weights.z);

			return bv_variable_create_float(outVal);
		}
		else if (var1.type == bv_type_object) {
			glm::vec4 vec1 = sd::AsVector<4, float>(var1);
			glm::vec4 vec2 = sd::AsVector<4, float>(var2);
			glm::vec4 vec3 = sd::AsVector<4, float>(var3);

			glm::vec4 outVal = (vec1 * weights.x + vec2 * weights.y + vec3 * weights.z) / (weights.x + weights.y + weights.z);

			bv_object* vec1Obj = bv_variable_get_object(var1);

			bv_type vecType = vec1Obj->prop[0].type;
			u16 vecSize = vec1Obj->type->props.name_count;
			bv_variable outVar = sd::Common::create_vec(prog, vecType, vecSize);
			bv_object* outObj = bv_variable_get_object(outVar);

			for (u16 i = 0; i < vecSize; i++)
				outObj->prop[i] = bv_variable_cast(vecType, bv_variable_create_float(outVal[i]));

			return outVar;
		}

		return bv_variable_create_void();
	}
	glm::vec2 getScreenCoord(const glm::vec4& inp)
	{
		glm::vec4 ret = inp / inp.w;
		return glm::vec2((ret + 1.0f) * 0.5f);
	}
	glm::vec3 getWeights(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 p) {
		glm::vec2 ab = b - a;
		glm::vec2 ac = c - a;
		glm::vec2 ap = p - a;
		float factor = 1 / (ab.x * ac.y - ab.y * ac.x);
		float s = (ac.y * ap.x - ac.x * ap.y) * factor;
		float t = (ab.x * ap.y - ab.y * ap.x) * factor;
		return glm::vec3(1 - s - t, s, t);
	}
	bv_variable applySemantic(sd::ShaderDebugger* debugger, PixelInformation* pixel, sd::ShaderType stage, const std::string& semantic, const std::string& argType, int vertId)
	{
		pipe::ShaderPass* pass = (pipe::ShaderPass*)pixel->Owner->Data;

		std::string sName = semantic;
		std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);

		InputLayoutItem semanticItem;
		bool semanticFound = false;
		for (const auto& lItem : pass->InputLayout) {
			std::string lName = lItem.Semantic;
			std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);

			bool sNameLastDigit = isdigit(sName[sName.size() - 1]);
			bool lNameLastDigit = isdigit(lName[lName.size() - 1]);

			if (sName == lName || (sNameLastDigit && !lNameLastDigit && sName == lName + "0") ||
				(!sNameLastDigit && lNameLastDigit && sName + "0" == lName))
			{
				semanticItem = lItem;
				semanticFound = true;
				break;
			}
		}

		bv_variable varValue = bv_variable_create_void();

		if (stage == sd::ShaderType::Vertex && semanticFound) {
			varValue = bv_variable_create_object(bv_program_get_object_info(debugger->GetProgram(), argType.c_str()));
			bv_object* obj = bv_variable_get_object(varValue);

			glm::vec4 inpValue(pixel->Vertex[vertId].Position, 1.0f);
			switch (semanticItem.Value) {
			case InputLayoutValue::Normal: inpValue = glm::vec4(pixel->Vertex[vertId].Normal, 1.0f); break;
			case InputLayoutValue::Texcoord: inpValue = glm::vec4(pixel->Vertex[vertId].TexCoords, 1.0f, 1.0f); break;
			case InputLayoutValue::Tangent: inpValue = glm::vec4(pixel->Vertex[vertId].Tangent, 1.0f); break;
			case InputLayoutValue::Binormal: inpValue = glm::vec4(pixel->Vertex[vertId].Binormal, 1.0f); break;
			case InputLayoutValue::Color: inpValue = pixel->Vertex[vertId].Color; break;
			}

			for (int p = 0; p < obj->type->props.name_count; p++)
				obj->prop[p] = bv_variable_create_float(inpValue[p]);
		}
		else {
			bv_variable semVal = debugger->GetSemanticValue(sName);
			if (semVal.type == bv_type_void) {
				bool isLastDigit = isdigit(sName[sName.size() - 1]);
				if (isLastDigit)
					sName = sName.substr(0, sName.size() - 1);
				else
					sName = sName + "0";
				semVal = debugger->GetSemanticValue(sName);
			}

			varValue = bv_variable_copy(semVal);
		}

		return varValue;
	}
	bool semanticExists(const std::string& sem, pipe::ShaderPass* pass)
	{
		std::string sName = sem;
		std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);

		for (const auto& lItem : pass->InputLayout) {
			std::string lName = lItem.Semantic;
			std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);

			bool sNameLastDigit = isdigit(sName[sName.size() - 1]);
			bool lNameLastDigit = isdigit(lName[lName.size() - 1]);

			if (sName == lName || (sNameLastDigit && !lNameLastDigit && sName == lName + "0") ||
				(!sNameLastDigit && lNameLastDigit && sName + "0" == lName))
			{
				return true;
			}
		}

		return false;
	}
	bv_variable fetchBufferElement(bv_program* prog, ed::ShaderLanguage lang, void* data, ShaderVariable::ValueType type, int instanceID, int perRowSize, int rowOffset)
	{
		bool ret = false;
		int actualIndex = instanceID * perRowSize + rowOffset;
		void* actualData = (char*)data + actualIndex;

		switch (type) {
		case ed::ShaderVariable::ValueType::Float4x4:
			return sd::Common::create_mat(prog, lang == ed::ShaderLanguage::GLSL ? "mat4" : "float4x4", new sd::Matrix(glm::make_mat4x4((float*)actualData), 4, 4));
		case ed::ShaderVariable::ValueType::Float3x3:
			return sd::Common::create_mat(prog, lang == ed::ShaderLanguage::GLSL ? "mat3" : "float3x3", new sd::Matrix(glm::make_mat3x3((float*)actualData), 3, 3));
		case ed::ShaderVariable::ValueType::Float2x2:
			return sd::Common::create_mat(prog, lang == ed::ShaderLanguage::GLSL ? "mat2" : "float2x2", new sd::Matrix(glm::make_mat2x2((float*)actualData), 2, 2));
		case ed::ShaderVariable::ValueType::Float1:
			return bv_variable_create_float(*(float*)actualData);
		case ed::ShaderVariable::ValueType::Float2:
			return sd::Common::create_float2(prog, glm::make_vec2<float>((float*)actualData));
		case ed::ShaderVariable::ValueType::Float3:
			return sd::Common::create_float3(prog, glm::make_vec3<float>((float*)actualData));
		case ed::ShaderVariable::ValueType::Float4:
			return sd::Common::create_float4(prog, glm::make_vec4<float>((float*)actualData));
		case ed::ShaderVariable::ValueType::Integer1:
			return bv_variable_create_int(*((int*)actualData));
		case ed::ShaderVariable::ValueType::Integer2:
			return sd::Common::create_int2(prog, glm::make_vec2<int>((int*)actualData));
		case ed::ShaderVariable::ValueType::Integer3:
			return sd::Common::create_int3(prog, glm::make_vec3<int>((int*)actualData));
		case ed::ShaderVariable::ValueType::Integer4:
			return sd::Common::create_int4(prog, glm::make_vec4<int>((int*)actualData));
		case ed::ShaderVariable::ValueType::Boolean1:
			return bv_variable_create_uchar(*((bool*)actualData));
		case ed::ShaderVariable::ValueType::Boolean2:
			return sd::Common::create_bool2(prog, glm::make_vec2<bool>((bool*)actualData));
		case ed::ShaderVariable::ValueType::Boolean3:
			return sd::Common::create_bool3(prog, glm::make_vec3<bool>((bool*)actualData));
		case ed::ShaderVariable::ValueType::Boolean4:
			return sd::Common::create_bool4(prog, glm::make_vec4<bool>((bool*)actualData));
		}

		return bv_variable_create_void();
	}


	void DebugInformation::m_cleanTextures(sd::ShaderType stage)
	{
		for (sd::Texture* tex : m_textures[stage])
			delete tex;
		for (sd::TextureCube* cube : m_cubemaps[stage])
			delete cube;
		m_textures[stage].clear();
		m_cubemaps[stage].clear();
	}


	DebugInformation::DebugInformation(ObjectManager* objs, RenderEngine* renderer)
	{
		m_pixel = nullptr;
		m_lang = ed::ShaderLanguage::HLSL;
		m_stage = sd::ShaderType::Vertex;
		m_entry = "";
		m_objs = objs;
		m_renderer = renderer;

		m_libHLSL = sd::HLSL::Library();
		m_libGLSL = sd::GLSL::Library();

		m_args.capacity = 0;
		m_args.data = nullptr;
		m_argsFetch.capacity = 0;
		m_argsFetch.data = nullptr;
		m_isDebugging = false;
	}
	bool DebugInformation::SetSource(ed::ShaderLanguage lang, sd::ShaderType stage, const std::string& entry, const std::string& src)
	{
		bool ret = false;

		m_lang = lang;
		m_stage = stage;
		m_entry = entry;

		if (lang == ed::ShaderLanguage::HLSL) {
			ret = Engine.SetSource<sd::HLSLCompiler>(stage, src, entry, 0, m_libHLSL);
			
			if (ret) {
				if (stage == sd::ShaderType::Vertex) {
					bv_variable svPosition = sd::Common::create_float4(Engine.GetProgram());
					
					Engine.SetSemanticValue("SV_VertexID", bv_variable_create_int(0));
					Engine.SetSemanticValue("SV_Position", svPosition);

					bv_variable_deinitialize(&svPosition);
				}
				else if (stage == sd::ShaderType::Pixel) {
					Engine.SetSemanticValue("SV_IsFrontFace", bv_variable_create_uchar(0));
				}
			}
		}
		else {
			ret = Engine.SetSource<sd::GLSLCompiler>(stage, src, entry, 0, m_libGLSL);

			// TODO: check if built-in variables have been redeclared
			// TODO: add other variables too
			if (ret) {
				if (stage == sd::ShaderType::Vertex) {
					Engine.AddGlobal("gl_VertexID");
					Engine.SetGlobalValue("gl_VertexID", bv_variable_create_int(0));

					Engine.AddGlobal("gl_InstanceID");
					Engine.SetGlobalValue("gl_InstanceID", bv_variable_create_int(0));

					Engine.AddGlobal("gl_Position");
					Engine.SetGlobalValue("gl_Position", "vec4", glm::vec4(0.0f));
				}
				else if (stage == sd::ShaderType::Pixel) {
					Engine.AddGlobal("gl_FragCoord");
					Engine.SetGlobalValue("gl_FragCoord", "vec4", glm::vec4(0.0f));

					Engine.AddGlobal("gl_FrontFacing");
					Engine.SetGlobalValue("gl_FrontFacing", bv_variable_create_uchar(0));
				}
			}
		}

		return ret;
	}
	glm::vec2 DebugInformation::GetVertexScreenPosition(PixelInformation& pixel, int id)
	{
		/* INPUTS */
		// look for arguments when using HLSL
		if (m_lang == ed::ShaderLanguage::HLSL) {
			// TODO: vertex count
			glm::vec4 glPos;

			// getting objects and SV_Positions
			int posInd = 0;
			for (const auto& memb : m_vsOutput.Members) {
				std::string smn = memb.Semantic;
				std::transform(smn.begin(), smn.end(), smn.begin(), ::tolower);

				if (smn == "sv_position")
					break;
				posInd++;
			}
			if (m_vsOutput.Name.empty())
				posInd = -1;
			
			if (posInd == -1)
				glPos = sd::AsVector<4, float>(pixel.VertexShaderOutput[id]["return"]);
			else {
				bv_object* obj = bv_variable_get_object(pixel.VertexShaderOutput[id]["return"]);
				glPos = sd::AsVector<4, float>(obj->prop[posInd]);
			}

			return getScreenCoord(glPos);
		}
		// look for global variables when using GLSL
		else {
			glm::vec4 glPos = sd::AsVector<4, float>(pixel.VertexShaderOutput[id]["gl_Position"]);

			return getScreenCoord(glPos);
		}
	}
	void DebugInformation::InitEngine(PixelInformation& pixel, int id)
	{
		m_pixel = &pixel;
		m_cleanTextures(m_stage);
		
		int vertexBase = (m_pixel->VertexID / m_pixel->VertexCount) * m_pixel->VertexCount;
		pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);
		bool isInstanced = false;
		BufferObject* instanceBuffer = nullptr;

		// update some values
		auto& itemVarValues = m_renderer->GetItemVariableValues();

		// update the value for this element
		for (int k = 0; k < itemVarValues.size(); k++)
			if (itemVarValues[k].Item == pixel.Object)
				itemVarValues[k].Variable->Data = itemVarValues[k].NewValue->Data;

		// update system variables
		if (pixel.Object->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(pixel.Object->Data);

			if (geoData->Type == pipe::GeometryItem::Rectangle) {
				// TODO: don't multiply with m_renderer->GetLastRenderSize() but rather with actual RT size
				glm::vec3 scaleRect(geoData->Scale.x * m_renderer->GetLastRenderSize().x, geoData->Scale.y * m_renderer->GetLastRenderSize().y, 1.0f);
				glm::vec3 posRect((geoData->Position.x + 0.5f) * m_renderer->GetLastRenderSize().x, (geoData->Position.y + 0.5f) * m_renderer->GetLastRenderSize().y, -1000.0f);
				SystemVariableManager::Instance().SetGeometryTransform(pixel.Object, scaleRect, geoData->Rotation, posRect);
			}
			else
				SystemVariableManager::Instance().SetGeometryTransform(pixel.Object, geoData->Scale, geoData->Rotation, geoData->Position);

			SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(pixel.Object));

			isInstanced = geoData->Instanced;
			instanceBuffer = (BufferObject*)geoData->InstanceBuffer;
		}
		else if (pixel.Object->Type == PipelineItem::ItemType::Model) {
			pipe::Model* objData = reinterpret_cast<pipe::Model*>(pixel.Object->Data);

			SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(pixel.Object));
			SystemVariableManager::Instance().SetGeometryTransform(pixel.Object, objData->Scale, objData->Rotation, objData->Position);
		
			isInstanced = objData->Instanced;
			instanceBuffer = (BufferObject*)objData->InstanceBuffer;
		}

		// update variables
		pass->Variables.Bind(pixel.Object);

		/* UNIFORMS */
		const auto& globals = Engine.GetCompiler()->GetGlobals();
		const auto& passUniforms = pass->Variables.GetVariables();
		const std::vector<GLuint>& srvs = m_objs->GetBindList(pixel.Owner);
		int samplerId = 0;
		for (const auto& glob : globals) {
			if (glob.Storage == sd::Variable::StorageType::Uniform) {
				if (sd::IsBasicTexture(glob.Type.c_str())) {
					int myId = glob.InputSlot == -1 ? samplerId : glob.InputSlot;

					if (myId < srvs.size()) {
						std::string itemName = m_objs->GetItemNameByTextureID(srvs[myId]);
						ObjectManagerItem* itemData = m_objs->GetObjectManagerItem(itemName);

						// get texture size
						glm::ivec2 size(1, 1);
						if (itemData != nullptr) {
							if (itemData->RT != nullptr)
								size = m_objs->GetRenderTextureSize(itemName);
							else if (itemData->Image != nullptr)
								size = itemData->Image->Size;
							else if (itemData->Sound != nullptr)
								size = glm::ivec2(512, 2);
							else
								size = itemData->ImageSize;
						}

						// allocate ShaderDebugger texture
						sd::Texture* tex = new sd::Texture();
						tex->Allocate(size.x, size.y);
						tex->UserData = srvs[myId];

						// get the data from the GPU
						glBindTexture(GL_TEXTURE_2D, srvs[myId]);
						glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &tex->Data[0][0]);
						glBindTexture(GL_TEXTURE_2D, 0);

						// set and cache
						Engine.SetGlobalValue(glob.Name, glob.Type, tex);
						m_textures[m_stage].push_back(tex);
					}

					samplerId++;
				} 
				else if (sd::IsCubemap(glob.Type.c_str())) {
					int myId = glob.InputSlot == -1 ? samplerId : glob.InputSlot;

					if (myId < srvs.size()) {
						std::string itemName = m_objs->GetItemNameByTextureID(srvs[myId]);
						ObjectManagerItem* itemData = m_objs->GetObjectManagerItem(itemName);

						// get texture size
						glm::ivec2 size(1, 1);
						if (itemData != nullptr) {
							if (itemData->RT != nullptr)
								size = m_objs->GetRenderTextureSize(itemName);
							else if (itemData->Image != nullptr)
								size = itemData->Image->Size;
							else if (itemData->Sound != nullptr)
								size = glm::ivec2(512, 2);
							else
								size = itemData->ImageSize;
						}

						// allocate cubemap
						sd::TextureCube* cube = new sd::TextureCube();
						cube->UserData = srvs[myId];

						// get the data from the GPU
						glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[myId]);
						for (int i = 0; i < 6; i++) {
							sd::Texture* tex = new sd::Texture();
							tex->Allocate(size.x, size.y);

							glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, &tex->Data[0][0]);
							
							cube->Faces[i] = tex;
							m_textures[m_stage].push_back(tex);
						}
						glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

						// set and cache
						Engine.SetGlobalValue(glob.Name, glob.Type, cube);
						m_cubemaps[m_stage].push_back(cube);
					}

					samplerId++;
				}
				else {
					for (ShaderVariable* var : passUniforms) {
						if (strcmp(glob.Name.c_str(), var->Name) == 0) {
							ShaderVariable::ValueType valType = var->GetType();

							bv_variable varValue;
							switch (valType) {
							case ShaderVariable::ValueType::Boolean1: varValue = bv_variable_create_uchar(var->AsBoolean()); break;
							case ShaderVariable::ValueType::Boolean2: varValue = sd::Common::create_bool2(Engine.GetProgram(), glm::make_vec2<bool>(var->AsBooleanPtr())); break;
							case ShaderVariable::ValueType::Boolean3: varValue = sd::Common::create_bool3(Engine.GetProgram(), glm::make_vec3<bool>(var->AsBooleanPtr())); break;
							case ShaderVariable::ValueType::Boolean4: varValue = sd::Common::create_bool4(Engine.GetProgram(), glm::make_vec4<bool>(var->AsBooleanPtr())); break;
							case ShaderVariable::ValueType::Integer1: varValue = bv_variable_create_int(var->AsInteger()); break;
							case ShaderVariable::ValueType::Integer2: varValue = sd::Common::create_int2(Engine.GetProgram(), glm::make_vec2<int>(var->AsIntegerPtr())); break;
							case ShaderVariable::ValueType::Integer3: varValue = sd::Common::create_int3(Engine.GetProgram(), glm::make_vec3<int>(var->AsIntegerPtr())); break;
							case ShaderVariable::ValueType::Integer4: varValue = sd::Common::create_int4(Engine.GetProgram(), glm::make_vec4<int>(var->AsIntegerPtr())); break;
							case ShaderVariable::ValueType::Float1: varValue = bv_variable_create_float(var->AsFloat()); break;
							case ShaderVariable::ValueType::Float2: varValue = sd::Common::create_float2(Engine.GetProgram(), glm::make_vec2<float>(var->AsFloatPtr())); break;
							case ShaderVariable::ValueType::Float3: varValue = sd::Common::create_float3(Engine.GetProgram(), glm::make_vec3<float>(var->AsFloatPtr())); break;
							case ShaderVariable::ValueType::Float4: varValue = sd::Common::create_float4(Engine.GetProgram(), glm::make_vec4<float>(var->AsFloatPtr())); break;
							case ShaderVariable::ValueType::Float2x2: varValue = sd::Common::create_mat(Engine.GetProgram(), m_lang == ed::ShaderLanguage::GLSL ? "mat2" : "float2x2", new sd::Matrix(glm::make_mat2x2(var->AsFloatPtr()), 2, 2)); break;
							case ShaderVariable::ValueType::Float3x3: varValue = sd::Common::create_mat(Engine.GetProgram(), m_lang == ed::ShaderLanguage::GLSL ? "mat3" : "float3x3", new sd::Matrix(glm::make_mat3x3(var->AsFloatPtr()), 3, 3)); break;
							case ShaderVariable::ValueType::Float4x4: varValue = sd::Common::create_mat(Engine.GetProgram(), m_lang == ed::ShaderLanguage::GLSL ? "mat4" : "float4x4", new sd::Matrix(glm::make_mat4x4(var->AsFloatPtr()), 4, 4)); break;
							}
							Engine.SetGlobalValue(glob.Name, varValue);

							bv_variable_deinitialize(&varValue);

							break;
						}
					}
				}
			}
		}

		/* INPUTS */
		// look for arguments when using HLSL
		if (m_lang == ed::ShaderLanguage::HLSL) {
			const auto& funcs = Engine.GetCompiler()->GetFunctions();
			const auto& structs = Engine.GetCompiler()->GetStructures();
			std::vector<sd::Variable> args;

			for (const auto& f : funcs)
				if (f.Name == m_entry) {
					args = f.Arguments;
					break;
				}

			if (m_args.data != nullptr) {
				bv_stack_delete_memory(&m_args);
				m_args.data = nullptr;
			}
			if (m_argsFetch.data != nullptr) {
				bv_stack_delete_memory(&m_argsFetch);
				m_argsFetch.data = nullptr;
			}

			m_args = bv_stack_create();
			m_argsFetch = bv_stack_create();

			if (m_stage == sd::ShaderType::Vertex) {
				Engine.SetSemanticValue("SV_VertexID", bv_variable_create_int(vertexBase + id));
				Engine.SetSemanticValue("SV_InstanceID", bv_variable_create_int(m_pixel->InstanceID));
				
				// setting instance buffer values
				if (isInstanced && instanceBuffer != nullptr) {
					auto bufFormatList = m_objs->ParseBufferFormat(instanceBuffer->ViewFormat);
					int instItemIndex = 0;
					int instCurOffset = 0;

					// size per each row
					int perRowSize = 0;
					for (int i = 0; i < bufFormatList.size(); i++)
						perRowSize += ShaderVariable::GetSize(bufFormatList[i]);

					// update the data
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, instanceBuffer->ID);
					glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, instanceBuffer->Size, instanceBuffer->Data);
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

					for (const auto& arg : args) {
						// structures
						if (arg.Semantic.empty()) {
							sd::Structure str;
							for (const auto& s : structs)
								if (s.Name == arg.Type) {
									str = s;
									break;
								}

							for (int i = 0; i < str.Members.size(); i++) {
								sd::Variable memb = str.Members[i];
								if (!semanticExists(memb.Semantic, pass)) {
									bv_variable varVal = fetchBufferElement(Engine.GetProgram(), m_lang, instanceBuffer->Data, bufFormatList[instItemIndex], m_pixel->InstanceID, perRowSize, instCurOffset);
									Engine.SetSemanticValue(memb.Semantic, varVal);
									bv_variable_deinitialize(&varVal);

									instCurOffset += ShaderVariable::GetSize(bufFormatList[instItemIndex]);
									instItemIndex++;
								}
							}

						}
						// vectors, scalars, etc..
						else {
							if (!semanticExists(arg.Semantic, pass)) {
								bv_variable varVal = fetchBufferElement(Engine.GetProgram(), m_lang, instanceBuffer->Data, bufFormatList[instItemIndex], m_pixel->InstanceID, perRowSize, instCurOffset);
								Engine.SetSemanticValue(arg.Semantic, varVal);
								bv_variable_deinitialize(&varVal);

								instCurOffset += ShaderVariable::GetSize(bufFormatList[instItemIndex]);
								instItemIndex++;
							}
						}
					}
				}

				// actual arguments
				for (const auto& arg : args) {
					// structures
					if (arg.Semantic.empty()) {
						bv_variable varValue = bv_variable_create_object(bv_program_get_object_info(Engine.GetProgram(), arg.Type.c_str()));
						bv_object* varObj = bv_variable_get_object(varValue);

						sd::Structure str;
						for (const auto& s : structs)
							if (s.Name == arg.Type) {
								str = s;
								break;
							}

						for (int i = 0; i < str.Members.size(); i++) {
							sd::Variable memb = str.Members[i];
							varObj->prop[i] = applySemantic(&Engine, m_pixel, m_stage, memb.Semantic, memb.Type, id);
						}

						bv_stack_push(&m_argsFetch, bv_variable_copy(varValue));
						bv_stack_push(&m_args, varValue);
					}
					// vectors, scalars, etc..
					else {
						bv_variable varValue = applySemantic(&Engine, m_pixel, m_stage, arg.Semantic, arg.Type, id);

						bv_stack_push(&m_argsFetch, bv_variable_copy(varValue));
						bv_stack_push(&m_args, varValue);
					}
				}
			}
			else if (m_stage == sd::ShaderType::Pixel) {
				// TODO: vertex count
				glm::vec4 glPos[3];
				bv_object* obj[3];

				// getting objects and SV_Positions
				int posInd = 0;
				for (const auto& memb : m_vsOutput.Members) {
					std::string smn = memb.Semantic;
					std::transform(smn.begin(), smn.end(), smn.begin(), ::tolower);

					if (smn == "sv_position")
						break;
					posInd++;
				}
				if (m_vsOutput.Name.empty())
					posInd = -1;
				for (int i = 0; i < m_pixel->VertexCount; i++) {
					if (posInd == -1)
						glPos[i] = sd::AsVector<4, float>(m_pixel->VertexShaderOutput[i]["return"]);
					else {
						obj[i] = bv_variable_get_object(m_pixel->VertexShaderOutput[i]["return"]);
						glPos[i] = sd::AsVector<4, float>(obj[i]->prop[posInd]);
					}
				}

				// weigths
				glm::vec2 scrnPos1 = getScreenCoord(glPos[0]);
				glm::vec2 scrnPos2 = getScreenCoord(glPos[1]);
				glm::vec2 scrnPos3 = getScreenCoord(glPos[2]);
				glm::vec3 weights = getWeights(scrnPos1, scrnPos2, scrnPos3, m_pixel->RelativeCoordinate);
				weights *= glm::vec3(1.0f / glPos[0].w, 1.0f / glPos[1].w, 1.0f / glPos[2].w);

				// setting semantics
				int propId = 0;
				for (const auto& memb : m_vsOutput.Members) {
					bv_variable ival = memb.Flat ? obj[2]->prop[propId] : interpolateValues(Engine.GetProgram(), obj[0]->prop[propId], obj[1]->prop[propId], obj[2]->prop[propId], weights);
					Engine.SetSemanticValue(memb.Semantic, ival);
					bv_variable_deinitialize(&ival);
					propId++;
				}

				// set sv_position (TODO: seems to be wrong values)
				float f = 1.0f, n = 0.0f;
				float interW = (glPos[0].w * weights.x + glPos[1].w * weights.y + glPos[2].w * weights.z) * (weights.x + weights.y + weights.z);
				float interZ = (glPos[0].z * weights.x + glPos[1].z * weights.y + glPos[2].z * weights.z) * (weights.x + weights.y + weights.z);
				float Zd = interZ / interW;
				float Zw = n + Zd * (f-n);
				bv_variable svPosVal = sd::Common::create_float4(Engine.GetProgram(), glm::vec4(m_pixel->Coordinate.x, m_pixel->Coordinate.y, Zw, interW));
				Engine.SetSemanticValue("SV_Position", svPosVal);
				bv_variable_deinitialize(&svPosVal);

				if (m_pixel->VertexShaderOutput[0].count("return")) {
					for (const auto& arg : args) {
						// structures
						if (arg.Semantic.empty()) {
							bv_variable varValue = bv_variable_create_object(bv_program_get_object_info(Engine.GetProgram(), arg.Type.c_str()));
							bv_object* varObj = bv_variable_get_object(varValue);

							sd::Structure str;
							for (const auto& s : structs)
								if (s.Name == arg.Type) {
									str = s;
									break;
								}

							for (int i = 0; i < str.Members.size(); i++) {
								sd::Variable memb = str.Members[i];
								varObj->prop[i] = applySemantic(&Engine, m_pixel, m_stage, memb.Semantic, memb.Type, id);
							}

							bv_stack_push(&m_argsFetch, bv_variable_copy(varValue));
							bv_stack_push(&m_args, varValue);
						}
						// vectors, scalars, etc..
						else {
							bv_variable varValue = applySemantic(&Engine, m_pixel, m_stage, arg.Semantic, arg.Type, id);

							bv_stack_push(&m_argsFetch, bv_variable_copy(varValue));
							bv_stack_push(&m_args, varValue);
						}
					}
				}
			}

			Engine.SetArguments(&m_args);
		}
		// look for global variables when using GLSL
		else {
			if (m_stage == sd::ShaderType::Vertex) {
				for (const auto& glob : globals) {
					if (glob.Storage == sd::Variable::StorageType::In) {
						if (glob.InputSlot < pass->InputLayout.size()) {
							const InputLayoutItem& item = pass->InputLayout[glob.InputSlot];

							bv_variable varValue = bv_variable_create_object(bv_program_get_object_info(Engine.GetProgram(), glob.Type.c_str()));
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

							Engine.SetGlobalValue(glob.Name, varValue);

							bv_variable_deinitialize(&varValue);
						}
						else if (isInstanced && instanceBuffer != nullptr && glob.InputSlot != -1) {
							auto bufFormatList = m_objs->ParseBufferFormat(instanceBuffer->ViewFormat);
							int instCurOffset = 0;
							int fmtIndex = glob.InputSlot - pass->InputLayout.size();

							// size per each row
							int perRowSize = 0;
							for (int i = 0; i < bufFormatList.size(); i++) {
								perRowSize += ShaderVariable::GetSize(bufFormatList[i]);
								if (i < fmtIndex)
									instCurOffset = perRowSize;
							}

							// update the data
							glBindBuffer(GL_SHADER_STORAGE_BUFFER, instanceBuffer->ID);
							glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, instanceBuffer->Size, instanceBuffer->Data);
							glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

							bv_variable varVal = fetchBufferElement(Engine.GetProgram(), m_lang, instanceBuffer->Data, bufFormatList[fmtIndex], m_pixel->InstanceID, perRowSize, instCurOffset);
							Engine.SetGlobalValue(glob.Name, varVal);
							bv_variable_deinitialize(&varVal);
						}
					}
				}

				Engine.SetGlobalValue("gl_VertexID", bv_variable_create_int(vertexBase + id));
				Engine.SetGlobalValue("gl_InstanceID", bv_variable_create_int(m_pixel->InstanceID));
			}
			else if (m_stage == sd::ShaderType::Pixel) {
				glm::vec4 glPos1 = sd::AsVector<4, float>(m_pixel->VertexShaderOutput[0]["gl_Position"]);
				glm::vec4 glPos2 = sd::AsVector<4, float>(m_pixel->VertexShaderOutput[1]["gl_Position"]);
				glm::vec4 glPos3 = sd::AsVector<4, float>(m_pixel->VertexShaderOutput[2]["gl_Position"]);

				glm::vec2 scrnPos1 = getScreenCoord(glPos1);
				glm::vec2 scrnPos2 = getScreenCoord(glPos2);
				glm::vec2 scrnPos3 = getScreenCoord(glPos3);

				glm::vec3 weights = getWeights(scrnPos1, scrnPos2, scrnPos3, m_pixel->RelativeCoordinate);
				weights *= glm::vec3(1.0f / glPos1.w, 1.0f / glPos2.w, 1.0f / glPos3.w);
				
				for (const auto& glob : globals) {
					if (glob.Storage == sd::Variable::StorageType::In) {
						if (m_pixel->VertexShaderOutput[0].count(glob.Name)) {
							
							// TODO: vertex count
							bv_variable var1 = m_pixel->VertexShaderOutput[0][glob.Name];
							bv_variable var2 = m_pixel->VertexShaderOutput[1][glob.Name];
							bv_variable var3 = m_pixel->VertexShaderOutput[2][glob.Name];

							// last vertex convention
							bv_variable varValue = glob.Flat ? bv_variable_copy(var3) : interpolateValues(Engine.GetProgram(), var1, var2, var3, weights);

							Engine.SetGlobalValue(glob.Name, varValue);

							bv_variable_deinitialize(&varValue);
						}
					}
				}

				// TODO: clip control
				float f = 1.0f, n = 0.0f;
				float s = (f - n) / 2;
				float b = (f + n) / 2;

				float interW = (glPos1.w * weights.x + glPos2.w * weights.y + glPos3.w * weights.z) * (weights.x + weights.y + weights.z);
				float interZ = (glPos1.z * weights.x + glPos2.z * weights.y + glPos3.z * weights.z) * (weights.x + weights.y + weights.z);
				float Zd = interZ / interW;
				float Zw = s * Zd + b;
				Engine.SetGlobalValue("gl_FragCoord", "vec4", glm::vec4(m_pixel->Coordinate.x, m_pixel->Coordinate.y, Zw, interW));
			}
		}



		// return old values
		if (pixel.Object->Type == PipelineItem::ItemType::Geometry || pixel.Object->Type == PipelineItem::ItemType::Model)
			for (int k = 0; k < itemVarValues.size(); k++)
				if (itemVarValues[k].Item == pixel.Object)
					itemVarValues[k].Variable->Data = itemVarValues[k].OldValue;
	}
	void DebugInformation::Fetch(int id)
	{
		bv_variable returnValue = Engine.Execute(m_entry, &m_argsFetch);

		if (m_stage == sd::ShaderType::Vertex) {
			if (m_lang == ed::ShaderLanguage::HLSL) {
				m_pixel->VertexShaderOutput[id]["return"] = bv_variable_copy(returnValue);

				// cache the output structure description
				const auto& structs = Engine.GetCompiler()->GetStructures();
				const auto& funcs = Engine.GetCompiler()->GetFunctions();

				m_vsOutput = sd::Structure();

				std::string returnTypeName;
				for (const auto& func : funcs)
					if (func.Name == m_entry) {
						returnTypeName = func.ReturnType;
						break;
					}

				for (const auto& s : structs)
					if (s.Name == returnTypeName) {
						m_vsOutput = s;
						break;
					}
			} 
			else {
				m_pixel->VertexShaderOutput[id]["gl_Position"] = bv_variable_copy(*Engine.GetGlobalValue("gl_Position"));

				// GLSL output variables are globals
				const auto& globals = Engine.GetCompiler()->GetGlobals();
				for (const auto& glob : globals)
					if (glob.Storage == sd::Variable::StorageType::Out)
						m_pixel->VertexShaderOutput[id][glob.Name] = bv_variable_copy(*Engine.GetGlobalValue(glob.Name));
			}
		}
		else if (m_stage == sd::ShaderType::Pixel) {
			// TODO: RT index
			if (m_lang == ed::ShaderLanguage::HLSL) {
				const auto& structs = Engine.GetCompiler()->GetStructures();
				const auto& funcs = Engine.GetCompiler()->GetFunctions();

				std::string returnTypeName;
				for (const auto& func : funcs)
					if (func.Name == m_entry) {
						returnTypeName = func.ReturnType;
						break;
					}
				if (!returnTypeName.empty()) {
					sd::Structure str;
					for (const auto& s : structs)
						if (s.Name == returnTypeName) {
							str = s;
							break;
						}
					
					// vector
					if (str.Name.empty()) 
						m_pixel->DebuggerColor = glm::clamp(sd::AsVector<4, float>(returnValue), glm::vec4(0.0f), glm::vec4(1.0f));
					// object
					else {
						int outIndex = 0;
						for (const auto& memb : str.Members) {
							char digit = memb.Semantic[memb.Semantic.size() - 1]; // TODO: double digits?
							if (isdigit(digit))
								digit -= '0';
							else digit = 0;

							if (m_pixel->RenderTextureIndex == digit)
								break;
							
							outIndex++;
						}

						bv_object* retObj = bv_variable_get_object(returnValue);
						m_pixel->DebuggerColor = glm::clamp(sd::AsVector<4, float>(retObj->prop[outIndex]), glm::vec4(0.0f), glm::vec4(1.0f));
					}
				}
			}
			else {
				int outIndex = 0;
				const auto& globals = Engine.GetCompiler()->GetGlobals();
				for (const auto& glob : globals)
					if (glob.Storage == sd::Variable::StorageType::Out) {
						m_pixel->DebuggerColor = glm::clamp(sd::AsVector<4, float>(*Engine.GetGlobalValue(glob.Name)), glm::vec4(0.0f), glm::vec4(1.0f));
						if (m_pixel->RenderTextureIndex == glob.InputSlot || (m_pixel->RenderTextureIndex == outIndex && glob.InputSlot == -1))
							break;
						outIndex++;
					}
			}
		}

		bv_variable_deinitialize(&returnValue);
	}


	void DebugInformation::ClearWatchList()
	{
		for (auto& expr : m_watchExprs)
			free(expr);

		m_watchExprs.clear();
		m_watchValues.clear();
	}
	void DebugInformation::RemoveWatch(size_t index)
	{
		free(m_watchExprs[index]);
		m_watchExprs.erase(m_watchExprs.begin() + index);
		m_watchValues.erase(m_watchValues.begin() + index);
	}
	void DebugInformation::AddWatch(const std::string& expr, bool execute)
	{
		char* data = (char*)calloc(256, sizeof(char));
		strcpy(data, expr.c_str());

		m_watchExprs.push_back(data);
		m_watchValues.push_back("");

		if (execute)
			UpdateWatchValue(m_watchExprs.size() - 1);
	}
	void DebugInformation::UpdateWatchValue(size_t index)
	{
		char* expr = m_watchExprs[index];
		bv_variable exprVal = Engine.Immediate(expr);
		m_watchValues[index] = VariableValueToString(exprVal);
		bv_variable_deinitialize(&exprVal);
	}


	std::string DebugInformation::VariableValueToString(const bv_variable& var, int indent)
	{
		std::string ret = "";

		std::string indentStr(indent * 2, ' ');

		if (var.type == bv_type_float)
			ret += indentStr + std::to_string(bv_variable_get_float(var));
		else if (bv_type_is_integer(var.type))
			ret += indentStr + std::to_string(bv_variable_get_int(var));
		else if (var.type == bv_type_object) {
			bv_object* obj = bv_variable_get_object(var);
			for (u16 i = 0; i < obj->type->props.name_count; i++) {
				bv_variable& prop = obj->prop[i];
				bool isObject = prop.type == bv_type_object;

				std::string propName = obj->type->props.names[i];
				ret += indentStr + "." + propName + " = ";

				if (isObject)
					ret += "{\n";

				ret += VariableValueToString(prop, isObject ? (indent + 1) : 0);

				if (isObject)
					ret += "\n" + indentStr + "}";

				if (i != obj->type->props.name_count - 1)
					ret += "\n";
			}

			if (obj->type->props.name_count == 0) {
				if (sd::IsBasicTexture(obj->type->name))
					ret += obj->type->name;
				else {
					bv_type type = sd::GetMatrixTypeFromName(obj->type->name);
					// matrix
					if (type != bv_type_void) {
						sd::Matrix* mat = (sd::Matrix*)obj->user_data;
						std::stringstream ss;

						for (int y = 0; y < mat->Rows; y++) {
							for (int x = 0; x < mat->Columns; x++)
								ss << indentStr << std::left << std::setw(14) << mat->Data[y][x];
							ss << "\n";
						}

						ret += ss.str();
					}
				}
			}
		}

		return ret;
	}


	void DebugInformation::AddBreakpoint(const std::string& file, int line, const std::string& condition, bool enabled)
	{
		std::vector<sd::Breakpoint>& bkpts = m_breakpoints[file];

		bool alreadyExists = false;
		for (size_t i = 0; i < bkpts.size(); i++) {
			if (bkpts[i].Line == line) {
				bkpts[i].Condition = condition;
				bkpts[i].IsConditional = !condition.empty();
				m_breakpointStates[file][i] = enabled;
				alreadyExists = true;
				break;
			}
		}

		if (!alreadyExists) {
			sd::Breakpoint bkpt;
			bkpt.Line = line;
			bkpt.Condition = condition;
			bkpt.IsConditional = !condition.empty();
			bkpts.push_back(bkpt);
			m_breakpointStates[file].push_back(enabled);
		}
	}
	void DebugInformation::RemoveBreakpoint(const std::string& file, int line)
	{
		std::vector<sd::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& states = m_breakpointStates[file];

		for (size_t i = 0; i < bkpts.size(); i++) {
			if (bkpts[i].Line == line) {
				bkpts.erase(bkpts.begin() + i);
				states.erase(states.begin() + i);
				break;
			}
		}
	}
	void DebugInformation::SetBreakpointEnabled(const std::string& file, int line, bool enable)
	{
		std::vector<sd::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& states = m_breakpointStates[file];

		for (size_t i = 0; i < bkpts.size(); i++) {
			if (bkpts[i].Line == line) {
				states[i] = enable;
				break;
			}
		}
	}
	const sd::Breakpoint& DebugInformation::GetBreakpoint(const std::string& file, int line)
	{
		std::vector<sd::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& states = m_breakpointStates[file];

		for (size_t i = 0; i < bkpts.size(); i++)
			if (bkpts[i].Line == line)
				return bkpts[i];
	}
}