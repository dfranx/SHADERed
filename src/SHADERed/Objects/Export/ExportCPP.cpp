#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/Export/ExportCPP.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/Logger.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define HARRAYSIZE(a) (sizeof(a) / sizeof(*a)) // TODO: define this somewhere else....

namespace ed {
	std::string loadFile(const std::string& filename)
	{
		std::ifstream file(filename);
		std::string src((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		file.close();
		return src;
	}
	size_t findSection(const std::string& str, const std::string& sec)
	{
		return str.find("[$$" + sec + "$$]");
	}
	std::string getSectionIndent(const std::string& str, const std::string& sec)
	{
		size_t pos = str.find("[$$" + sec + "$$]");
		size_t newLinePos = str.substr(0, pos).find_last_of("\n") + 1; // find last space
		std::string ind = str.substr(newLinePos, pos - newLinePos);

		for (int i = 0; i < ind.size(); i++) {
			if (!isspace(ind[i])) {
				ind.erase(ind.begin() + i);
				i--;
			}
		}
		return ind;
	}
	void insertSection(std::string& out, size_t loc, const std::string& sec)
	{
		size_t secEnd = out.find("$$]", loc);
		size_t newLinePos = out.substr(0, loc).find_last_of("\n") + 1; // find last space

		out.erase(out.begin() + newLinePos, out.begin() + secEnd + 3);
		out.insert(loc, sec);
	}
	void replaceSections(std::string& out, const std::string& sec, const std::string& contents)
	{
		size_t secLoc = findSection(out, sec);
		while (secLoc != std::string::npos) {
			size_t secEnd = out.find("$$]", secLoc);
			out.erase(out.begin() + secLoc, out.begin() + secEnd + 3);
			out.insert(secLoc, contents);
			secLoc = findSection(out, sec);
		}
	}
	std::string getFilename(const std::string& filename)
	{
		std::string ret = std::filesystem::path(filename).filename().string();

		size_t dotPos = ret.find_last_of('.');
		ret = ret.substr(0, dotPos);

		for (int i = 0; i < ret.size(); i++)
			if (isspace(ret[i])) ret[i] = '_';

		return ret;
	}
	std::string getShaderFilename(const std::string& filename)
	{
		return "ShaderSource_" + getFilename(filename);
	}
	std::string getTopologyName(GLuint topology)
	{
		static const char* names[] = {
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
	std::string getFormatName(GLuint fmt)
	{
		static const char* names[] = {
			"GL_UNKNOWN",
			"GL_RGBA",
			"GL_RGB",
			"GL_RG",
			"GL_RED",
			"GL_R8",
			"GL_R8_SNORM",
			"GL_R16",
			"GL_R16_SNORM",
			"GL_RG8",
			"GL_RG8_SNORM",
			"GL_RG16",
			"GL_RG16_SNORM",
			"GL_R3_G3_B2",
			"GL_RGB4",
			"GL_RGB5",
			"GL_RGB8",
			"GL_RGB8_SNORM",
			"GL_RGB10",
			"GL_RGB12",
			"GL_RGB16_SNORM",
			"GL_RGBA2",
			"GL_RGBA4",
			"GL_RGB5_A1",
			"GL_RGBA8",
			"GL_RGBA8_SNORM",
			"GL_RGB10_A2",
			"GL_RGB10_A2_UINT",
			"GL_RGBA12",
			"GL_RGBA16",
			"GL_SRGB8",
			"GL_SRGB_ALPHA8",
			"GL_R16F",
			"GL_RG16F",
			"GL_RGB16F",
			"GL_RGBA16F",
			"GL_R32F",
			"GL_RG32F",
			"GL_RGB32F",
			"GL_RGBA32F",
			"GL_R11F_G11F_B10F",
			"GL_RGB9_E5",
			"GL_R8I",
			"GL_R8UI",
			"GL_R16I",
			"GL_R16UI",
			"GL_R32I",
			"GL_R32UI",
			"GL_RG8I",
			"GL_RG8UI",
			"GL_RG16I",
			"GL_RG16UI",
			"GL_RG32I",
			"GL_RG32UI",
			"GL_RGB8I",
			"GL_RGB8UI",
			"GL_RGB16I",
			"GL_RGB16UI",
			"GL_RGB32I",
			"GL_RGB32UI",
			"GL_RGBA8I",
			"GL_RGBA8UI",
			"GL_RGBA16I",
			"GL_RGBA16UI",
			"GL_RGBA32I",
			"GL_RGBA32UI"
		};

		for (int tind = 0; tind < HARRAYSIZE(FORMAT_VALUES); tind++)
			if (FORMAT_VALUES[tind] == fmt)
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
		case ed::SystemShaderVariable::MousePosition: return "sysMousePosition";
		}

		return "shaderVariable";
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
				ret += std::to_string(*(var->AsFloatPtr() + i)) + "f";
				if (i != size - 1)
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

		if (isSystem) {
			switch (var->GetType()) {
			case ShaderVariable::ValueType::Boolean1:
			case ShaderVariable::ValueType::Integer1:
				ret = "glUniform1i(" + locSrc + ", " + systemName + ");";
				break;
			case ShaderVariable::ValueType::Boolean2:
			case ShaderVariable::ValueType::Integer2:
				ret = "glUniform2iv(" + locSrc + ", 1, glm::value_ptr(" + systemName + "));";
				break;
			case ShaderVariable::ValueType::Boolean3:
			case ShaderVariable::ValueType::Integer3:
				ret = "glUniform3iv(" + locSrc + ", 1, glm::value_ptr(" + systemName + "));";
				break;
			case ShaderVariable::ValueType::Boolean4:
			case ShaderVariable::ValueType::Integer4:
				ret = "glUniform4iv(" + locSrc + ", 1, glm::value_ptr(" + systemName + "));";
				break;
			case ShaderVariable::ValueType::Float1:
				ret = "glUniform1f(" + locSrc + ", " + systemName + ");";
				break;
			case ShaderVariable::ValueType::Float2:
				ret = "glUniform2fv(" + locSrc + ", 1, glm::value_ptr(" + systemName + "));";
				break;
			case ShaderVariable::ValueType::Float3:
				ret = "glUniform3fv(" + locSrc + ", 1, glm::value_ptr(" + systemName + "));";
				break;
			case ShaderVariable::ValueType::Float4:
				ret = "glUniform4fv(" + locSrc + ", 1, glm::value_ptr(" + systemName + "));";
				break;
			}
		} else {
			switch (var->GetType()) {
			case ShaderVariable::ValueType::Boolean1:
			case ShaderVariable::ValueType::Integer1:
				ret = "glUniform1i(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			case ShaderVariable::ValueType::Boolean2:
			case ShaderVariable::ValueType::Integer2:
				ret = "glUniform2i(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			case ShaderVariable::ValueType::Boolean3:
			case ShaderVariable::ValueType::Integer3:
				ret = "glUniform3i(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			case ShaderVariable::ValueType::Boolean4:
			case ShaderVariable::ValueType::Integer4:
				ret = "glUniform4i(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			case ShaderVariable::ValueType::Float1:
				ret = "glUniform1f(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			case ShaderVariable::ValueType::Float2:
				ret = "glUniform2f(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			case ShaderVariable::ValueType::Float3:
				ret = "glUniform3f(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			case ShaderVariable::ValueType::Float4:
				ret = "glUniform4f(" + locSrc + ", " + getVariableValue(var) + ");";
				break;
			}
		}

		return ret;
	}

	bool ExportCPP::Export(InterfaceManager* data, const std::string& outPath, bool externalShaders, bool exportCmakeFiles, const std::string& cmakeProject, bool copyCMakeModules, bool copySTBImage, bool copyImages)
	{
		ed::Logger::Get().Log("Exporting SHADERed project as C++ project");

		bool usesGeometry[pipe::GeometryItem::GeometryType::Count] = { false };
		bool usesTextures = false;

		if (!std::filesystem::exists("data/export/cpp/template.cpp"))
			return false;

		if (exportCmakeFiles && !std::filesystem::exists("data/export/cpp/CMakeLists.txt"))
			return false;

		if (copyCMakeModules && !std::filesystem::exists("data/export/cpp/FindGLM.cmake"))
			return false;

		if (copySTBImage && !std::filesystem::exists("data/export/cpp/stb_image.h"))
			return false;

		std::filesystem::path parentPath = std::filesystem::path(outPath).parent_path();

		// copy CMakeLists.txt
		if (exportCmakeFiles) {
			std::string cmakeLists = loadFile("data/export/cpp/CMakeLists.txt");
			replaceSections(cmakeLists, "project_name", cmakeProject);
			replaceSections(cmakeLists, "project_file", std::filesystem::path(outPath).filename().string());

			std::ofstream outCmake(parentPath / "CMakeLists.txt");
			outCmake << cmakeLists;
			outCmake.close();
		}
		if (copyCMakeModules) {
			if (!std::filesystem::exists(parentPath / "cmake"))
				std::filesystem::create_directory(parentPath / "cmake");

			std::filesystem::path cmakePath("data/export/cpp/FindGLM.cmake");
			std::filesystem::copy_file(cmakePath, parentPath / "cmake/FindGLM.cmake", std::filesystem::copy_options::skip_existing);
		}
		if (copySTBImage) {
			std::filesystem::path cmakePath("data/export/cpp/stb_image.h");
			std::filesystem::copy_file(cmakePath, parentPath / "stb_image.h", std::filesystem::copy_options::skip_existing);
		}

		// load template code data
		std::ifstream templateFile("data/export/cpp/template.cpp");
		std::string templateSrc((std::istreambuf_iterator<char>(templateFile)), std::istreambuf_iterator<char>());
		templateFile.close();

		// get list of all shader files
		std::vector<std::string> allShaderFiles;
		std::vector<std::string> allShaderEntries;
		std::vector<ShaderStage> allShaderTypes;
		const auto& pipeItems = data->Pipeline.GetList();
		for (int i = 0; i < pipeItems.size(); i++) {
			if (pipeItems[i]->Type == ed::PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)pipeItems[i]->Data;

				// VS
				if (std::count(allShaderFiles.begin(), allShaderFiles.end(), pass->VSPath) == 0) {
					allShaderFiles.push_back(pass->VSPath);
					allShaderEntries.push_back(pass->VSEntry);
					allShaderTypes.push_back(ShaderStage::Vertex);
				}
				// PS
				if (std::count(allShaderFiles.begin(), allShaderFiles.end(), pass->PSPath) == 0) {
					allShaderFiles.push_back(pass->PSPath);
					allShaderEntries.push_back(pass->PSEntry);
					allShaderTypes.push_back(ShaderStage::Pixel);
				}
			}
		}

		// store shaders in the generated file
		size_t locShaders = findSection(templateSrc, "shaders");
		std::string indent = getSectionIndent(templateSrc, "shaders");
		if (locShaders != std::string::npos) {
			std::string shadersSrc = "";

			if (!externalShaders) {
				for (int i = 0; i < allShaderFiles.size(); i++) {
					std::string shdrSource = data->Parser.LoadProjectFile(allShaderFiles[i]);
					ShaderLanguage shdrLanguage = ShaderCompiler::GetShaderLanguageFromExtension(allShaderFiles[i].c_str());
					if (shdrLanguage != ShaderLanguage::GLSL) {
						std::vector<ed::ShaderMacro> tempMacros;
						std::vector<unsigned int> tempSPV;

						ShaderCompiler::CompileSourceToSPIRV(tempSPV, shdrLanguage, allShaderFiles[i], shdrSource, allShaderTypes[i], allShaderEntries[i], tempMacros, nullptr, nullptr);
						shdrSource = ShaderCompiler::ConvertToGLSL(tempSPV, shdrLanguage, allShaderTypes[i], false, false, nullptr);
					}

					shadersSrc += "std::string " + getShaderFilename(allShaderFiles[i]) + " = R\"(\n";
					shadersSrc += shdrSource + "\n";
					shadersSrc += ")\";\n";
				}
			}

			insertSection(templateSrc, locShaders, shadersSrc);
		}

		// initialize geometry, framebuffers, etc...
		const auto& objItems = data->Objects.GetObjects();
		size_t locInit = findSection(templateSrc, "init");
		indent = getSectionIndent(templateSrc, "init");
		if (locInit != std::string::npos) {
			std::string initSrc = "";

			// external shaders
			if (externalShaders) {
				initSrc += indent + "// shaders\n";
				for (int i = 0; i < allShaderFiles.size(); i++) {
					std::string shdrFile = (parentPath / std::filesystem::path(allShaderFiles[i]).filename()).string();

					initSrc += indent + "std::string " + getShaderFilename(allShaderFiles[i]) + " = LoadFile(\"" + shdrFile + "\");\n";

					// copy the shader
					std::string shdrSource = data->Parser.LoadProjectFile(allShaderFiles[i]);
					ShaderLanguage shdrLanguage = ShaderCompiler::GetShaderLanguageFromExtension(allShaderFiles[i].c_str());
					if (shdrLanguage != ShaderLanguage::GLSL) {
						std::vector<ed::ShaderMacro> tempMacros;
						std::vector<unsigned int> tempSPV;

						ShaderCompiler::CompileSourceToSPIRV(tempSPV, shdrLanguage, allShaderFiles[i], shdrSource, allShaderTypes[i], allShaderEntries[i], tempMacros, nullptr, nullptr);
						shdrSource = ShaderCompiler::ConvertToGLSL(tempSPV, shdrLanguage, allShaderTypes[i], false, false, nullptr);
					}

					std::ofstream shaderWriter(shdrFile);
					shaderWriter << shdrSource;
					shaderWriter.close();
				}
				initSrc += "\n";
			}

			glm::mat4 matView = SystemVariableManager::Instance().GetViewMatrix();
			glm::mat4 matProj = SystemVariableManager::Instance().GetProjectionMatrix();
			glm::mat4 matOrtho = SystemVariableManager::Instance().GetOrthographicMatrix();

			// system variables
			initSrc += indent + "// system variables\n";
			initSrc += indent + "float sysTime = 0.0f, sysTimeDelta = 0.0f;\n";
			initSrc += indent + "unsigned int sysFrameIndex = 0;\n";

			initSrc += indent + "glm::vec2 sysMousePosition(sedMouseX, sedMouseY);\n";
			initSrc += indent + "glm::vec2 sysViewportSize(sedWindowWidth, sedWindowHeight);\n";
			initSrc += indent + "glm::mat4 sysView(";
			for (int i = 0; i < 16; i++) {
				initSrc += std::to_string(matView[i / 4][i % 4]) + "f";
				if (i != 15)
					initSrc += ", ";
			}
			initSrc += ");\n";
			initSrc += indent + "glm::mat4 sysProjection = glm::perspective(glm::radians(45.0f), sedWindowWidth / sedWindowHeight, 0.1f, 1000.0f);\n";
			initSrc += indent + "glm::mat4 sysOrthographic = glm::ortho(0.0f, sedWindowWidth, sedWindowHeight, 0.0f, 0.1f, 1000.0f);\n";
			initSrc += indent + "glm::mat4 sysGeometryTransform = glm::mat4(1.0f);\n";
			initSrc += indent + "glm::mat4 sysViewProjection = sysProjection * sysView;\n";
			initSrc += indent + "glm::mat4 sysViewOrthographic = sysOrthographic * sysView;\n\n";

			// objects
			initSrc += indent + "// objects\n";
			for (int i = 0; i < objItems.size(); i++) {
				if (objItems[i]->Type == ObjectType::RenderTexture) { // RT
					const auto rtObj = objItems[i]->RT;

					std::string itemName = objItems[i]->Name;
					std::string rtColorName = objItems[i]->Name + "_Color";
					std::string rtDepthName = objItems[i]->Name + "_Depth";

					// size string
					std::string sizeSrc = "";
					if (rtObj->FixedSize.x == -1)
						sizeSrc += std::to_string(rtObj->RatioSize.x) + "f * sedWindowWidth, " + std::to_string(rtObj->RatioSize.y) + "f * sedWindowHeight";
					else
						sizeSrc += std::to_string(rtObj->FixedSize.x) + ", " + std::to_string(rtObj->FixedSize.y);

					initSrc += indent + "// " + itemName + " render texture\n";
					initSrc += indent + "GLuint " + rtColorName + ", " + rtDepthName + ";\n";

					// color texture
					initSrc += indent + "glGenTextures(1, &" + rtColorName + ");\n";
					initSrc += indent + "glBindTexture(GL_TEXTURE_2D, " + rtColorName + ");\n";
					initSrc += indent + "glTexImage2D(GL_TEXTURE_2D, 0, " + std::string(getFormatName(rtObj->Format)) + ", " + sizeSrc + ", 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);\n";
					initSrc += indent + "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);\n";
					initSrc += indent + "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n";
					initSrc += indent + "glBindTexture(GL_TEXTURE_2D, 0);\n";

					// depth texture
					initSrc += indent + "glGenTextures(1, &" + rtDepthName + ");\n";
					initSrc += indent + "glBindTexture(GL_TEXTURE_2D, " + rtDepthName + ");\n";
					initSrc += indent + "glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, " + sizeSrc + ", 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);\n";
					initSrc += indent + "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);\n";
					initSrc += indent + "glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);\n";
					initSrc += indent + "glBindTexture(GL_TEXTURE_2D, 0);\n\n";
				} else if (objItems[i]->Type == ObjectType::Texture) { // Texture
					std::string actualName = objItems[i]->Name;
					std::string texName = actualName;
					if (texName.find('\\') != std::string::npos || texName.find('/') != std::string::npos || texName.find('.') != std::string::npos) {
						texName = std::filesystem::path(texName).filename().string();
					}
					texName = getFilename(texName);

					initSrc += indent + "GLuint " + texName + " = LoadTexture(\"" + std::filesystem::path(actualName).filename().string() + "\");\n\n";

					if (copyImages)
						std::filesystem::copy_file(data->Parser.GetProjectPath(actualName), parentPath / std::filesystem::path(actualName).filename(), std::filesystem::copy_options::skip_existing);
				}
			}

			// pipeline items
			for (int i = 0; i < pipeItems.size(); i++) {
				if (pipeItems[i]->Type == ed::PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)pipeItems[i]->Data;

					// load shaders
					initSrc += indent + "GLuint " + std::string(pipeItems[i]->Name) + "_SP = CreateShader(" + getShaderFilename(pass->VSPath) + ".c_str(), " + getShaderFilename(pass->PSPath) + ".c_str());\n\n";

					// framebuffers
					if (pass->RTCount == 1 && pass->RenderTextures[0] == data->Renderer.GetTexture()) {
					} else {
						GLuint lastID = pass->RenderTextures[pass->RTCount - 1];
						const auto depthRT = data->Objects.GetByTextureID(lastID);

						initSrc += indent + "GLuint " + std::string(pipeItems[i]->Name) + "_FBO = 0;\n";
						initSrc += indent + "glGenFramebuffers(1, &" + std::string(pipeItems[i]->Name) + "_FBO);\n";
						initSrc += indent + "glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)" + std::string(pipeItems[i]->Name) + "_FBO);\n";
						initSrc += indent + "glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, " + depthRT->Name + "_Depth, 0);\n";

						for (int i = 0; i < pass->RTCount; i++) {
							const auto curRT = data->Objects.GetByTextureID(pass->RenderTextures[i]);
							initSrc += indent + "glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + " + std::to_string(i) + ", GL_TEXTURE_2D, " + curRT->Name + "_Color, 0);\n";
						}
						initSrc += indent + "glBindFramebuffer(GL_FRAMEBUFFER, 0);\n\n";
					}

					// items
					for (const auto& pItem : pass->Items) {
						if (pItem->Type == PipelineItem::ItemType::Geometry) {
							std::string pName = pItem->Name;

							initSrc += indent + "GLuint " + pName + "_VAO, " + pName + "_VBO;\n";
							pipe::GeometryItem* geo = (pipe::GeometryItem*)pItem->Data;
							if (geo->Type == pipe::GeometryItem::GeometryType::ScreenQuadNDC)
								initSrc += indent + pName + "_VAO = CreateScreenQuadNDC(" + pName + "_VBO);\n";
							else if (geo->Type == pipe::GeometryItem::GeometryType::Rectangle)
								initSrc += indent + pName + "_VAO = CreatePlane(" + pName + "_VBO, 1, 1);\n";
							else if (geo->Type == pipe::GeometryItem::Plane)
								initSrc += indent + pName + "_VAO = CreatePlane(" + pName + "_VBO, " + std::to_string(geo->Size.x) + "f, " + std::to_string(geo->Size.y) + "f);\n";
							else if (geo->Type == pipe::GeometryItem::Cube)
								initSrc += indent + pName + "_VAO = CreateCube(" + pName + "_VBO, " + std::to_string(geo->Size.x) + "f, " + std::to_string(geo->Size.y) + "f, " + std::to_string(geo->Size.z) + "f);\n";
							initSrc += "\n";
						}
					}
				}
			}

			insertSection(templateSrc, locInit, initSrc);
		}

		// events for system variables
		size_t locResizeEvent = findSection(templateSrc, "resize_event");
		indent = getSectionIndent(templateSrc, "resize_event");
		if (locResizeEvent != std::string::npos) {
			std::string resizeEventSrc = "";

			resizeEventSrc += indent + "sysViewportSize = glm::vec2(sedWindowWidth, sedWindowHeight);\n";
			resizeEventSrc += indent + "sysProjection = glm::perspective(glm::radians(45.0f), sedWindowWidth / sedWindowHeight, 0.1f, 1000.0f);\n";
			resizeEventSrc += indent + "sysOrthographic = glm::ortho(0.0f, sedWindowWidth, sedWindowHeight, 0.0f, 0.1f, 1000.0f);\n";
			resizeEventSrc += indent + "sysGeometryTransform = glm::mat4(1.0f);\n";
			resizeEventSrc += indent + "sysViewProjection = sysProjection * sysView;\n";
			resizeEventSrc += indent + "sysViewOrthographic = sysOrthographic * sysView;\n\n";

			for (const auto& rt : objItems) {
				if (rt->Type == ObjectType::RenderTexture) {
					std::string itemName = rt->Name;
					std::string rtColorName = itemName + "_Color";
					std::string rtDepthName = itemName + "_Depth";

					// size string
					std::string sizeSrc = "";
					if (rt->RT->FixedSize.x == -1)
						sizeSrc += std::to_string(rt->RT->RatioSize.x) + "f * sedWindowWidth, " + std::to_string(rt->RT->RatioSize.y) + "f * sedWindowHeight";
					else
						sizeSrc += std::to_string(rt->RT->FixedSize.x) + ", " + std::to_string(rt->RT->FixedSize.y);

					resizeEventSrc += indent + "glBindTexture(GL_TEXTURE_2D, " + rtColorName + ");\n";
					resizeEventSrc += indent + "glTexImage2D(GL_TEXTURE_2D, 0, " + getFormatName(rt->RT->Format) + +"," + sizeSrc + ", 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);\n";

					resizeEventSrc += indent + "glBindTexture(GL_TEXTURE_2D, " + rtDepthName + ");\n";
					resizeEventSrc += indent + "glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, " + sizeSrc + ", 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);\n";
					resizeEventSrc += indent + "glBindTexture(GL_TEXTURE_2D, 0);\n\n";
				}
			}
			insertSection(templateSrc, locResizeEvent, resizeEventSrc); // TODO: system variables
		}

		// render all the items
		size_t locRender = findSection(templateSrc, "render");
		indent = getSectionIndent(templateSrc, "render");
		if (locRender != std::string::npos) {
			std::string renderSrc = "";

			GLuint previousTexture[MAX_RENDER_TEXTURES] = { 0 }; // dont clear the render target if we use it two times in a row
			GLuint previousDepth = 0;

			for (int i = 0; i < pipeItems.size(); i++) {
				if (pipeItems[i]->Type == ed::PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* pass = (pipe::ShaderPass*)pipeItems[i]->Data;

					// use the program
					renderSrc += indent + "// " + std::string(pipeItems[i]->Name) + " shader pass\n";
					renderSrc += indent + "glUseProgram(" + std::string(pipeItems[i]->Name) + "_SP);\n\n";

					// FBO
					if (pass->RTCount == 1 && pass->RenderTextures[0] == data->Renderer.GetTexture())
						renderSrc += indent + "glBindFramebuffer(GL_FRAMEBUFFER, 0);\n";
					else {
						renderSrc += indent + "glBindFramebuffer(GL_FRAMEBUFFER, " + std::string(pipeItems[i]->Name) + "_FBO);\n";
						renderSrc += indent + "glDrawBuffers(" + std::to_string(pass->RTCount) + ", FBO_Buffers);\n";

						// clear depth texture
						if (pass->DepthTexture != previousDepth) {
							renderSrc += indent + "glStencilMask(0xFFFFFFFF);\n";
							renderSrc += indent + "glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);\n";
							previousDepth = pass->DepthTexture;
						}

						// bind RTs
						for (int i = 0; i < MAX_RENDER_TEXTURES; i++) {
							if (pass->RenderTextures[i] == 0)
								break;

							GLuint rt = pass->RenderTextures[i];
							ObjectManagerItem* rtObjectItem = data->Objects.GetByTextureID(rt);
							RenderTextureObject* rtObject = rtObjectItem->RT;

							// clear and bind rt (only if not used in last shader pass)
							bool usedPreviously = false;
							for (int j = 0; j < MAX_RENDER_TEXTURES; j++)
								if (previousTexture[j] == rt) {
									usedPreviously = true;
									break;
								}
							if (!usedPreviously && rtObject->Clear) {
								renderSrc += indent + "glm::vec4 " + std::string(rtObjectItem->Name) + "_ClearColor = glm::vec4(" + std::to_string(rtObject->ClearColor.r) + ", " + std::to_string(rtObject->ClearColor.g) + ", " + std::to_string(rtObject->ClearColor.b) + ", " + std::to_string(rtObject->ClearColor.a) + ");\n";
								renderSrc += indent + "glClearBufferfv(GL_COLOR, " + std::to_string(i) + ", glm::value_ptr(" + std::string(rtObjectItem->Name) + "_ClearColor));\n";
							}
						}
						for (int i = 0; i < pass->RTCount; i++)
							previousTexture[i] = pass->RenderTextures[i];
					}
					renderSrc += "\n";

					// bind textures
					const auto& srvs = data->Objects.GetBindList(pipeItems[i]);
					for (int j = 0; j < srvs.size(); j++) {
						ed::ObjectManagerItem* itemData = data->Objects.GetByTextureID(srvs[j]);
						std::string texName = itemData->Name;
						if (texName.find('\\') != std::string::npos || texName.find('/') != std::string::npos || texName.find('.') != std::string::npos)
							texName = std::filesystem::path(texName).filename().string();
						texName = getFilename(texName);

						renderSrc += indent + "glActiveTexture(GL_TEXTURE0 + " + std::to_string(j) + ");\n";
						if (itemData->Type == ObjectType::CubeMap)
							renderSrc += indent + "glBindTexture(GL_TEXTURE_CUBE_MAP, " + texName + ");\n";
						else if (itemData->Type == ObjectType::Image3D || itemData->Type == ObjectType::Texture3D)
							renderSrc += indent + "glBindTexture(GL_TEXTURE_3D, " + texName + ");\n";
						else if (itemData->Type == ObjectType::RenderTexture)
							renderSrc += indent + "glBindTexture(GL_TEXTURE_2D, " + texName + "_Color);\n";
						else
							renderSrc += indent + "glBindTexture(GL_TEXTURE_2D, " + texName + ");\n";

						std::string unitName = pass->Variables.GetSamplerList()[j];
						renderSrc += indent + "glUniform1i(glGetUniformLocation(" + std::string(pipeItems[i]->Name) + "_SP, \"" + unitName + "\"), " + std::to_string(j) + ");\n\n";
					}

					// bind variables
					const auto& vars = pass->Variables.GetVariables();
					for (const auto& var : vars) {
						if (var->System != ed::SystemShaderVariable::GeometryTransform) {
							renderSrc += indent + bindVariable(var, std::string(pipeItems[i]->Name)) + "\n";
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
									if (geoData->Type == pipe::GeometryItem::GeometryType::Rectangle) {
										renderSrc += indent + "sysGeometryTransform = glm::translate(glm::mat4(1), glm::vec3((" + std::to_string(geoData->Position.x) + "f + 0.5f) * sedWindowWidth, (" + std::to_string(geoData->Position.y) + "f + 0.5f) * sedWindowHeight, -1000.0f)) *" + "glm::yawPitchRoll(" + std::to_string(geoData->Rotation.y) + "f, " + std::to_string(geoData->Rotation.x) + "f, " + std::to_string(geoData->Rotation.z) + "f) * " + "glm::scale(glm::mat4(1.0f), glm::vec3(" + std::to_string(geoData->Scale.x) + "f * sedWindowWidth, " + std::to_string(geoData->Scale.y) + "f * sedWindowHeight, 1.0f));\n";
									} else {
										renderSrc += indent + "sysGeometryTransform = glm::translate(glm::mat4(1), glm::vec3(" + std::to_string(geoData->Position.x) + "f, " + std::to_string(geoData->Position.y) + "f, " + std::to_string(geoData->Position.z) + "f)) *" + "glm::yawPitchRoll(" + std::to_string(geoData->Rotation.y) + "f, " + std::to_string(geoData->Rotation.x) + "f, " + std::to_string(geoData->Rotation.z) + "f) * " + "glm::scale(glm::mat4(1.0f), glm::vec3(" + std::to_string(geoData->Scale.x) + "f, " + std::to_string(geoData->Scale.y) + "f, " + std::to_string(geoData->Scale.z) + "f));\n";
									}
									renderSrc += indent + "glUniformMatrix4fv(glGetUniformLocation(" + std::string(pipeItems[i]->Name) + "_SP, \"" + std::string(var->Name) + "\"), 1, GL_FALSE, glm::value_ptr(sysGeometryTransform));\n";
									break;
								}
							}
							renderSrc += indent + "glBindVertexArray(" + pName + "_VAO);\n";
							if (geoData->Instanced)
								renderSrc += indent + "glDrawArraysInstanced(" + getTopologyName(geoData->Topology) + ", 0, " + std::to_string(eng::GeometryFactory::VertexCount[geoData->Type]) + ", " + std::to_string(geoData->InstanceCount) + ");\n";
							else
								renderSrc += indent + "glDrawArrays(" + getTopologyName(geoData->Topology) + ", 0, " + std::to_string(eng::GeometryFactory::VertexCount[geoData->Type]) + ");\n";
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

		return true;
	}
}
