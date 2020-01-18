#include "ExportCPP.h"
#include "../../Engine/GeometryFactory.h"
#include "../SystemVariableManager.h"
#include "../Names.h"
#include <ghc/filesystem.hpp>
#include <string>
#include <vector>

#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a)) // TODO: define this somewhere else....

namespace ed
{
	std::string loadFile(const std::string& filename)
	{
		std::ifstream file("file.txt");
		std::string src((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		file.close();
		return src;
	}
	size_t findSection(const std::string& str, const std::string& sec)
	{
		return str.find("[$$" + sec + "$$]");
	}
	void insertSection(std::string& out, size_t loc, const std::string& sec)
	{
		size_t secEnd = out.find("$$]", loc);
		out.erase(out.begin() + loc, out.begin() + secEnd + 3);
		out.insert(loc, sec);
	}
	std::string getShaderFilename(const std::string& filename)
	{
		std::string ret = ghc::filesystem::path(filename).filename();
		
		size_t dotPos = ret.find_last_of('.');
		ret = ret.substr(0, dotPos);

		for (int i = 0; i < ret.size(); i++)
			if (isspace(ret[i])) ret[i] = '_';

		return "ShaderSource_" + ret;
	}
	std::string getTopologyName(GLuint topology)
	{
		static const char* names[] =
		{
			"GL_UNDEFINED",
			"GL_POINTS",
			"GL_LINES",
			"GL_LINE_STRIP",
			"GL_TRIANGLES",
			"GL_TRIANGLE_STRIP",
			"GL_LINES_ADJACENCY",
			"GL_LINE_STRIP_ADJACENCY",
			"GL_TRIANGLES_ADJACENCY",
			"GL_TRIANGLE_STRIP_ADJACENCY"
		};

		for (int tind = 0; tind < HARRAYSIZE(TOPOLOGY_ITEM_VALUES); tind++)
			if (TOPOLOGY_ITEM_VALUES[tind] == topology)
				return std::string(names[tind]);

		return "GL_UNDEFINED";
	}

	std::string getSystemVariableName(ed::ShaderVariable* var)
	{
		switch (var->System) {
		case ed::SystemShaderVariable::Time: return "sysTime";
		case ed::SystemShaderVariable::TimeDelta: return "sysTimeDelta";
		case ed::SystemShaderVariable::FrameIndex: return "sysFrameIndex";
		case ed::SystemShaderVariable::ViewportSize: return "sysViewportSize";
		case ed::SystemShaderVariable::View: return "sysView";
		case ed::SystemShaderVariable::Projection: return "sysProjection";
		case ed::SystemShaderVariable::Orthographic: return "sysOrthographic";
		case ed::SystemShaderVariable::GeometryTransform: return "sysGeometryTransform";
		case ed::SystemShaderVariable::ViewProjection: return "sysViewProjection";
		case ed::SystemShaderVariable::ViewOrthographic: return "sysViewOrthographic";
		}

		return "";
	}

	std::string getVariableValue(ed::ShaderVariable* var)
	{
		switch (var->GetType()) {
		case ShaderVariable::ValueType::Boolean1:
		case ShaderVariable::ValueType::Integer1:
			return std::to_string(var->AsInteger());
		case ShaderVariable::ValueType::Boolean2:
		case ShaderVariable::ValueType::Integer2:
			return std::to_string(*var->AsIntegerPtr()) + ", " + std::to_string(*(var->AsIntegerPtr() + 1));
		case ShaderVariable::ValueType::Boolean3:
		case ShaderVariable::ValueType::Integer3:
			return std::to_string(*var->AsIntegerPtr()) + ", " + std::to_string(*(var->AsIntegerPtr() + 1)) + ", " + std::to_string(*(var->AsIntegerPtr() + 2));
		case ShaderVariable::ValueType::Boolean4:
		case ShaderVariable::ValueType::Integer4:
			return std::to_string(*var->AsIntegerPtr()) + ", " + std::to_string(*(var->AsIntegerPtr() + 1)) + ", " + std::to_string(*(var->AsIntegerPtr() + 2)) + ", " + std::to_string(*(var->AsIntegerPtr() + 3));
		case ShaderVariable::ValueType::Float1:
		case ShaderVariable::ValueType::Float2:
		case ShaderVariable::ValueType::Float3:
		case ShaderVariable::ValueType::Float4:
		case ShaderVariable::ValueType::Float2x2:
		case ShaderVariable::ValueType::Float3x3:
		case ShaderVariable::ValueType::Float4x4: {
			std::string ret = "";
			int size = ed::ShaderVariable::GetSize(var->GetType()) / sizeof(float);
			for (int i = 0; i < size; i++) {
				ret += std::to_string(*(var->AsFloatPtr() + i));
				if (i != size-1)
					ret += ", ";
			}
			return ret;
		} break;
		}
		return "";
	}

	std::string bindVariable(ed::ShaderVariable* var, std::string passName)
	{
		std::string ret = "";

		std::string locSrc = "glGetUniformLocation(" + passName + "_SP, \"" + std::string(var->Name) + "\")";
		
		std::string systemName = getSystemVariableName(var);
		bool isSystem = var->System != ed::SystemShaderVariable::None;

		switch (var->GetType()) {
		case ShaderVariable::ValueType::Boolean1:
		case ShaderVariable::ValueType::Integer1:
			ret = "glUniform1i(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Boolean2:
		case ShaderVariable::ValueType::Integer2:
			ret = "glUniform2i(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Boolean3:
		case ShaderVariable::ValueType::Integer3:
			ret = "glUniform3i(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Boolean4:
		case ShaderVariable::ValueType::Integer4:
			ret = "glUniform4i(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Float1:
			ret = "glUniform1i(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Float2:
			ret = "glUniform2f(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Float3:
			ret = "glUniform3f(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Float4:
			ret = "glUniform4f(" + locSrc + ", " + (isSystem ? systemName : getVariableValue(var)) + ");";
			break;
		case ShaderVariable::ValueType::Float2x2: {
			std::string varName = "tempVar" + passName + "_" + std::string(var->Name);
			if (!isSystem)
				ret = "glm::mat2 " + varName + "(" + getVariableValue(var) + ");\n";
			ret += "glUniformMatrix2fv(" + locSrc + ", 1, GL_FALSE, glm::value_ptr(" + (isSystem ? systemName : varName) + "));";
		} break;
		case ShaderVariable::ValueType::Float3x3: {
			std::string varName = "tempVar" + passName + "_" + std::string(var->Name);
			if (!isSystem)
				ret = "glm::mat3 " + varName + "(" + getVariableValue(var) + ");\n";
			ret += "glUniformMatrix3fv(" + locSrc + ", 1, GL_FALSE, glm::value_ptr(" + (isSystem ? systemName : varName) + "));";
		} break;
		case ShaderVariable::ValueType::Float4x4: {
			std::string varName = "tempVar" + passName + "_" + std::string(var->Name);
			if (!isSystem)
				ret = "glm::mat4 " + varName + "(" + getVariableValue(var) + ");\n";
			ret += "glUniformMatrix4fv(" + locSrc + ", 1, GL_FALSE, glm::value_ptr(" + (isSystem ? systemName : varName) + "));";
		} break;
		}

		return ret;
	}

	bool ExportCPP::Export(InterfaceManager* data, const std::string& outPath, bool externalShaders)
	{
		// TODO: input layouts, all system variables, compute passes, geometry shaders, etc...

		bool usesGeometry[pipe::GeometryItem::GeometryType::Count] = { false };
		bool usesTextures = false;

		if (!ghc::filesystem::exists("data/template.cpp"))
			return false;

		// load template code data
		std::ifstream templateFile("data/template.cpp");
		std::string templateSrc((std::istreambuf_iterator<char>(templateFile)), std::istreambuf_iterator<char>());
		templateFile.close();

		// get list of all shader files
		std::vector<std::string> allShaderFiles;
		const auto& pipeItems = data->Pipeline.GetList();
		for (int i = 0; i < pipeItems.size(); i++) {
			if (pipeItems[i]->Type == ed::PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pipeItems[i]->Data;
				
				// VS
				if (std::count(allShaderFiles.begin(), allShaderFiles.end(), pass->VSPath) == 0)
					allShaderFiles.push_back(pass->VSPath);
				// PS
				if (std::count(allShaderFiles.begin(), allShaderFiles.end(), pass->PSPath) == 0)
					allShaderFiles.push_back(pass->PSPath);
			}
		}

		// store shaders in the generated file
		if (!externalShaders) {
			size_t locShaders = findSection(templateSrc, "shaders");
			if (locShaders != std::string::npos) {
				std::string shadersSrc = "";

				for (const auto& shdrFile : allShaderFiles) {
					shadersSrc += "const char* " + getShaderFilename(shdrFile) + " = R\"(\n";
					shadersSrc += data->Parser.LoadProjectFile(shdrFile) + "\n";
					shadersSrc += ")\";\n";
				}

				insertSection(templateSrc, locShaders, shadersSrc);
			}
		}
		
		// initialize geometry, framebuffers, etc...
		size_t locInit = findSection(templateSrc, "init");
		if (locInit != std::string::npos) {
			std::string initSrc = "";

			// system variables
			initSrc += "float sysTime = 0.0f, sysTimeDelta = 0.0f;\n";
			initSrc += "unsigned int sysFrameIndex = 0;\n";
			initSrc += "glm::vec2 sysViewportSize(sedWindowWidth, sedWindowHeight);\n";

			glm::mat4 matView = SystemVariableManager::Instance().GetViewMatrix();
			glm::mat4 matProj = SystemVariableManager::Instance().GetProjectionMatrix();
			glm::mat4 matOrtho = SystemVariableManager::Instance().GetOrthographicMatrix();

			// sysView
			initSrc += "glm::mat4 sysView(";
			for (int i = 0; i < 16; i++) {
				initSrc += std::to_string(matView[i/4][i%4]);
				if (i != 15)
					initSrc += ", ";
			}
			initSrc += ");\n";

			initSrc += "glm::mat4 sysProjection = glm::perspective(glm::radians(45.0f), sedWindowWidth / sedWindowHeight, 0.1f, 1000.0f);\n";
			initSrc += "glm::mat4 sysOrthographic = glm::ortho(0.0f, sedWindowWidth, sedWindowHeight, 0.0f, 0.1f, 1000.0f);\n";
			initSrc += "glm::mat4 sysGeometryTransform = glm::mat4(1.0f);\n";
			initSrc += "glm::mat4 sysViewProjection = Projection * View;\n";
			initSrc += "glm::mat4 sysViewOrthographic = Orthographic * View;\n\n";

			// objects
			const auto& objItems = data->Objects.GetItemDataList();
			for (int i = 0; i < objItems.size(); i++) {
				if (objItems[i]->RT != nullptr) { // RT
					const auto rtObj = objItems[i]->RT;

					std::string itemName = objItems[i]->RT->Name;
					std::string rtColorName = objItems[i]->RT->Name + "_Color";
					std::string rtDepthName = objItems[i]->RT->Name + "_Depth";

					// size string
					std::string sizeSrc = "";
					if (rtObj->FixedSize.x == -1)
						sizeSrc += std::to_string(rtObj->RatioSize.x) + " * sedWindowWidth, " + std::to_string(rtObj->RatioSize.y) + " * sedWindowHeight";
					else
						sizeSrc += std::to_string(rtObj->FixedSize.x) + ", " + std::to_string(rtObj->FixedSize.y);

					initSrc += "GLuint " + rtColorName + ", " + rtDepthName + ";\n";

					// color texture
					initSrc += "glGenTextures(1, &" + rtColorName + ");\n";
					initSrc += "glBindTexture(GL_TEXTURE_2D, " + rtColorName + ");\n";
					initSrc += "glTexImage2D(GL_TEXTURE_2D, 0, GL_" + std::string(gl::String::Format(rtObj->Format)) + ", " + sizeSrc + ", 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);\n";
					initSrc += "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);\n";
					initSrc += "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n";
					initSrc += "glBindTexture(GL_TEXTURE_2D, 0);\n";

					// depth texture
					initSrc += "glGenTextures(1, &" + rtDepthName + ");\n";
					initSrc += "glBindTexture(GL_TEXTURE_2D, " + rtDepthName + ");\n";
					initSrc += "glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, " + sizeSrc + ", 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);\n";
					initSrc += "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);\n";
					initSrc += "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n";
					initSrc += "glBindTexture(GL_TEXTURE_2D, 0);\n";
				}
			}

			// pipeline items
			for (int i = 0; i < pipeItems.size(); i++) {
				if (pipeItems[i]->Type == ed::PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)pipeItems[i]->Data;

					// load shaders
					if (externalShaders) { }
					else
						initSrc += "GLuint " + std::string(pipeItems[i]->Name) + "_SP = CreateShader(" + getShaderFilename(pass->VSPath) + ", " + getShaderFilename(pass->PSPath) + ");\n";

					// framebuffers
					if (pass->RTCount == 1 && pass->RenderTextures[0] == data->Renderer.GetTexture()) {}
					else {
						GLuint lastID = pass->RenderTextures[pass->RTCount - 1];
						const auto depthRT = data->Objects.GetRenderTexture(lastID);

						initSrc += "glGenFramebuffers(1, &" + std::string(pipeItems[i]->Name) + "_FBO);\n";
						initSrc += "glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)" + std::string(pipeItems[i]->Name) + "_FBO);\n";
						initSrc += "glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, " + depthRT->Name + "_Depth, 0);\n";

						
						for (int i = 0; i < pass->RTCount; i++) {
							const auto curRT = data->Objects.GetRenderTexture(pass->RenderTextures[i]);
							initSrc += "glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + " + std::to_string(i) + ", GL_TEXTURE_2D, " + curRT->Name + "_Color, 0);\n";
						}
						initSrc += "glBindFramebuffer(GL_FRAMEBUFFER, 0);\n";
					}



					// items
					for (const auto& pItem : pass->Items) {
						if (pItem->Type == PipelineItem::ItemType::Geometry) {
							std::string pName = pItem->Name;

							initSrc += "GLuint " + pName + "_VAO, " + pName + "_VBO;\n";
							pipe::GeometryItem* geo = (pipe::GeometryItem*)pItem->Data;
							if (geo->Type == pipe::GeometryItem::GeometryType::ScreenQuadNDC)
								initSrc += pName + "_VAO = CreateScreenQuadNDC(" + pName + "_VBO);\n";
							else if (geo->Type == pipe::GeometryItem::GeometryType::Rectangle)
								initSrc += pName + "_VAO = CreatePlane(" + pName + "_VBO, 1, 1);\n";
							else if (geo->Type == pipe::GeometryItem::Plane)
								initSrc += pName + "_VAO = CreatePlane(" + pName + "_VBO," + std::to_string(geo->Size.x) + ", " + std::to_string(geo->Size.y) + ");";
							else if (geo->Type == pipe::GeometryItem::Cube)
								initSrc += pName + "_VAO = CreateCube(" + pName + "_VBO," + std::to_string(geo->Size.x) + ", " + std::to_string(geo->Size.y) + ", " + std::to_string(geo->Size.z) + ");";
						}
					}
				}
			}

			insertSection(templateSrc, locInit, initSrc);
		}

		// events for system variables
		size_t locEvent = findSection(templateSrc, "event");
		insertSection(templateSrc, locEvent, ""); // TODO: system variables

		// render all the items
		size_t locRender = findSection(templateSrc, "render");
		if (locRender != std::string::npos) {
			std::string renderSrc = "";

			for (int i = 0; i < pipeItems.size(); i++) {
				if (pipeItems[i]->Type == ed::PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)pipeItems[i]->Data;

					// use the program
					renderSrc += "// " + std::string(pipeItems[i]->Name) + " shader pass\n";
					renderSrc += "glUseProgram(" + std::string(pipeItems[i]->Name) + "_SP);\n";

					// bind variables
					const auto& vars = pass->Variables.GetVariables();
					for (const auto& var : vars) {
						if (var->System != ed::SystemShaderVariable::GeometryTransform) {
							renderSrc += bindVariable(var, std::string(pipeItems[i]->Name)) + "\n";
						}
					}
					
					renderSrc += "\n";

					// items
					for (const auto& pItem : pass->Items) {
						if (pItem->Type == PipelineItem::ItemType::Geometry) {
							std::string pName = pItem->Name;
							pipe::GeometryItem* geoData = (pipe::GeometryItem*)pItem->Data;

							for (const auto& var : vars) {
								if (var->System == ed::SystemShaderVariable::GeometryTransform) {
									renderSrc += "sysGeometryTransform = glm::translate(glm::mat4(1), glm::vec3(" + std::to_string(geoData->Position.x) + ", " + std::to_string(geoData->Position.y) + ", " + std::to_string(geoData->Position.z) + ")) *" +
										"glm::yawPitchRoll(" + std::to_string(geoData->Rotation.y) + ", " + std::to_string(geoData->Rotation.x) + ", " + std::to_string(geoData->Rotation.z) + ") * " +
										"glm::scale(glm::mat4(1.0f), glm::vec3(" + std::to_string(geoData->Scale.x) + ", " + std::to_string(geoData->Scale.y) + ", " + std::to_string(geoData->Scale.z) + "));\n";
									renderSrc += "glUniformMatrix4fv(glGetUniformLocation(" + std::string(pipeItems[i]->Name) + "_SP, \"" + std::string(var->Name) + "\"), 1, GL_FALSE, glm::value_ptr(sysGeometryTransform));\n";
									break;
								}
							}
							renderSrc += "glBindVertexArray(" + pName + "_VAO);\n";
							if (geoData->Instanced)
								renderSrc += "glDrawArraysInstanced(" + getTopologyName(geoData->Topology) + ", 0, " + std::to_string(eng::GeometryFactory::VertexCount[geoData->Type]) + ", " + std::to_string(geoData->InstanceCount) + ");\n";
							else
								renderSrc += "glDrawArraysInstanced(" + getTopologyName(geoData->Topology) + ", 0, " + std::to_string(eng::GeometryFactory::VertexCount[geoData->Type]) + ");\n";
							renderSrc += "\n";
						}
						renderSrc += "\n";
					}
				}
			}

			insertSection(templateSrc, locRender, renderSrc);
		}
		
		std::ofstream fileWriter(outPath);
		fileWriter << templateSrc;
		fileWriter.close();
	}
}