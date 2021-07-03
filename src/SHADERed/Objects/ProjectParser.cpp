#include <SHADERed/Objects/CameraSnapshots.h>
#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/FunctionVariableManager.h>
#include <SHADERed/Objects/InputLayout.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <SHADERed/Objects/PluginManager.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/RenderEngine.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>

#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/PinnedUI.h>
#include <SHADERed/UI/PipelineUI.h>
#include <SHADERed/UI/PropertyUI.h>

#include <filesystem>
#include <algorithm>
#include <fstream>

#define HARRAYSIZE(a) (sizeof(a) / sizeof(*a))

namespace ed {
	std::string getExtension(const std::string& filename)
	{
		switch (ShaderCompiler::GetShaderLanguageFromExtension(filename)) {
		case ShaderLanguage::HLSL: return Settings::Instance().General.HLSLExtensions[0];
		case ShaderLanguage::GLSL: return "glsl";
		case ShaderLanguage::VulkanGLSL: return Settings::Instance().General.VulkanGLSLExtensions[0];
		case ShaderLanguage::Plugin: return filename.substr(filename.find_last_of('.') + 1);
		}

		return "glsl";
	}
	std::string getInnerXML(const pugi::xml_node& node)
	{
		std::ostringstream pxmlStream;
		for (pugi::xml_node_iterator it = node.begin(); it != node.end(); ++it)
			it->print(pxmlStream);

		return pxmlStream.str();
	}

	std::string toGenericPath(const std::string& p)
	{
		std::string ret = p;
		std::replace(ret.begin(), ret.end(), '\\', '/');
		return ret;
	}
	std::string newShaderFilename(const std::string& proj, const std::string& shaderpass, const std::string& stage, const std::string& ext)
	{
		return proj + "_" + shaderpass + stage + "." + ext; // eg: project_SimpleVS.glsl
	}

	ProjectParser::ProjectParser(PipelineManager* pipeline, ObjectManager* objects, RenderEngine* rend, PluginManager* plugins, MessageStack* msgs, DebugInformation* debugger, GUIManager* gui)
			: m_pipe(pipeline)
			, m_file("")
			, m_renderer(rend)
			, m_objects(objects)
			, m_msgs(msgs)
			, m_plugins(plugins)
			, m_debug(debugger)
	{
		ResetProjectDirectory();
		m_ui = gui;
	}
	ProjectParser::~ProjectParser()
	{
	}
	void ProjectParser::Open(const std::string& file)
	{
		Logger::Get().Log("Opening a project file " + file);

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(file.c_str());
		if (!result) {
			Logger::Get().Log("Failed to parse a project file", true);
			return;
		}

		// check if user has all required plugins
		m_pluginList.clear();
		bool pluginTest = true;
		pugi::xml_node pluginsContainerNode = doc.child("project").child("plugins");
		for (pugi::xml_node pluginNode : pluginsContainerNode.children("entry")) {
			std::string pname = pluginNode.attribute("name").as_string();
			int pver = pluginNode.attribute("ver").as_int();
			bool required = true;
			
			if (!pluginNode.attribute("required").empty())
				required = pluginNode.attribute("required").as_bool();

			IPlugin1* plugin = m_plugins->GetPlugin(pname);
			
			if (plugin)
				m_pluginList.push_back(pname);

			if (plugin == nullptr && required) {
				pluginTest = false;

				std::string msg = "The project you are trying to open requires plugin \"" + pname + "\".";

				const SDL_MessageBoxButtonData buttons[] = {
					{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK" },
				};
				const SDL_MessageBoxData messageboxdata = {
					SDL_MESSAGEBOX_INFORMATION, /* .flags */
					m_ui->GetSDLWindow(),		/* .window */
					"SHADERed",					/* .title */
					msg.c_str(),				/* .message */
					SDL_arraysize(buttons),		/* .numbuttons */
					buttons,					/* .buttons */
					NULL						/* .colorScheme */
				};
				int buttonid;
				if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) { }

				if (buttonid == 0) { }

				pluginTest = false;
			} else {
				if (plugin) {
					int instPVer = m_plugins->GetPluginVersion(pname);
					if (instPVer < pver && !plugin->IsVersionCompatible(instPVer)) {
						pluginTest = false;

						std::string msg = "The project you are trying to open requires plugin " + pname + " version " + std::to_string(pver) + " while you have version " + std::to_string(instPVer) + " installed.\n";

						const SDL_MessageBoxButtonData buttons[] = {
							{ /* .flags, .buttonid, .text */ 0, 1, "NO" },
							{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "YES" },
						};
						const SDL_MessageBoxData messageboxdata = {
							SDL_MESSAGEBOX_INFORMATION, /* .flags */
							m_ui->GetSDLWindow(),		/* .window */
							"SHADERed",					/* .title */
							msg.c_str(),				/* .message */
							SDL_arraysize(buttons),		/* .numbuttons */
							buttons,					/* .buttons */
							NULL						/* .colorScheme */
						};
						int buttonid;
						if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {}

						if (buttonid == 0) {
							// TODO: redirect to .../plugin?name=pname
						}

						break;
					}
				}
			}
		}

		if (!pluginTest) {
			Logger::Get().Log("Missing plugin - project not loaded", true);
			return;
		}

		CameraSnapshots::Clear();

		m_file = file;
		SetProjectDirectory(file.substr(0, file.find_last_of("/\\")));

		m_msgs->Clear();
		m_debug->ClearPixelList();
		m_debug->ClearWatchList();
		m_debug->ClearVectorWatchList();
		m_debug->ClearBreakpointList();

		for (auto& mdl : m_models) {
			delete mdl.second;
			mdl.second = nullptr;
		}
		m_models.clear();

		m_pipe->Clear();
		m_objects->Clear();

		Settings::Instance().Project.FPCamera = false;
		Settings::Instance().Project.ClearColor = glm::vec4(0, 0, 0, 0);
		Settings::Instance().Project.UseAlphaChannel = false;

		pugi::xml_node projectNode = doc.child("project");
		int projectVersion = 1; // if no project version is specified == using first project file
		if (!projectNode.attribute("version").empty())
			projectVersion = projectNode.attribute("version").as_int();

		// notify plugins that we've finished with loading
		for (const auto& pname : m_pluginList)
			m_plugins->GetPlugin(pname)->Project_BeginLoad();

		switch (projectVersion) {
		case 1: m_parseV1(projectNode); break;
		case 2: m_parseV2(projectNode); break;
		default:
			Logger::Get().Log("Tried to open a project that is newer version", true);
			break;
		}

		m_modified = false;

		// reset time, frame index, etc...
		SystemVariableManager::Instance().Reset();

		// notify plugins that we've finished with loading
		for (const auto& pname : m_pluginList)
			m_plugins->GetPlugin(pname)->Project_EndLoad();

		Logger::Get().Log("Finished with parsing a project file");
	}
	void ProjectParser::OpenTemplate()
	{
		Open(std::filesystem::current_path().generic_string() + "/templates/" + m_template + "/template.sprj");
		m_file = ""; // disallow overwriting template.sprj project file
	}
	void ProjectParser::Save()
	{
		SaveAs(m_file);
	}
	void ProjectParser::SaveAs(const std::string& file, bool copyFiles)
	{
		Logger::Get().Log("Saving project file...");

		m_pluginList.clear();
		m_modified = false;
		m_file = file;
		std::string oldProjectPath = m_projectPath;
		SetProjectDirectory(file.substr(0, file.find_last_of("/\\")));

		std::vector<PipelineItem*> passItems = m_pipe->GetList();
		std::vector<pipe::ShaderPass*> collapsedSP = ((PipelineUI*)m_ui->Get(ViewID::Pipeline))->GetCollapsedItems();

		std::string projectStem = "proj";
		if (std::filesystem::path(file).has_stem())
			projectStem = std::filesystem::path(file).stem().string();

		// copy shader files to a directory
		std::string shadersDir = m_projectPath + "/shaders";
		if (copyFiles) {
			Logger::Get().Log("Copying shader files...");

			std::filesystem::create_directories(shadersDir);
			std::error_code errc;

			std::string proj = oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/");

			for (PipelineItem* passItem : passItems) {
				if (passItem->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* passData = (pipe::ShaderPass*)passItem->Data;

					std::string vs = std::filesystem::path(passData->VSPath).is_absolute() ? passData->VSPath : (proj + std::string(passData->VSPath));
					std::string ps = std::filesystem::path(passData->PSPath).is_absolute() ? passData->PSPath : (proj + std::string(passData->PSPath));

					std::string vsExt = getExtension(vs);
					std::string psExt = getExtension(ps);

					std::filesystem::copy_file(vs, shadersDir + "/" + newShaderFilename(projectStem, passItem->Name, "VS", vsExt), std::filesystem::copy_options::overwrite_existing, errc);
					std::filesystem::copy_file(ps, shadersDir + "/" + newShaderFilename(projectStem, passItem->Name, "PS", psExt), std::filesystem::copy_options::overwrite_existing, errc);

					if (passData->GSUsed) {
						std::string gs = std::filesystem::path(passData->GSPath).is_absolute() ? passData->GSPath : (proj + std::string(passData->GSPath));
						std::string gsExt = getExtension(gs);

						std::filesystem::copy_file(gs, shadersDir + "/" + newShaderFilename(projectStem, passItem->Name, "GS", gsExt), std::filesystem::copy_options::overwrite_existing, errc);
					}

					if (passData->TSUsed) {
						// tcs
						std::string tcs = std::filesystem::path(passData->TCSPath).is_absolute() ? passData->TCSPath : (proj + std::string(passData->TCSPath));
						std::string tcsExt = getExtension(tcs);
						std::filesystem::copy_file(tcs, shadersDir + "/" + newShaderFilename(projectStem, passItem->Name, "TCS", tcsExt), std::filesystem::copy_options::overwrite_existing, errc);
					
						// tes
						std::string tes = std::filesystem::path(passData->TESPath).is_absolute() ? passData->TESPath : (proj + std::string(passData->TESPath));
						std::string tesExt = getExtension(tes);
						std::filesystem::copy_file(tes, shadersDir + "/" + newShaderFilename(projectStem, passItem->Name, "TES", tesExt), std::filesystem::copy_options::overwrite_existing, errc);
					}

					if (errc)
						ed::Logger::Get().Log("Failed to copy a file (source == destination)", true);
				} else if (passItem->Type == PipelineItem::ItemType::ComputePass) {
					pipe::ComputePass* passData = (pipe::ComputePass*)passItem->Data;

					std::string cs = std::filesystem::path(passData->Path).is_absolute() ? passData->Path : (proj + std::string(passData->Path));
					std::string csExt = getExtension(cs);

					std::filesystem::copy_file(cs, shadersDir + "/" + newShaderFilename(projectStem, passItem->Name, "CS", csExt), std::filesystem::copy_options::overwrite_existing, errc);
					if (errc)
						ed::Logger::Get().Log("Failed to copy a file (source == destination)", true);
				} else if (passItem->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass* passData = (pipe::AudioPass*)passItem->Data;

					std::string ss = std::filesystem::path(passData->Path).is_absolute() ? passData->Path : (proj + std::string(passData->Path));
					std::string ssExt = getExtension(ss);

					std::filesystem::copy_file(ss, shadersDir + "/" + newShaderFilename(projectStem, passItem->Name, "SS", ssExt), std::filesystem::copy_options::overwrite_existing, errc);
					if (errc)
						ed::Logger::Get().Log("Failed to copy a file (source == destination)", true);
				} else if (passItem->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* pdata = (pipe::PluginItemData*)passItem->Data;
					m_addPlugin(m_plugins->GetPluginName(pdata->Owner));
				}
			}

			for (const auto& pname : m_pluginList)
				m_plugins->GetPlugin(pname)->Project_CopyFilesOnSave(m_projectPath.c_str());
		}

		pugi::xml_document doc;
		pugi::xml_node projectNode = doc.append_child("project");
		projectNode.append_attribute("version").set_value(2);
		pugi::xml_node pipelineNode = projectNode.append_child("pipeline");
		pugi::xml_node objectsNode = projectNode.append_child("objects");
		pugi::xml_node camsnapsNode = projectNode.append_child("cameras");
		pugi::xml_node settingsNode = projectNode.append_child("settings");
		pugi::xml_node pluginDataNode = projectNode.append_child("plugindata");

		// shader passes
		for (PipelineItem* passItem : passItems) {
			pugi::xml_node passNode = pipelineNode.append_child("pass");
			passNode.append_attribute("name").set_value(passItem->Name);

			if (passItem->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* passData = (pipe::ShaderPass*)passItem->Data;

				passNode.append_attribute("type").set_value("shader");
				passNode.append_attribute("active").set_value(passData->Active);
				passNode.append_attribute("patchverts").set_value(passData->TSPatchVertices);

				/* collapsed="true" attribute */
				for (int i = 0; i < collapsedSP.size(); i++)
					if (collapsedSP[i] == passData) {
						passNode.append_attribute("collapsed").set_value(true);
						break;
					}

				// vertex shader
				pugi::xml_node vsNode = passNode.append_child("shader");
				std::string relativePath = (std::filesystem::path(passData->VSPath).is_absolute()) ? passData->VSPath : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(passData->VSPath));
				if (copyFiles) {
					std::string vsExt = getExtension(passData->VSPath);
					relativePath = "shaders/" + newShaderFilename(projectStem, passItem->Name, "VS", vsExt);
				}

				vsNode.append_attribute("type").set_value("vs");
				vsNode.append_attribute("path").set_value(relativePath.c_str());
				vsNode.append_attribute("entry").set_value(passData->VSEntry);

				// pixel shader
				pugi::xml_node psNode = passNode.append_child("shader");
				relativePath = (std::filesystem::path(passData->PSPath).is_absolute()) ? passData->PSPath : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(passData->PSPath));
				if (copyFiles) {
					std::string psExt = getExtension(passData->PSPath);
					relativePath = "shaders/" + newShaderFilename(projectStem, passItem->Name, "PS", psExt);
				}

				psNode.append_attribute("type").set_value("ps");
				psNode.append_attribute("path").set_value(relativePath.c_str());
				psNode.append_attribute("entry").set_value(passData->PSEntry);

				// geometry shader
				if (strlen(passData->GSEntry) > 0 && strlen(passData->GSPath) > 0) {
					pugi::xml_node gsNode = passNode.append_child("shader");
					relativePath = (std::filesystem::path(passData->GSPath).is_absolute()) ? passData->GSPath : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(passData->GSPath));
					if (copyFiles) {
						std::string gsExt = getExtension(passData->GSPath);
						relativePath = "shaders/" + newShaderFilename(projectStem, passItem->Name, "GS", gsExt);
					}

					gsNode.append_attribute("used").set_value(passData->GSUsed);

					gsNode.append_attribute("type").set_value("gs");
					gsNode.append_attribute("path").set_value(relativePath.c_str());
					gsNode.append_attribute("entry").set_value(passData->GSEntry);
				}

				// TCS shader
				if (strlen(passData->TCSEntry) > 0 && strlen(passData->TCSPath) > 0) {
					pugi::xml_node tcsNode = passNode.append_child("shader");
					relativePath = (std::filesystem::path(passData->TCSPath).is_absolute()) ? passData->TCSPath : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(passData->TCSPath));
					if (copyFiles) {
						std::string tcsExt = getExtension(passData->TCSPath);
						relativePath = "shaders/" + newShaderFilename(projectStem, passItem->Name, "TCS", tcsExt);
					}

					tcsNode.append_attribute("used").set_value(passData->TSUsed);

					tcsNode.append_attribute("type").set_value("tcs");
					tcsNode.append_attribute("path").set_value(relativePath.c_str());
					tcsNode.append_attribute("entry").set_value(passData->TCSEntry);
				}

				// TES shader
				if (strlen(passData->TESEntry) > 0 && strlen(passData->TESPath) > 0) {
					pugi::xml_node tesNode = passNode.append_child("shader");
					relativePath = (std::filesystem::path(passData->TESPath).is_absolute()) ? passData->TESPath : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(passData->TESPath));
					if (copyFiles) {
						std::string tesExt = getExtension(passData->TESPath);
						relativePath = "shaders/" + newShaderFilename(projectStem, passItem->Name, "TES", tesExt);
					}

					tesNode.append_attribute("used").set_value(passData->TSUsed);

					tesNode.append_attribute("type").set_value("tes");
					tesNode.append_attribute("path").set_value(relativePath.c_str());
					tesNode.append_attribute("entry").set_value(passData->TESEntry);
				}

				/* vs input layout */
				pugi::xml_node iLayout = passNode.append_child("inputlayout");
				for (auto& iteminp : passData->InputLayout) {
					pugi::xml_node lItem = iLayout.append_child("item");
					lItem.append_attribute("value").set_value(ATTRIBUTE_VALUE_NAMES[(int)iteminp.Value]);

					if (iteminp.Semantic.size() > 0)
						lItem.append_attribute("semantic").set_value(iteminp.Semantic.c_str());
				}

				/* render textures */
				for (int i = 0; i < MAX_RENDER_TEXTURES; i++) {
					if (passData->RenderTextures[i] == 0)
						break;
					GLuint rtID = passData->RenderTextures[i];
					if (rtID == m_renderer->GetTexture())
						passNode.append_child("rendertexture");
					else
						passNode.append_child("rendertexture").append_attribute("name").set_value(m_objects->GetByTextureID(rtID)->Name.c_str());
				}

				// pass items
				pugi::xml_node itemsNode = passNode.append_child("items");
				m_exportItems(itemsNode, passData->Items, oldProjectPath);

				// item variable values
				pugi::xml_node itemValuesNode = passNode.append_child("itemvalues");
				std::vector<RenderEngine::ItemVariableValue> itemValues = m_renderer->GetItemVariableValues();
				std::vector<ShaderVariable*>& vars = passData->Variables.GetVariables();
				for (const auto& itemVal : itemValues) {
					bool found = false;
					for (const auto& passChild : passData->Items)
						if (passChild == itemVal.Item) {
							found = true;
							break;
						}
					if (!found)
						continue;

					pugi::xml_node itemValueNode = itemValuesNode.append_child("value");

					itemValueNode.append_attribute("variable").set_value(itemVal.Variable->Name);
					itemValueNode.append_attribute("for").set_value(itemVal.Item->Name);

					m_exportVariableValue(itemValueNode, itemVal.NewValue);
				}

				// variables -> now global in pass element [V2]
				m_exportShaderVariables(passNode, passData->Variables.GetVariables());

				// macros
				pugi::xml_node macrosNode = passNode.append_child("macros");
				for (auto& macro : passData->Macros) {
					pugi::xml_node macroNode = macrosNode.append_child("define");
					macroNode.append_attribute("name").set_value(macro.Name);
					macroNode.append_attribute("active").set_value(macro.Active);
					macroNode.text().set(macro.Value);
				}
			} else if (passItem->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* passData = (pipe::ComputePass*)passItem->Data;

				passNode.append_attribute("type").set_value("compute");
				passNode.append_attribute("active").set_value(passData->Active);

				// compute shader
				pugi::xml_node csNode = passNode.append_child("shader");
				std::string relativePath = (std::filesystem::path(passData->Path).is_absolute()) ? passData->Path : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(passData->Path));
				if (copyFiles) {
					std::string csExt = getExtension(passData->Path);
					relativePath = "shaders/" + newShaderFilename(projectStem, passItem->Name, "CS", csExt);
				}

				csNode.append_attribute("type").set_value("cs");
				csNode.append_attribute("path").set_value(relativePath.c_str());
				csNode.append_attribute("entry").set_value(passData->Entry);

				// group size
				pugi::xml_node workNode = passNode.append_child("groupsize");
				workNode.append_attribute("x").set_value(passData->WorkX);
				workNode.append_attribute("y").set_value(passData->WorkY);
				workNode.append_attribute("z").set_value(passData->WorkZ);

				// variables -> now global in pass element [V2]
				m_exportShaderVariables(passNode, passData->Variables.GetVariables());

				// macros
				pugi::xml_node macrosNode = passNode.append_child("macros");
				for (auto& macro : passData->Macros) {
					pugi::xml_node macroNode = macrosNode.append_child("define");
					macroNode.append_attribute("name").set_value(macro.Name);
					macroNode.append_attribute("active").set_value(macro.Active);
					macroNode.text().set(macro.Value);
				}
			} else if (passItem->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* passData = (pipe::AudioPass*)passItem->Data;

				passNode.append_attribute("type").set_value("audio");

				// compute shader
				pugi::xml_node ssNode = passNode.append_child("shader");
				std::string relativePath = (std::filesystem::path(passData->Path).is_absolute()) ? passData->Path : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(passData->Path));
				if (copyFiles) {
					std::string ssExt = getExtension(passData->Path);
					relativePath = "shaders/" + newShaderFilename(projectStem, passItem->Name, "SS", ssExt);
				}

				ssNode.append_attribute("type").set_value("ss");
				ssNode.append_attribute("path").set_value(relativePath.c_str());

				// variables -> now global in pass element [V2]
				m_exportShaderVariables(passNode, passData->Variables.GetVariables());

				// macros
				pugi::xml_node macrosNode = passNode.append_child("macros");
				for (auto& macro : passData->Macros) {
					pugi::xml_node macroNode = macrosNode.append_child("define");
					macroNode.append_attribute("name").set_value(macro.Name);
					macroNode.append_attribute("active").set_value(macro.Active);
					macroNode.text().set(macro.Value);
				}
			} else if (passItem->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* plData = (pipe::PluginItemData*)passItem->Data;
				m_addPlugin(m_plugins->GetPluginName(plData->Owner));

				passNode.append_attribute("type").set_value("plugin");
				passNode.append_attribute("plugin").set_value(m_plugins->GetPluginName(plData->Owner).c_str());
				passNode.append_attribute("itemtype").set_value(plData->Type);

				if (plData->Items.size() > 0) {
					pugi::xml_node itemsNode = passNode.append_child("items");
					m_exportItems(itemsNode, plData->Items, oldProjectPath);
				}

				pugi::xml_node dataNode = passNode.append_child("data");
				const char* pObjectSrc = plData->Owner->PipelineItem_Export(plData->Type, plData->PluginData);
				dataNode.append_buffer(pObjectSrc, strlen(pObjectSrc));
			}
		}

		// camera snapshots
		{
			auto& names = CameraSnapshots::GetList();
			for (const auto& name : names) {
				pugi::xml_node camsnapNode = camsnapsNode.append_child("camera");
				camsnapNode.append_attribute("name").set_value(name.c_str());

				pugi::xml_node valueRowNode = camsnapNode.append_child("row");
				glm::mat4 cammat = CameraSnapshots::Get(name);
				int rowID = 0;
				for (int i = 0; i < 16; i++) {
					valueRowNode.append_child("value").text().set(cammat[i % 4][rowID]);

					if ((i + 1) % 4 == 0 && i != 0) {
						valueRowNode = camsnapNode.append_child("row");
						rowID++;
					}
				}
			}
		}

		// objects
		{
			// textures & buffers
			std::vector<ObjectManagerItem*> texs = m_objects->GetObjects();
			for (int i = 0; i < texs.size(); i++) {
				ObjectManagerItem* item = texs[i];

				bool isRT = (item->Type == ObjectType::RenderTexture);
				bool isAudio = (item->Type == ObjectType::Audio);
				bool isCube = (item->Type == ObjectType::CubeMap);
				bool isBuffer = (item->Type == ObjectType::Buffer);
				bool isImage = (item->Type == ObjectType::Image);
				bool isImage3D = (item->Type == ObjectType::Image3D);
				bool isPluginOwner = (item->Type == ObjectType::PluginObject);
				bool isTexture = (item->Type == ObjectType::Texture);
				bool isTexture3D = (item->Type == ObjectType::Texture3D);
				bool isKeyboardTexture = (item->Type == ObjectType::KeyboardTexture);
				
				std::string texOutPath = item->Name;
				if ((isTexture && !isKeyboardTexture) || isAudio)
					texOutPath = GetTexturePath(item->Name, oldProjectPath);
				
				std::string typeName = "texture";
				if (isBuffer) typeName = "buffer";
				else if (isRT) typeName = "rendertexture";
				else if (isAudio) typeName = "audio";
				else if (isImage) typeName = "image";
				else if (isImage3D) typeName = "image3d";
				else if (isPluginOwner) typeName = "pluginobject";

				pugi::xml_node textureNode = objectsNode.append_child("object");
				textureNode.append_attribute("type").set_value(typeName.c_str());
				textureNode.append_attribute(((isTexture && !isKeyboardTexture) || isTexture3D || isAudio) ? "path" : "name").set_value(texOutPath.c_str());

				if (isRT) {
					ed::RenderTextureObject* rtObj = item->RT;

					if (rtObj->Format != GL_RGBA)
						textureNode.append_attribute("format").set_value(gl::String::Format(rtObj->Format));

					if (rtObj->FixedSize.x != -1)
						textureNode.append_attribute("fsize").set_value((std::to_string(rtObj->FixedSize.x) + "," + std::to_string(rtObj->FixedSize.y)).c_str());
					else
						textureNode.append_attribute("rsize").set_value((std::to_string(rtObj->RatioSize.x) + "," + std::to_string(rtObj->RatioSize.y)).c_str());

					textureNode.append_attribute("clear").set_value(rtObj->Clear);
					if (rtObj->ClearColor.r != 0) textureNode.append_attribute("r").set_value(rtObj->ClearColor.r);
					if (rtObj->ClearColor.g != 0) textureNode.append_attribute("g").set_value(rtObj->ClearColor.g);
					if (rtObj->ClearColor.b != 0) textureNode.append_attribute("b").set_value(rtObj->ClearColor.b);
					if (rtObj->ClearColor.a != 0) textureNode.append_attribute("a").set_value(rtObj->ClearColor.a);
				}

				if (isCube) {
					const std::vector<std::string>& texmaps = item->CubemapPaths;

					textureNode.append_attribute("cube").set_value(isCube);

					textureNode.append_attribute("left").set_value(GetTexturePath(texmaps[0], oldProjectPath).c_str());
					textureNode.append_attribute("top").set_value(GetTexturePath(texmaps[1], oldProjectPath).c_str());
					textureNode.append_attribute("front").set_value(GetTexturePath(texmaps[2], oldProjectPath).c_str());
					textureNode.append_attribute("bottom").set_value(GetTexturePath(texmaps[3], oldProjectPath).c_str());
					textureNode.append_attribute("right").set_value(GetTexturePath(texmaps[4], oldProjectPath).c_str());
					textureNode.append_attribute("back").set_value(GetTexturePath(texmaps[5], oldProjectPath).c_str());
				}

				if ((isTexture && !isKeyboardTexture) || isTexture3D) {
					textureNode.append_attribute("vflip").set_value(item->Texture_VFlipped);
					textureNode.append_attribute("min_filter").set_value(ed::gl::String::TextureMinFilter(item->Texture_MinFilter));
					textureNode.append_attribute("mag_filter").set_value(ed::gl::String::TextureMagFilter(item->Texture_MagFilter));
					textureNode.append_attribute("wrap_s").set_value(ed::gl::String::TextureWrap(item->Texture_WrapS));
					textureNode.append_attribute("wrap_t").set_value(ed::gl::String::TextureWrap(item->Texture_WrapT));

					if (isTexture3D) {
						textureNode.append_attribute("is_3d").set_value(true);
						textureNode.append_attribute("wrap_r").set_value(ed::gl::String::TextureWrap(item->Texture_WrapR));
					}
				}

				if (isKeyboardTexture)
					textureNode.append_attribute("keyboard_texture").set_value(isKeyboardTexture);

				if (isImage) {
					ImageObject* iobj = item->Image;

					if (iobj->DataPath[0] != 0)
						textureNode.append_attribute("data").set_value(GetTexturePath(iobj->DataPath, oldProjectPath).c_str());

					textureNode.append_attribute("width").set_value(iobj->Size.x);
					textureNode.append_attribute("height").set_value(iobj->Size.y);
					textureNode.append_attribute("format").set_value(gl::String::Format(iobj->Format));
				}
				if (isImage3D) {
					Image3DObject* iobj = item->Image3D;

					textureNode.append_attribute("width").set_value(iobj->Size.x);
					textureNode.append_attribute("height").set_value(iobj->Size.y);
					textureNode.append_attribute("depth").set_value(iobj->Size.z);
					textureNode.append_attribute("format").set_value(gl::String::Format(iobj->Format));
				}

				PluginObject* pluginObj = item->Plugin;
				bool isPluginObjectUAV = isPluginOwner && pluginObj->Owner->Object_IsBindableUAV(pluginObj->Type);

				if (isPluginOwner) {
					m_addPlugin(m_plugins->GetPluginName(pluginObj->Owner));

					textureNode.append_attribute("plugin").set_value(m_plugins->GetPluginName(pluginObj->Owner).c_str());
					textureNode.append_attribute("objecttype").set_value(pluginObj->Type);

					const char* pObjectSrc = pluginObj->Owner->Object_Export(pluginObj->Type, pluginObj->Data, pluginObj->ID);

					if (pObjectSrc != nullptr)
						textureNode.append_buffer(pObjectSrc, strlen(pObjectSrc));
				}

				if (isBuffer) {
					ed::BufferObject* bobj = item->Buffer;

					textureNode.append_attribute("size").set_value(bobj->Size);
					textureNode.append_attribute("format").set_value(bobj->ViewFormat);
					textureNode.append_attribute("pausedpreview").set_value(bobj->PreviewPaused);

					std::string bPath = GetProjectPath("buffers/" + item->Name + ".buf");
					if (!std::filesystem::exists(GetProjectPath("buffers")))
						std::filesystem::create_directories(GetProjectPath("buffers"));

					std::ofstream bufWrite(bPath, std::ios::binary);
					bufWrite.write((char*)bobj->Data, bobj->Size);
					bufWrite.close();

					for (int j = 0; j < passItems.size(); j++) {
						const std::vector<GLuint>& bound = m_objects->GetUniformBindList(passItems[j]);

						for (int slot = 0; slot < bound.size(); slot++) {
							if (bound[slot] == bobj->ID) {
								pugi::xml_node bindNode = textureNode.append_child("bind");
								bindNode.append_attribute("slot").set_value(slot);
								bindNode.append_attribute("name").set_value(passItems[j]->Name);
							}
						}
					}
				} else if (isImage || isImage3D || isPluginObjectUAV) {
					for (int j = 0; j < passItems.size(); j++) {

						GLuint myTex = item->Texture;

						// as image2D
						const std::vector<GLuint>& boundUBO = m_objects->GetUniformBindList(passItems[j]);
						for (int slot = 0; slot < boundUBO.size(); slot++) {
							if (boundUBO[slot] == myTex) {
								pugi::xml_node bindNode = textureNode.append_child("bind");
								bindNode.append_attribute("slot").set_value(slot);
								bindNode.append_attribute("name").set_value(passItems[j]->Name);
								bindNode.append_attribute("uav").set_value(1);
							}
						}

						// as sampler2D
						std::vector<GLuint> bound = m_objects->GetBindList(passItems[j]);
						for (int slot = 0; slot < bound.size(); slot++)
							if (bound[slot] == myTex) {
								pugi::xml_node bindNode = textureNode.append_child("bind");
								bindNode.append_attribute("slot").set_value(slot);
								bindNode.append_attribute("name").set_value(passItems[j]->Name);
								bindNode.append_attribute("uav").set_value(0);
							}
					}
				} else {
					GLuint myTex = item->Texture;
					if (isPluginOwner)
						myTex = pluginObj->ID;

					for (int j = 0; j < passItems.size(); j++) {
						std::vector<GLuint> bound = m_objects->GetBindList(passItems[j]);

						for (int slot = 0; slot < bound.size(); slot++)
							if (bound[slot] == myTex) {
								pugi::xml_node bindNode = textureNode.append_child("bind");
								bindNode.append_attribute("slot").set_value(slot);
								bindNode.append_attribute("name").set_value(passItems[j]->Name);
							}
					}
				}
			}
		}

		// settings
		{
			// property ui
			PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
			if (props->HasItemSelected()) {
				std::string name = props->CurrentItemName();

				pugi::xml_node propNode = settingsNode.append_child("entry");
				propNode.append_attribute("type").set_value("property");
				propNode.append_attribute("name").set_value(name.c_str());
				propNode.append_attribute("item").set_value(props->IsPipelineItem() ? "pipe" : (props->IsRenderTexture() ? "rt" : (props->IsImage() ? "image" : "image3D")));
			}

			// code editor ui
			CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get(ViewID::Code));
			std::vector<std::pair<std::string, ShaderStage>> files = editor->GetOpenedFiles();
			for (const auto& file : files) {
				pugi::xml_node fileNode = settingsNode.append_child("entry");
				fileNode.append_attribute("type").set_value("file");
				fileNode.append_attribute("name").set_value(file.first.c_str());

				std::string shaderAbbr = "vs";
				if (file.second == ShaderStage::Pixel) shaderAbbr = "ps";
				if (file.second == ShaderStage::Geometry) shaderAbbr = "gs";
				if (file.second == ShaderStage::TessellationControl) shaderAbbr = "tcs";
				if (file.second == ShaderStage::TessellationEvaluation) shaderAbbr = "tes";
				if (file.second == ShaderStage::Compute) shaderAbbr = "cs";
				if (file.second == ShaderStage::Audio) shaderAbbr = "as";

				fileNode.append_attribute("shader").set_value(shaderAbbr.c_str());
			}

			// pinned ui
			PinnedUI* pinned = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
			std::vector<ShaderVariable*> pinnedVars = pinned->GetAll();
			for (const auto& var : pinnedVars) {
				pugi::xml_node varNode = settingsNode.append_child("entry");
				varNode.append_attribute("type").set_value("pinned");
				varNode.append_attribute("name").set_value(var->Name);

				for (PipelineItem* passItem : passItems) {
					bool found = false;

					std::vector<ShaderVariable*> vars;
					if (passItem->Type == PipelineItem::ItemType::ShaderPass) {
						pipe::ShaderPass* data = (pipe::ShaderPass*)passItem->Data;
						vars = data->Variables.GetVariables();
					} else if (passItem->Type == PipelineItem::ItemType::ComputePass) {
						pipe::ComputePass* data = (pipe::ComputePass*)passItem->Data;
						vars = data->Variables.GetVariables();
					} else if (passItem->Type == PipelineItem::ItemType::AudioPass) {
						pipe::AudioPass* data = (pipe::AudioPass*)passItem->Data;
						vars = data->Variables.GetVariables();
					}

					for (int i = 0; i < vars.size(); i++) {
						if (vars[i] == var) {
							varNode.append_attribute("owner").set_value(passItem->Name);
							found = true;
						}
					}

					if (found) break;
				}
			}

			// camera settings
			{
				if (Settings::Instance().Project.FPCamera) {
					ed::FirstPersonCamera* cam = (ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera();
					pugi::xml_node camNode = settingsNode.append_child("entry");
					camNode.append_attribute("type").set_value("camera");
					camNode.append_attribute("fp").set_value(true);

					glm::vec3 rota = cam->GetRotation();

					camNode.append_child("positionX").text().set(cam->GetPosition().x);
					camNode.append_child("positionY").text().set(cam->GetPosition().y);
					camNode.append_child("positionZ").text().set(cam->GetPosition().z);
					camNode.append_child("yaw").text().set(rota.x);
					camNode.append_child("pitch").text().set(rota.y);
				} else {
					ed::ArcBallCamera* cam = (ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera();
					pugi::xml_node camNode = settingsNode.append_child("entry");
					camNode.append_attribute("type").set_value("camera");
					camNode.append_attribute("fp").set_value(false);

					glm::vec3 rota = cam->GetRotation();

					camNode.append_child("distance").text().set(cam->GetDistance());
					camNode.append_child("pitch").text().set(rota.x);
					camNode.append_child("yaw").text().set(rota.y);
					camNode.append_child("roll").text().set(rota.z);
				}
			}

			ed::Settings& settings = Settings::Instance();

			// clear color
			{
				pugi::xml_node colorNode = settingsNode.append_child("entry");
				colorNode.append_attribute("type").set_value("clearcolor");
				colorNode.append_attribute("r").set_value(settings.Project.ClearColor.r);
				colorNode.append_attribute("g").set_value(settings.Project.ClearColor.g);
				colorNode.append_attribute("b").set_value(settings.Project.ClearColor.b);
				colorNode.append_attribute("a").set_value(settings.Project.ClearColor.a);
			}

			// usealpha
			{
				pugi::xml_node alphaNode = settingsNode.append_child("entry");
				alphaNode.append_attribute("type").set_value("usealpha");
				alphaNode.append_attribute("val").set_value(settings.Project.UseAlphaChannel);
			}

			// include paths
			if (settings.Project.IncludePaths.size() > 0) {
				pugi::xml_node pathsNode = settingsNode.append_child("entry");
				pathsNode.append_attribute("type").set_value("ipaths");

				for (int j = 0; j < settings.Project.IncludePaths.size(); j++) {
					pugi::xml_node ipath = pathsNode.append_child("path");
					ipath.text().set(settings.Project.IncludePaths[j].c_str());
				}
			}

			// watches
			const std::vector<char*>& watches = m_debug->GetWatchList();
			for (const auto& watch : watches) {
				pugi::xml_node watchNode = settingsNode.append_child("entry");
				watchNode.append_attribute("type").set_value("watch_expr");
				watchNode.append_attribute("expr").set_value(watch);
			}

			// vector watches
			const std::vector<char*>& vec_watches = m_debug->GetVectorWatchList();
			const std::vector<glm::vec4>& vec_watches_color = m_debug->GetVectorWatchColors();
			for (int i = 0; i < vec_watches.size(); i++) {
				pugi::xml_node watchNode = settingsNode.append_child("entry");
				watchNode.append_attribute("type").set_value("vecwatch_expr");
				watchNode.append_attribute("expr").set_value(vec_watches[i]);
				watchNode.append_attribute("color_r").set_value(vec_watches_color[i].r);
				watchNode.append_attribute("color_g").set_value(vec_watches_color[i].g);
				watchNode.append_attribute("color_b").set_value(vec_watches_color[i].b);
			}

			// breakpoints
			const auto& bkpts = m_debug->GetBreakpointList();
			auto bkptStates = m_debug->GetBreakpointStateList();
			for (const auto& bkpt : bkpts) {
				for (size_t i = 0; i < bkpt.second.size(); i++) {
					pugi::xml_node watchNode = settingsNode.append_child("entry");
					watchNode.append_attribute("type").set_value("bkpt");
					watchNode.append_attribute("file").set_value(bkpt.first.c_str());
					watchNode.append_attribute("line").set_value(bkpt.second[i].Line);
					watchNode.append_attribute("use_cond").set_value(bkpt.second[i].IsConditional);
					watchNode.append_attribute("cond").set_value(bkpt.second[i].Condition.c_str());
					watchNode.append_attribute("enabled").set_value(bkptStates[bkpt.first][i]);
				}
			}
		}

		for (auto& pl : m_plugins->Plugins()) {
			if (pl->Project_HasAdditionalData()) {
				const auto& plName = m_plugins->GetPluginName(pl);
				if (std::count(m_pluginList.begin(), m_pluginList.end(), plName) == 0)
					m_pluginList.push_back(plName);

				const char* data = pl->Project_ExportAdditionalData();

				pugi::xml_node entry = pluginDataNode.append_child("entry");
				entry.append_attribute("owner").set_value(plName.c_str());
				entry.append_buffer(data, strlen(data));
			}
		}

		if (m_pluginList.size() > 0) {
			pugi::xml_node pluginsNode = projectNode.append_child("plugins");
			for (const auto& pname : m_pluginList) {
				pugi::xml_node pnode = pluginsNode.append_child("entry");
				pnode.append_attribute("name").set_value(pname.c_str());
				pnode.append_attribute("ver").set_value(m_plugins->GetPluginVersion(pname));
				pnode.append_attribute("required").set_value(m_plugins->GetPlugin(pname)->IsRequired());
			}
		}

		doc.save_file(file.c_str());
	}
	std::string ProjectParser::LoadFile(const std::string& file)
	{
		std::ifstream in(file);
		if (in.is_open()) {
			in.seekg(0, std::ios::beg);

			std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
			in.close();
			return content;
		}
		return "";
	}
	std::string ProjectParser::LoadProjectFile(const std::string& file)
	{
		std::ifstream in(GetProjectPath(file));
		if (in.is_open()) {
			in.seekg(0, std::ios::beg);

			std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
			in.close();
			return content;
		}
		return "";
	}
	char* ProjectParser::LoadProjectFile(const std::string& file, size_t& fsize)
	{
		std::string actual = GetProjectPath(file);

		FILE* f = fopen(actual.c_str(), "rb");
		fseek(f, 0, SEEK_END);
		fsize = ftell(f);
		fseek(f, 0, SEEK_SET); //same as rewind(f);

		char* string = (char*)malloc(fsize + 1);
		fread(string, fsize, 1, f);
		fclose(f);

		string[fsize] = 0;

		return string;
	}
	eng::Model* ProjectParser::LoadModel(const std::string& file)
	{
		// return already loaded model
		for (auto& mdl : m_models)
			if (mdl.first == file)
				return mdl.second;

		m_models.push_back(std::make_pair(file, new eng::Model()));

		// load the model
		std::string path = GetProjectPath(file);
		bool loaded = m_models[m_models.size() - 1].second->LoadFromFile(path);
		if (!loaded) {
			m_models.erase(m_models.begin() + (m_models.size() - 1));
			return nullptr;
		}

		return m_models[m_models.size() - 1].second;
	}
	void ProjectParser::SaveProjectFile(const std::string& file, const std::string& data)
	{
		std::ofstream out(GetProjectPath(file));
		out << data;
		out.close();
	}
	std::string ProjectParser::GetRelativePath(const std::string& to)
	{
		std::filesystem::path fFrom(m_projectPath);
		std::filesystem::path fTo(to);

#if defined(_WIN32)
		if (fTo.is_absolute())
			if (fTo.root_name() != fFrom.root_name()) // not on the same drive
				return to;
#endif

		return std::filesystem::relative(fTo, fFrom).string();
	}
	std::string ProjectParser::GetProjectPath(const std::string& to)
	{
		std::filesystem::path fTo = std::filesystem::u8path(to);

		if (!fTo.is_absolute())
			fTo = std::filesystem::u8path(m_projectPath) / fTo;
		
		return fTo.generic_string();
	}
	bool ProjectParser::FileExists(const std::string& str)
	{
		return std::filesystem::exists(GetProjectPath(str));
	}
	std::string ProjectParser::GetTexturePath(const std::string& texPath, const std::string& oldProjectPath)
	{
		if (std::filesystem::path(texPath).is_absolute())
			return GetRelativePath(texPath);
		else
			return GetRelativePath(std::filesystem::absolute(std::filesystem::path(oldProjectPath) / texPath).string());				
	}
	void ProjectParser::ResetProjectDirectory()
	{
		m_file = "";
		m_projectPath = std::filesystem::current_path().string();
	}

	void ProjectParser::m_parseVariableValue(pugi::xml_node& node, ShaderVariable* var)
	{
		int rowID = 0;
		for (pugi::xml_node row : node.children("row")) {
			int colID = 0;
			for (pugi::xml_node value : row.children("value")) {
				if (var->Function != FunctionShaderVariable::None) {
					if (var->Function == FunctionShaderVariable::Pointer)
						strcpy(var->Arguments, value.text().as_string());
					else if (var->Function == FunctionShaderVariable::CameraSnapshot)
						strcpy(var->Arguments, value.text().as_string());
					else if (var->Function == FunctionShaderVariable::ObjectProperty) {
						if (colID == 0)
							strcpy(var->Arguments, value.text().as_string());
						else
							strcpy(var->Arguments + PIPELINE_ITEM_NAME_LENGTH, value.text().as_string());
						colID++;
					}
					else if (var->Function == FunctionShaderVariable::PluginFunction)
						var->PluginFuncData.Owner->VariableFunctions_ImportArguments(var->PluginFuncData.Name, (plugin::VariableType)var->GetType(), var->Arguments, getInnerXML(value).c_str());
					else
						*FunctionVariableManager::LoadFloat(var->Arguments, colID++) = value.text().as_float();
				} else {
					if (var->GetType() >= ShaderVariable::ValueType::Boolean1 && var->GetType() <= ShaderVariable::ValueType::Boolean4)
						var->SetBooleanValue(value.text().as_bool(), colID++);
					else if (var->GetType() >= ShaderVariable::ValueType::Integer1 && var->GetType() <= ShaderVariable::ValueType::Integer4)
						var->SetIntegerValue(value.text().as_int(), colID++);
					else
						var->SetFloat(value.text().as_float(), colID++, rowID);
				}
			}
			colID = colID % var->GetColumnCount();
			rowID++;
		}
	}
	void ProjectParser::m_exportVariableValue(pugi::xml_node& node, ShaderVariable* var)
	{
		pugi::xml_node valueRowNode = node.append_child("row");

		if (var->Function == FunctionShaderVariable::None) {
			int rowID = 0;
			int limit = ShaderVariable::GetSize(var->GetType()) / 4;
			for (int i = 0; i < limit; i++) {
				if (var->GetType() >= ShaderVariable::ValueType::Boolean1 && var->GetType() <= ShaderVariable::ValueType::Boolean4)
					valueRowNode.append_child("value").text().set(var->AsBoolean(i));
				else if (var->GetType() >= ShaderVariable::ValueType::Integer1 && var->GetType() <= ShaderVariable::ValueType::Integer4)
					valueRowNode.append_child("value").text().set(var->AsInteger(i));
				else
					valueRowNode.append_child("value").text().set(var->AsFloat(i % var->GetColumnCount(), rowID));

				if ((i + 1) % var->GetColumnCount() == 0 && i != 0 && i != limit - 1) {
					valueRowNode = node.append_child("row");
					rowID++;
				}
			}
		} else {
			if (var->Function == FunctionShaderVariable::Pointer) {
				valueRowNode.append_child("value").text().set(var->Arguments);
			} else if (var->Function == FunctionShaderVariable::CameraSnapshot) {
				valueRowNode.append_child("value").text().set(var->Arguments);
			} else if (var->Function == FunctionShaderVariable::ObjectProperty) {
				valueRowNode.append_child("value").text().set(var->Arguments);
				valueRowNode.append_child("value").text().set(var->Arguments + PIPELINE_ITEM_NAME_LENGTH);
			} else if (var->Function == FunctionShaderVariable::PluginFunction) {
				m_addPlugin(m_plugins->GetPluginName(var->PluginFuncData.Owner));

				const char* valNode = var->PluginFuncData.Owner->VariableFunctions_ExportArguments(var->PluginFuncData.Name, (plugin::VariableType)var->GetType(), var->Arguments);
				valueRowNode.append_child("value").append_buffer(valNode, strlen(valNode));
			} else {
				// save arguments
				for (int i = 0; i < FunctionVariableManager::GetArgumentCount(var->Function); i++)
					valueRowNode.append_child("value").text().set(*FunctionVariableManager::LoadFloat(var->Arguments, i));
			}
		}
	}
	void ProjectParser::m_exportShaderVariables(pugi::xml_node& node, std::vector<ShaderVariable*>& vars)
	{
		if (vars.size() > 0) {
			pugi::xml_node varsNodes = node.append_child("variables");
			for (const auto& var : vars) {
				if (var->GetType() == ShaderVariable::ValueType::Count)
					continue;
					
				pugi::xml_node varNode = varsNodes.append_child("variable");
				varNode.append_attribute("type").set_value(VARIABLE_TYPE_NAMES[(int)var->GetType()]);
				varNode.append_attribute("name").set_value(var->Name);

				bool isInvert = var->Flags & (char)ShaderVariable::Flag::Inverse;
				bool isLastFrame = var->Flags & (char)ShaderVariable::Flag::LastFrame;
				if (isInvert) varNode.append_attribute("invert").set_value(isInvert);
				if (isLastFrame) varNode.append_attribute("lastframe").set_value(isLastFrame);

				if (var->System != SystemShaderVariable::None) {
					varNode.append_attribute("system").set_value(SYSTEM_VARIABLE_NAMES[(int)var->System]);
					if (var->System == SystemShaderVariable::PluginVariable) {
						m_addPlugin(m_plugins->GetPluginName(var->PluginSystemVarData.Owner));
						varNode.append_attribute("plugin").set_value(m_plugins->GetPluginName(var->PluginSystemVarData.Owner).c_str());
						varNode.append_attribute("itemname").set_value(var->PluginSystemVarData.Name);
					}
				} else if (var->Function != FunctionShaderVariable::None) {
					varNode.append_attribute("function").set_value(FUNCTION_NAMES[(int)var->Function]);

					if (var->Function == FunctionShaderVariable::PluginFunction) {
						m_addPlugin(m_plugins->GetPluginName(var->PluginFuncData.Owner));
						varNode.append_attribute("plugin").set_value(m_plugins->GetPluginName(var->PluginFuncData.Owner).c_str());
						varNode.append_attribute("funcname").set_value(var->PluginFuncData.Name);
					}
				}

				if (var->System == SystemShaderVariable::None)
					m_exportVariableValue(varNode, var);
			}
		}
	}
	GLenum ProjectParser::m_toBlend(const char* text)
	{
		for (int k = 0; k < HARRAYSIZE(BLEND_NAMES); k++)
			if (strcmp(text, BLEND_NAMES[k]) == 0)
				return BLEND_VALUES[k];
		return GL_CONSTANT_COLOR;
	}
	GLenum ProjectParser::m_toBlendOp(const char* text)
	{
		for (int k = 0; k < HARRAYSIZE(BLEND_OPERATOR_NAMES); k++)
			if (strcmp(text, BLEND_OPERATOR_NAMES[k]) == 0)
				return BLEND_OPERATOR_VALUES[k];
		return GL_FUNC_ADD;
	}
	GLenum ProjectParser::m_toComparisonFunc(const char* str)
	{
		for (int k = 0; k < HARRAYSIZE(COMPARISON_FUNCTION_NAMES); k++)
			if (strcmp(str, COMPARISON_FUNCTION_NAMES[k]) == 0)
				return COMPARISON_FUNCTION_VALUES[k];
		return GL_ALWAYS;
	}
	GLenum ProjectParser::m_toStencilOp(const char* str)
	{
		for (int k = 0; k < HARRAYSIZE(STENCIL_OPERATION_NAMES); k++)
			if (strcmp(str, STENCIL_OPERATION_NAMES[k]) == 0)
				return STENCIL_OPERATION_VALUES[k];
		return GL_KEEP;
	}
	GLenum ProjectParser::m_toCullMode(const char* str)
	{
		for (int k = 0; k < HARRAYSIZE(CULL_MODE_NAMES); k++)
			if (strcmp(str, CULL_MODE_NAMES[k]) == 0)
				return CULL_MODE_VALUES[k];
		return GL_BACK;
	}

	void ProjectParser::m_exportItems(pugi::xml_node& node, std::vector<PipelineItem*>& items, const std::string& oldProjectPath)
	{
		for (PipelineItem* item : items) {
			pugi::xml_node itemNode = node.append_child("item");
			itemNode.append_attribute("name").set_value(item->Name);

			if (item->Type == PipelineItem::ItemType::Geometry) {
				itemNode.append_attribute("type").set_value("geometry");

				ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(item->Data);

				itemNode.append_child("type").text().set(GEOMETRY_NAMES[tData->Type]);
				itemNode.append_child("width").text().set(tData->Size.x);
				itemNode.append_child("height").text().set(tData->Size.y);
				itemNode.append_child("depth").text().set(tData->Size.z);
				if (tData->Scale.x != 1.0f)
					itemNode.append_child("scaleX").text().set(tData->Scale.x);
				if (tData->Scale.y != 1.0f)
					itemNode.append_child("scaleY").text().set(tData->Scale.y);
				if (tData->Scale.z != 1.0f)
					itemNode.append_child("scaleZ").text().set(tData->Scale.z);
				if (tData->Rotation.z != 0.0f)
					itemNode.append_child("roll").text().set(tData->Rotation.z);
				if (tData->Rotation.x != 0.0f)
					itemNode.append_child("pitch").text().set(tData->Rotation.x);
				if (tData->Rotation.y != 0.0f)
					itemNode.append_child("yaw").text().set(tData->Rotation.y);
				if (tData->Position.x != 0.0f)
					itemNode.append_child("x").text().set(tData->Position.x);
				if (tData->Position.y != 0.0f)
					itemNode.append_child("y").text().set(tData->Position.y);
				if (tData->Position.z != 0.0f)
					itemNode.append_child("z").text().set(tData->Position.z);
				if (tData->Instanced)
					itemNode.append_child("instanced").text().set(tData->Instanced);
				if (tData->InstanceCount > 0)
					itemNode.append_child("instancecount").text().set(tData->InstanceCount);
				if (tData->InstanceBuffer != nullptr)
					itemNode.append_child("instancebuffer").text().set(m_objects->GetByBufferID(((BufferObject*)tData->InstanceBuffer)->ID)->Name.c_str());
				for (int tind = 0; tind < HARRAYSIZE(TOPOLOGY_ITEM_VALUES); tind++) {
					if (TOPOLOGY_ITEM_VALUES[tind] == tData->Topology) {
						itemNode.append_child("topology").text().set(TOPOLOGY_ITEM_NAMES[tind]);
						break;
					}
				}
			} else if (item->Type == PipelineItem::ItemType::RenderState) {
				itemNode.append_attribute("type").set_value("renderstate");

				ed::pipe::RenderState* s = reinterpret_cast<ed::pipe::RenderState*>(item->Data);

				if (s->PolygonMode != GL_FILL)
					itemNode.append_child("wireframe").text().set(s->PolygonMode == GL_LINE);
				if (!s->CullFace)
					itemNode.append_child("cull").text().set(s->CullFace);
				if (s->CullFaceType != GL_BACK)
					itemNode.append_child("cullfront").text().set(true);
				if (s->FrontFace != GL_CCW)
					itemNode.append_child("ccw").text().set(false);

				itemNode.append_child("blend").text().set(s->Blend);
				itemNode.append_child("alpha2coverage").text().set(s->AlphaToCoverage);
				itemNode.append_child("colorsrcfactor").text().set(gl::String::BlendFactor(s->BlendSourceFactorRGB));
				itemNode.append_child("colordstfactor").text().set(gl::String::BlendFactor(s->BlendDestinationFactorRGB));
				itemNode.append_child("colorfunc").text().set(gl::String::BlendFunction(s->BlendFunctionColor));
				itemNode.append_child("alphasrcfactor").text().set(gl::String::BlendFactor(s->BlendSourceFactorAlpha));
				itemNode.append_child("alphadstfactor").text().set(gl::String::BlendFactor(s->BlendDestinationFactorAlpha));
				itemNode.append_child("alphafunc").text().set(gl::String::BlendFunction(s->BlendFunctionAlpha));
				itemNode.append_child("blendfactor_r").text().set(s->BlendFactor.r);
				itemNode.append_child("blendfactor_g").text().set(s->BlendFactor.g);
				itemNode.append_child("blendfactor_b").text().set(s->BlendFactor.b);
				itemNode.append_child("blendfactor_a").text().set(s->BlendFactor.a);

				itemNode.append_child("depthtest").text().set(s->DepthTest);
				itemNode.append_child("depthclamp").text().set(s->DepthClamp);
				itemNode.append_child("depthmask").text().set(s->DepthMask);
				itemNode.append_child("depthfunc").text().set(gl::String::ComparisonFunction(s->DepthFunction));
				itemNode.append_child("depthbias").text().set(s->DepthBias);

				itemNode.append_child("stenciltest").text().set(s->StencilTest);
				itemNode.append_child("stencilmask").text().set(s->StencilMask);
				itemNode.append_child("stencilref").text().set(s->StencilReference);
				itemNode.append_child("stencilfrontfunc").text().set(gl::String::ComparisonFunction(s->StencilFrontFaceFunction));
				itemNode.append_child("stencilbackfunc").text().set(gl::String::ComparisonFunction(s->StencilBackFaceFunction));
				itemNode.append_child("stencilfrontpass").text().set(gl::String::StencilOperation(s->StencilFrontFaceOpPass));
				itemNode.append_child("stencilbackpass").text().set(gl::String::StencilOperation(s->StencilBackFaceOpPass));
				itemNode.append_child("stencilfrontfail").text().set(gl::String::StencilOperation(s->StencilFrontFaceOpStencilFail));
				itemNode.append_child("stencilbackfail").text().set(gl::String::StencilOperation(s->StencilBackFaceOpStencilFail));
				itemNode.append_child("depthfrontfail").text().set(gl::String::StencilOperation(s->StencilFrontFaceOpDepthFail));
				itemNode.append_child("depthbackfail").text().set(gl::String::StencilOperation(s->StencilBackFaceOpDepthFail));
			} else if (item->Type == PipelineItem::ItemType::Model) {
				itemNode.append_attribute("type").set_value("model");

				ed::pipe::Model* data = reinterpret_cast<ed::pipe::Model*>(item->Data);

				std::string opath = (std::filesystem::path(data->Filename).is_absolute()) ? data->Filename : GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '/') ? "" : "/") + std::string(data->Filename));

				itemNode.append_child("filepath").text().set(opath.c_str());
				itemNode.append_child("grouponly").text().set(data->OnlyGroup);
				if (data->OnlyGroup)
					itemNode.append_child("group").text().set(data->GroupName);
				if (data->Scale.x != 1.0f)
					itemNode.append_child("scaleX").text().set(data->Scale.x);
				if (data->Scale.y != 1.0f)
					itemNode.append_child("scaleY").text().set(data->Scale.y);
				if (data->Scale.z != 1.0f)
					itemNode.append_child("scaleZ").text().set(data->Scale.z);
				if (data->Rotation.z != 0.0f)
					itemNode.append_child("roll").text().set(data->Rotation.z);
				if (data->Rotation.x != 0.0f)
					itemNode.append_child("pitch").text().set(data->Rotation.x);
				if (data->Rotation.y != 0.0f)
					itemNode.append_child("yaw").text().set(data->Rotation.y);
				if (data->Position.x != 0.0f)
					itemNode.append_child("x").text().set(data->Position.x);
				if (data->Position.y != 0.0f)
					itemNode.append_child("y").text().set(data->Position.y);
				if (data->Position.z != 0.0f)
					itemNode.append_child("z").text().set(data->Position.z);
				if (data->Instanced)
					itemNode.append_child("instanced").text().set(data->Instanced);
				if (data->InstanceCount > 0)
					itemNode.append_child("instancecount").text().set(data->InstanceCount);
				if (data->InstanceBuffer != nullptr)
					itemNode.append_child("instancebuffer").text().set(m_objects->GetByBufferID(((BufferObject*)data->InstanceBuffer)->ID)->Name.c_str());
			} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
				itemNode.append_attribute("type").set_value("vertexbuffer");

				pipe::VertexBuffer* tData = reinterpret_cast<pipe::VertexBuffer*>(item->Data);

				itemNode.append_child("buffer").text().set(m_objects->GetByBufferID(((ed::BufferObject*)tData->Buffer)->ID)->Name.c_str());
				if (tData->Scale.x != 1.0f)
					itemNode.append_child("scaleX").text().set(tData->Scale.x);
				if (tData->Scale.y != 1.0f)
					itemNode.append_child("scaleY").text().set(tData->Scale.y);
				if (tData->Scale.z != 1.0f)
					itemNode.append_child("scaleZ").text().set(tData->Scale.z);
				if (tData->Rotation.z != 0.0f)
					itemNode.append_child("roll").text().set(tData->Rotation.z);
				if (tData->Rotation.x != 0.0f)
					itemNode.append_child("pitch").text().set(tData->Rotation.x);
				if (tData->Rotation.y != 0.0f)
					itemNode.append_child("yaw").text().set(tData->Rotation.y);
				if (tData->Position.x != 0.0f)
					itemNode.append_child("x").text().set(tData->Position.x);
				if (tData->Position.y != 0.0f)
					itemNode.append_child("y").text().set(tData->Position.y);
				if (tData->Position.z != 0.0f)
					itemNode.append_child("z").text().set(tData->Position.z);
				if (tData->Instanced)
					itemNode.append_child("instanced").text().set(tData->Instanced);
				if (tData->InstanceCount > 0)
					itemNode.append_child("instancecount").text().set(tData->InstanceCount);
				if (tData->InstanceBuffer != nullptr)
					itemNode.append_child("instancebuffer").text().set(m_objects->GetByBufferID(((BufferObject*)tData->InstanceBuffer)->ID)->Name.c_str());
				for (int tind = 0; tind < HARRAYSIZE(TOPOLOGY_ITEM_VALUES); tind++) {
					if (TOPOLOGY_ITEM_VALUES[tind] == tData->Topology) {
						itemNode.append_child("topology").text().set(TOPOLOGY_ITEM_NAMES[tind]);
						break;
					}
				}
			} else if (item->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* plData = (pipe::PluginItemData*)item->Data;
				m_addPlugin(m_plugins->GetPluginName(plData->Owner));

				itemNode.append_attribute("type").set_value("plugin");
				itemNode.append_attribute("plugin").set_value(m_plugins->GetPluginName(plData->Owner).c_str());
				itemNode.append_attribute("itemtype").set_value(plData->Type);

				pugi::xml_node dataNode = itemNode.append_child("data");
				const char* pObjectSrc = plData->Owner->PipelineItem_Export(plData->Type, plData->PluginData);

				if (pObjectSrc != nullptr)
					dataNode.append_buffer(pObjectSrc, strlen(pObjectSrc));
			}
		}
	}
	void ProjectParser::m_importItems(const char* name, pipe::ShaderPass* data, const pugi::xml_node& node, const std::vector<InputLayoutItem>& inpLayout,
		std::map<pipe::GeometryItem*, std::pair<std::string, pipe::ShaderPass*>>& geoUBOs,
		std::map<pipe::Model*, std::pair<std::string, pipe::ShaderPass*>>& modelUBOs,
		std::map<pipe::VertexBuffer*, std::pair<std::string, pipe::ShaderPass*>>& vbUBOs,
		std::map<pipe::VertexBuffer*, std::pair<std::string, pipe::ShaderPass*>>& vbInstanceUBOs)
	{
		for (pugi::xml_node itemNode : node.children()) {
			char itemName[PIPELINE_ITEM_NAME_LENGTH];
			ed::PipelineItem::ItemType itemType = ed::PipelineItem::ItemType::Geometry;
			void* itemData = nullptr;

			strcpy(itemName, itemNode.attribute("name").as_string());

			// parse the inner content of the item
			if (strcmp(itemNode.attribute("type").as_string(), "geometry") == 0) {
				itemType = ed::PipelineItem::ItemType::Geometry;
				itemData = new pipe::GeometryItem;
				pipe::GeometryItem* tData = (pipe::GeometryItem*)itemData;

				tData->Scale = glm::vec3(1, 1, 1);
				tData->Position = glm::vec3(0, 0, 0);
				tData->Rotation = glm::vec3(0, 0, 0);
				tData->Instanced = false;
				tData->InstanceCount = 0;
				tData->InstanceBuffer = nullptr;

				for (pugi::xml_node attrNode : itemNode.children()) {
					if (strcmp(attrNode.name(), "width") == 0)
						tData->Size.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "height") == 0)
						tData->Size.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "depth") == 0)
						tData->Size.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "scaleX") == 0)
						tData->Scale.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "scaleY") == 0)
						tData->Scale.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "scaleZ") == 0)
						tData->Scale.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "roll") == 0)
						tData->Rotation.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "yaw") == 0)
						tData->Rotation.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "pitch") == 0)
						tData->Rotation.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "x") == 0)
						tData->Position.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "y") == 0)
						tData->Position.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "z") == 0)
						tData->Position.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "instanced") == 0)
						tData->Instanced = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "instancecount") == 0)
						tData->InstanceCount = attrNode.text().as_int();
					else if (strcmp(attrNode.name(), "instancebuffer") == 0)
						geoUBOs[tData] = std::make_pair(attrNode.text().as_string(), data);
					else if (strcmp(attrNode.name(), "topology") == 0) {
						for (int k = 0; k < HARRAYSIZE(TOPOLOGY_ITEM_NAMES); k++)
							if (strcmp(attrNode.text().as_string(), TOPOLOGY_ITEM_NAMES[k]) == 0)
								tData->Topology = TOPOLOGY_ITEM_VALUES[k];
					} else if (strcmp(attrNode.name(), "type") == 0) {
						for (int k = 0; k < HARRAYSIZE(GEOMETRY_NAMES); k++)
							if (strcmp(attrNode.text().as_string(), GEOMETRY_NAMES[k]) == 0)
								tData->Type = (pipe::GeometryItem::GeometryType)k;
					}
				}
			} else if (strcmp(itemNode.attribute("type").as_string(), "renderstate") == 0) {
				itemType = ed::PipelineItem::ItemType::RenderState;
				itemData = new pipe::RenderState;

				pipe::RenderState* tData = (pipe::RenderState*)itemData;

				for (pugi::xml_node attrNode : itemNode.children()) {
					// rasterizer
					if (strcmp(attrNode.name(), "wireframe") == 0)
						tData->PolygonMode = attrNode.text().as_bool() ? GL_LINE : GL_FILL;
					else if (strcmp(attrNode.name(), "cull") == 0)
						tData->CullFace = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "cullfront") == 0)
						tData->CullFaceType = attrNode.text().as_bool() ? GL_FRONT : GL_BACK;
					else if (strcmp(attrNode.name(), "ccw") == 0)
						tData->FrontFace = attrNode.text().as_bool() ? GL_CCW : GL_CW;

					// blend
					else if (strcmp(attrNode.name(), "blend") == 0)
						tData->Blend = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "colorsrcfactor") == 0)
						tData->BlendSourceFactorRGB = m_toBlend(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "colorfunc") == 0)
						tData->BlendFunctionColor = m_toBlendOp(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "colordstfactor") == 0)
						tData->BlendDestinationFactorRGB = m_toBlend(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "alphasrcfactor") == 0)
						tData->BlendSourceFactorAlpha = m_toBlend(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "alphafunc") == 0)
						tData->BlendFunctionAlpha = m_toBlendOp(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "alphadstfactor") == 0)
						tData->BlendDestinationFactorAlpha = m_toBlend(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "alpha2coverage") == 0)
						tData->AlphaToCoverage = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "blendfactor_r") == 0)
						tData->BlendFactor.r = attrNode.text().as_uint();
					else if (strcmp(attrNode.name(), "blendfactor_g") == 0)
						tData->BlendFactor.g = attrNode.text().as_uint();
					else if (strcmp(attrNode.name(), "blendfactor_b") == 0)
						tData->BlendFactor.b = attrNode.text().as_uint();
					else if (strcmp(attrNode.name(), "blendfactor_a") == 0)
						tData->BlendFactor.a = attrNode.text().as_uint();

					// depth
					else if (strcmp(attrNode.name(), "depthtest") == 0)
						tData->DepthTest = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "depthfunc") == 0)
						tData->DepthFunction = m_toComparisonFunc(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "depthbias") == 0)
						tData->DepthBias = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "depthclamp") == 0)
						tData->DepthClamp = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "depthmask") == 0)
						tData->DepthMask = attrNode.text().as_bool();

					// stencil
					else if (strcmp(attrNode.name(), "stenciltest") == 0)
						tData->StencilTest = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "stencilmask") == 0)
						tData->StencilMask = attrNode.text().as_uint();
					else if (strcmp(attrNode.name(), "stencilref") == 0)
						tData->StencilReference = attrNode.text().as_uint();
					else if (strcmp(attrNode.name(), "stencilfrontfunc") == 0)
						tData->StencilFrontFaceFunction = m_toComparisonFunc(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "stencilfrontpass") == 0)
						tData->StencilFrontFaceOpPass = m_toStencilOp(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "stencilfrontfail") == 0)
						tData->StencilFrontFaceOpStencilFail = m_toStencilOp(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "stencilbackfunc") == 0)
						tData->StencilBackFaceFunction = m_toComparisonFunc(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "stencilbackpass") == 0)
						tData->StencilBackFaceOpPass = m_toStencilOp(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "stencilbackfail") == 0)
						tData->StencilBackFaceOpStencilFail = m_toStencilOp(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "depthfrontfail") == 0)
						tData->StencilFrontFaceOpDepthFail = m_toStencilOp(attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "depthbackfail") == 0)
						tData->StencilBackFaceOpDepthFail = m_toStencilOp(attrNode.text().as_string());
				}
			} else if (strcmp(itemNode.attribute("type").as_string(), "model") == 0) {
				itemType = ed::PipelineItem::ItemType::Model;
				itemData = new pipe::Model;

				pipe::Model* mdata = (pipe::Model*)itemData;

				mdata->OnlyGroup = false;
				mdata->Scale = glm::vec3(1, 1, 1);
				mdata->Position = glm::vec3(0, 0, 0);
				mdata->Rotation = glm::vec3(0, 0, 0);
				mdata->InstanceBuffer = nullptr;
				mdata->Instanced = false;
				mdata->InstanceCount = 0;

				modelUBOs[mdata] = std::make_pair("", data);

				for (pugi::xml_node attrNode : itemNode.children()) {
					if (strcmp(attrNode.name(), "filepath") == 0)
						strcpy(mdata->Filename, attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "group") == 0)
						strcpy(mdata->GroupName, attrNode.text().as_string());
					else if (strcmp(attrNode.name(), "grouponly") == 0)
						mdata->OnlyGroup = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "scaleX") == 0)
						mdata->Scale.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "scaleY") == 0)
						mdata->Scale.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "scaleZ") == 0)
						mdata->Scale.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "roll") == 0)
						mdata->Rotation.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "yaw") == 0)
						mdata->Rotation.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "pitch") == 0)
						mdata->Rotation.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "x") == 0)
						mdata->Position.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "y") == 0)
						mdata->Position.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "z") == 0)
						mdata->Position.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "instanced") == 0)
						mdata->Instanced = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "instancecount") == 0)
						mdata->InstanceCount = attrNode.text().as_int();
					else if (strcmp(attrNode.name(), "instancebuffer") == 0)
						modelUBOs[mdata] = std::make_pair(attrNode.text().as_string(), data);
				}

				if (strlen(mdata->Filename) > 0)
					strcpy(mdata->Filename, toGenericPath(mdata->Filename).c_str());
			} else if (strcmp(itemNode.attribute("type").as_string(), "vertexbuffer") == 0) {
				itemType = ed::PipelineItem::ItemType::VertexBuffer;
				itemData = new pipe::VertexBuffer;
				pipe::VertexBuffer* vbData = (pipe::VertexBuffer*)itemData;

				vbData->Buffer = 0;
				vbData->Scale = glm::vec3(1, 1, 1);
				vbData->Position = glm::vec3(0, 0, 0);
				vbData->Rotation = glm::vec3(0, 0, 0);
				vbData->Instanced = false;
				vbData->InstanceCount = 0;
				vbData->InstanceBuffer = nullptr;

				for (pugi::xml_node attrNode : itemNode.children()) {
					if (strcmp(attrNode.name(), "scaleX") == 0)
						vbData->Scale.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "scaleY") == 0)
						vbData->Scale.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "scaleZ") == 0)
						vbData->Scale.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "roll") == 0)
						vbData->Rotation.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "yaw") == 0)
						vbData->Rotation.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "pitch") == 0)
						vbData->Rotation.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "x") == 0)
						vbData->Position.x = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "y") == 0)
						vbData->Position.y = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "z") == 0)
						vbData->Position.z = attrNode.text().as_float();
					else if (strcmp(attrNode.name(), "buffer") == 0)
						vbUBOs[vbData] = std::make_pair(attrNode.text().as_string(), data);
					else if (strcmp(attrNode.name(), "instanced") == 0)
						vbData->Instanced = attrNode.text().as_bool();
					else if (strcmp(attrNode.name(), "instancecount") == 0)
						vbData->InstanceCount = attrNode.text().as_int();
					else if (strcmp(attrNode.name(), "instancebuffer") == 0)
						vbInstanceUBOs[vbData] = std::make_pair(attrNode.text().as_string(), data);
					else if (strcmp(attrNode.name(), "topology") == 0) {
						for (int k = 0; k < HARRAYSIZE(TOPOLOGY_ITEM_NAMES); k++)
							if (strcmp(attrNode.text().as_string(), TOPOLOGY_ITEM_NAMES[k]) == 0)
								vbData->Topology = TOPOLOGY_ITEM_VALUES[k];
					}
				}
			} else if (strcmp(itemNode.attribute("type").as_string(), "plugin") == 0) {
				IPlugin1* plugin = m_plugins->GetPlugin(itemNode.attribute("plugin").as_string());

				itemType = PipelineItem::ItemType::PluginItem;
				itemData = new pipe::PluginItemData;

				if (plugin) {
					pipe::PluginItemData* tData = (pipe::PluginItemData*)itemData;

					std::string otype(itemNode.attribute("itemtype").as_string());

					tData->Items.clear();
					tData->Owner = plugin;
					strcpy(tData->Type, otype.c_str());
					tData->PluginData = plugin->PipelineItem_Import(name, itemName, otype.c_str(), getInnerXML(itemNode.child("data")).c_str());
				}
			}

			// create and modify if needed
			if (itemType == ed::PipelineItem::ItemType::Geometry) {
				ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(itemData);
				if (tData->Type == pipe::GeometryItem::Cube)
					tData->VAO = eng::GeometryFactory::CreateCube(tData->VBO, tData->Size.x, tData->Size.y, tData->Size.z, inpLayout);
				else if (tData->Type == pipe::GeometryItem::Circle)
					tData->VAO = eng::GeometryFactory::CreateCircle(tData->VBO, tData->Size.x, tData->Size.y, inpLayout);
				else if (tData->Type == pipe::GeometryItem::Plane)
					tData->VAO = eng::GeometryFactory::CreatePlane(tData->VBO, tData->Size.x, tData->Size.y, inpLayout);
				else if (tData->Type == pipe::GeometryItem::Rectangle)
					tData->VAO = eng::GeometryFactory::CreatePlane(tData->VBO, 1, 1, inpLayout);
				else if (tData->Type == pipe::GeometryItem::Sphere)
					tData->VAO = eng::GeometryFactory::CreateSphere(tData->VBO, tData->Size.x, inpLayout);
				else if (tData->Type == pipe::GeometryItem::Triangle)
					tData->VAO = eng::GeometryFactory::CreateTriangle(tData->VBO, tData->Size.x, inpLayout);
				else if (tData->Type == pipe::GeometryItem::ScreenQuadNDC)
					tData->VAO = eng::GeometryFactory::CreateScreenQuadNDC(tData->VBO, inpLayout);
			} else if (itemType == ed::PipelineItem::ItemType::Model) {
				pipe::Model* tData = reinterpret_cast<pipe::Model*>(itemData);

				//std::string objMem = LoadProjectFile(tData->Filename);
				eng::Model* ptrObject = LoadModel(tData->Filename);
				bool loaded = ptrObject != nullptr;

				if (loaded)
					tData->Data = ptrObject;
				else
					m_msgs->Add(ed::MessageStack::Type::Error, name, "Failed to load .obj model " + std::string(itemName));
			}

			m_pipe->AddItem(name, itemName, itemType, itemData);
		}
	}

	void ProjectParser::m_addPlugin(const std::string& name)
	{
		if (std::count(m_pluginList.begin(), m_pluginList.end(), name) == 0)
			m_pluginList.push_back(name);
	}

	// parser versions
	void ProjectParser::m_parseV1(pugi::xml_node& projectNode)
	{
		Logger::Get().Log("Parsing a V1 project file...");

		std::map<pipe::ShaderPass*, std::vector<std::string>> fbos;

		Settings::Instance().Project.IncludePaths.clear();

		// shader passes
		for (pugi::xml_node passNode : projectNode.child("pipeline").children("pass")) {
			char name[PIPELINE_ITEM_NAME_LENGTH];
			ed::PipelineItem::ItemType type = ed::PipelineItem::ItemType::ShaderPass;
			ed::pipe::ShaderPass* data = new ed::pipe::ShaderPass();

			data->InputLayout = gl::CreateDefaultInputLayout();

			data->RenderTextures[0] = m_renderer->GetTexture();
			for (int i = 1; i < MAX_RENDER_TEXTURES; i++)
				data->RenderTextures[i] = 0;

			// get pass name
			if (!passNode.attribute("name").empty())
				strcpy(name, passNode.attribute("name").as_string());

			// check if it should be collapsed
			if (!passNode.attribute("collapsed").empty()) {
				bool cs = passNode.attribute("collapsed").as_bool();
				if (cs)
					((PipelineUI*)m_ui->Get(ViewID::Pipeline))->Collapse(data);
			}

			// get render textures
			int rtCur = 0;
			for (pugi::xml_node rtNode : passNode.children("rendertexture")) {
				std::string rtName(rtNode.attribute("name").as_string());
				fbos[data].push_back(rtName);
				rtCur++;
			}
			data->RTCount = (rtCur == 0) ? 1 : rtCur;

			// add the item
			m_pipe->AddShaderPass(name, data);

			// get shader properties (NOTE: a shader must have TYPE, PATH and ENTRY
			for (pugi::xml_node shaderNode : passNode.children("shader")) {
				std::string shaderNodeType(shaderNode.attribute("type").as_string()); // "vs" or "ps" or "gs"

				// parse path and type
				pugi::char_t shaderPath[SHADERED_MAX_PATH];
				strcpy(shaderPath, toGenericPath(shaderNode.child("path").text().as_string()).c_str());
				const pugi::char_t* shaderEntry = shaderNode.child("entry").text().as_string();
				if (shaderNodeType == "vs") {
					strcpy(data->VSPath, shaderPath);
					strcpy(data->VSEntry, shaderEntry);
				} else if (shaderNodeType == "ps") {
					strcpy(data->PSPath, shaderPath);
					strcpy(data->PSEntry, shaderEntry);
				} else if (shaderNodeType == "gs") {
					if (!shaderNode.attribute("used").empty())
						data->GSUsed = shaderNode.attribute("used").as_bool();
					else
						data->GSUsed = false;
					strcpy(data->GSPath, shaderPath);
					strcpy(data->GSEntry, shaderEntry);
				}

				std::string type = ((shaderNodeType == "vs") ? "vertex" : ((shaderNodeType == "ps") ? "pixel" : "geometry"));
				if (!FileExists(shaderPath))
					m_msgs->Add(ed::MessageStack::Type::Error, name, type + " shader does not exist.");

				// parse variables
				for (pugi::xml_node variableNode : shaderNode.child("variables").children("variable")) {
					ShaderVariable::ValueType type = ShaderVariable::ValueType::Float1;
					SystemShaderVariable system = SystemShaderVariable::None;
					FunctionShaderVariable func = FunctionShaderVariable::None;

					if (!variableNode.attribute("type").empty()) {
						const char* myType = variableNode.attribute("type").as_string();
						for (int i = 0; i < HARRAYSIZE(VARIABLE_TYPE_NAMES); i++)
							if (strcmp(myType, VARIABLE_TYPE_NAMES[i]) == 0) {
								type = (ed::ShaderVariable::ValueType)i;
								break;
							}
					}
					if (!variableNode.attribute("system").empty()) {
						const char* mySystem = variableNode.attribute("system").as_string();
						for (int i = 0; i < HARRAYSIZE(SYSTEM_VARIABLE_NAMES); i++)
							if (strcmp(mySystem, SYSTEM_VARIABLE_NAMES[i]) == 0) {
								system = (ed::SystemShaderVariable)i;
								break;
							}
						if (SystemVariableManager::GetType(system) != type)
							system = ed::SystemShaderVariable::None;
					}
					if (!variableNode.attribute("function").empty()) {
						const char* myFunc = variableNode.attribute("function").as_string();
						for (int i = 0; i < HARRAYSIZE(FUNCTION_NAMES); i++)
							if (strcmp(myFunc, FUNCTION_NAMES[i]) == 0) {
								func = (FunctionShaderVariable)i;
								break;
							}
						if (system != SystemShaderVariable::None || !FunctionVariableManager::HasValidReturnType(type, func))
							func = FunctionShaderVariable::None;
					}

					ShaderVariable* var = new ShaderVariable(type, variableNode.attribute("name").as_string(), system);
					FunctionVariableManager::AllocateArgumentSpace(var, func);

					// parse value
					if (system == SystemShaderVariable::None)
						m_parseVariableValue(variableNode, var);

					data->Variables.Add(var);
				}
			}

			// parse items
			for (pugi::xml_node itemNode : passNode.child("items").children()) {
				char itemName[PIPELINE_ITEM_NAME_LENGTH];
				ed::PipelineItem::ItemType itemType = ed::PipelineItem::ItemType::Geometry;
				void* itemData = nullptr;

				strcpy(itemName, itemNode.attribute("name").as_string());

				// parse the inner content of the item
				if (strcmp(itemNode.attribute("type").as_string(), "geometry") == 0) {
					itemType = ed::PipelineItem::ItemType::Geometry;
					itemData = new pipe::GeometryItem;
					pipe::GeometryItem* tData = (pipe::GeometryItem*)itemData;

					tData->Scale = glm::vec3(1, 1, 1);
					tData->Position = glm::vec3(0, 0, 0);
					tData->Rotation = glm::vec3(0, 0, 0);
					tData->InstanceBuffer = nullptr;
					tData->InstanceCount = 0;
					tData->Instanced = false;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "width") == 0)
							tData->Size.x = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "height") == 0)
							tData->Size.y = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "depth") == 0)
							tData->Size.z = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "scaleX") == 0)
							tData->Scale.x = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "scaleY") == 0)
							tData->Scale.y = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "scaleZ") == 0)
							tData->Scale.z = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "roll") == 0)
							tData->Rotation.z = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "yaw") == 0)
							tData->Rotation.y = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "pitch") == 0)
							tData->Rotation.x = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "x") == 0)
							tData->Position.x = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "y") == 0)
							tData->Position.y = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "z") == 0)
							tData->Position.z = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "topology") == 0) {
							for (int k = 0; k < HARRAYSIZE(TOPOLOGY_ITEM_NAMES); k++)
								if (strcmp(attrNode.text().as_string(), TOPOLOGY_ITEM_NAMES[k]) == 0)
									tData->Topology = TOPOLOGY_ITEM_VALUES[k];
						} else if (strcmp(attrNode.name(), "type") == 0) {
							for (int k = 0; k < HARRAYSIZE(GEOMETRY_NAMES); k++)
								if (strcmp(attrNode.text().as_string(), GEOMETRY_NAMES[k]) == 0)
									tData->Type = (pipe::GeometryItem::GeometryType)k;
						}
					}
				} else if (strcmp(itemNode.attribute("type").as_string(), "blend") == 0) {
					itemType = ed::PipelineItem::ItemType::RenderState;
					itemData = new pipe::RenderState();

					pipe::RenderState* tData = (pipe::RenderState*)itemData;
					glm::vec4 blendFactor(0, 0, 0, 0);

					tData->Blend = true;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "srcblend") == 0)
							tData->BlendSourceFactorRGB = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "blendop") == 0)
							tData->BlendFunctionColor = m_toBlendOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "destblend") == 0)
							tData->BlendDestinationFactorRGB = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "srcblendalpha") == 0)
							tData->BlendSourceFactorAlpha = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "alphablendop") == 0)
							tData->BlendFunctionAlpha = m_toBlendOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "destblendalpha") == 0)
							tData->BlendDestinationFactorAlpha = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "alpha2cov") == 0)
							tData->AlphaToCoverage = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "bf_red") == 0)
							tData->BlendFactor.x = attrNode.text().as_uint();
						else if (strcmp(attrNode.name(), "bf_green") == 0)
							tData->BlendFactor.y = attrNode.text().as_uint();
						else if (strcmp(attrNode.name(), "bf_blue") == 0)
							tData->BlendFactor.z = attrNode.text().as_uint();
						else if (strcmp(attrNode.name(), "bf_alpha") == 0)
							tData->BlendFactor.w = attrNode.text().as_uint();
					}
				} else if (strcmp(itemNode.attribute("type").as_string(), "depthstencil") == 0) {
					itemType = ed::PipelineItem::ItemType::RenderState;
					itemData = new pipe::RenderState;

					pipe::RenderState* tData = (pipe::RenderState*)itemData;

					tData->StencilMask = 0xFF;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "depthenable") == 0)
							tData->DepthTest = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "depthfunc") == 0)
							tData->DepthFunction = m_toComparisonFunc(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "stencilenable") == 0)
							tData->StencilTest = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "frontfunc") == 0)
							tData->StencilFrontFaceFunction = m_toComparisonFunc(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "frontpass") == 0)
							tData->StencilFrontFaceOpPass = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "frontfail") == 0)
							tData->StencilFrontFaceOpStencilFail = tData->StencilFrontFaceOpDepthFail = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "backfunc") == 0)
							tData->StencilBackFaceFunction = m_toComparisonFunc(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "backpass") == 0)
							tData->StencilBackFaceOpPass = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "backfail") == 0)
							tData->StencilBackFaceOpStencilFail = tData->StencilBackFaceOpDepthFail = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "sref") == 0)
							tData->StencilReference = attrNode.text().as_uint();
					}
				} else if (strcmp(itemNode.attribute("type").as_string(), "rasterizer") == 0) {
					itemType = ed::PipelineItem::ItemType::RenderState;
					itemData = new pipe::RenderState;

					pipe::RenderState* tData = (pipe::RenderState*)itemData;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "wireframe") == 0)
							tData->PolygonMode = attrNode.text().as_bool() ? GL_LINE : GL_FILL;
						else if (strcmp(attrNode.name(), "cull") == 0) {
							tData->CullFaceType = m_toCullMode(attrNode.text().as_string());

							if (tData->CullFaceType == GL_ZERO)
								tData->CullFace = false;
							else
								tData->CullFace = true;
						} else if (strcmp(attrNode.name(), "ccw") == 0)
							tData->FrontFace = attrNode.text().as_bool() ? GL_CCW : GL_CW;
						else if (strcmp(attrNode.name(), "depthbias") == 0)
							tData->DepthBias = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "depthclip") == 0)
							tData->DepthClamp = attrNode.text().as_bool();
					}
				} else if (strcmp(itemNode.attribute("type").as_string(), "model") == 0) {
					itemType = ed::PipelineItem::ItemType::Model;
					itemData = new pipe::Model;

					pipe::Model* mdata = (pipe::Model*)itemData;

					mdata->OnlyGroup = false;
					mdata->Scale = glm::vec3(1, 1, 1);
					mdata->Position = glm::vec3(0, 0, 0);
					mdata->Rotation = glm::vec3(0, 0, 0);
					mdata->InstanceBuffer = nullptr;
					mdata->InstanceCount = 0;
					mdata->Instanced = false;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "filepath") == 0)
							strcpy(mdata->Filename, attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "group") == 0)
							strcpy(mdata->GroupName, attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "grouponly") == 0)
							mdata->OnlyGroup = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "scaleX") == 0)
							mdata->Scale.x = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "scaleY") == 0)
							mdata->Scale.y = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "scaleZ") == 0)
							mdata->Scale.z = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "roll") == 0)
							mdata->Rotation.z = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "yaw") == 0)
							mdata->Rotation.y = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "pitch") == 0)
							mdata->Rotation.x = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "x") == 0)
							mdata->Position.x = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "y") == 0)
							mdata->Position.y = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "z") == 0)
							mdata->Position.z = attrNode.text().as_float();
					}

					if (strlen(mdata->Filename) > 0)
						strcpy(mdata->Filename, toGenericPath(mdata->Filename).c_str());
				}

				// create and modify if needed
				if (itemType == ed::PipelineItem::ItemType::Geometry) {
					ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(itemData);

					if (tData->Type == pipe::GeometryItem::Cube)
						tData->VAO = eng::GeometryFactory::CreateCube(tData->VBO, tData->Size.x, tData->Size.y, tData->Size.z, data->InputLayout);
					else if (tData->Type == pipe::GeometryItem::Circle)
						tData->VAO = eng::GeometryFactory::CreateCircle(tData->VBO, tData->Size.x, tData->Size.y, data->InputLayout);
					else if (tData->Type == pipe::GeometryItem::Plane)
						tData->VAO = eng::GeometryFactory::CreatePlane(tData->VBO, tData->Size.x, tData->Size.y, data->InputLayout);
					else if (tData->Type == pipe::GeometryItem::Rectangle)
						tData->VAO = eng::GeometryFactory::CreatePlane(tData->VBO, 1, 1, data->InputLayout);
					else if (tData->Type == pipe::GeometryItem::Sphere)
						tData->VAO = eng::GeometryFactory::CreateSphere(tData->VBO, tData->Size.x, data->InputLayout);
					else if (tData->Type == pipe::GeometryItem::Triangle)
						tData->VAO = eng::GeometryFactory::CreateTriangle(tData->VBO, tData->Size.x, data->InputLayout);
					else if (tData->Type == pipe::GeometryItem::ScreenQuadNDC)
						tData->VAO = eng::GeometryFactory::CreateScreenQuadNDC(tData->VBO, data->InputLayout);
				} else if (itemType == ed::PipelineItem::ItemType::Model) {
					pipe::Model* tData = reinterpret_cast<pipe::Model*>(itemData);

					//std::string objMem = LoadProjectFile(tData->Filename);
					eng::Model* ptrObject = LoadModel(tData->Filename);
					bool loaded = ptrObject != nullptr;

					if (loaded)
						tData->Data = ptrObject;
					else
						m_msgs->Add(ed::MessageStack::Type::Error, name, "Failed to load .obj model " + std::string(itemName));
				}

				m_pipe->AddItem(name, itemName, itemType, itemData);
			}

			// parse item values
			for (pugi::xml_node itemValueNode : passNode.child("itemvalues").children("value")) {
				std::string type = itemValueNode.attribute("from").as_string();
				const pugi::char_t* valname = itemValueNode.attribute("variable").as_string();
				const pugi::char_t* itemname = itemValueNode.attribute("for").as_string();

				std::vector<ShaderVariable*> vars = data->Variables.GetVariables();

				ShaderVariable* cpyVar = nullptr;
				for (auto& var : vars)
					if (strcmp(var->Name, valname) == 0) {
						cpyVar = var;
						break;
					}

				if (cpyVar != nullptr) {
					PipelineItem* cpyItem = nullptr;
					for (auto& item : data->Items)
						if (strcmp(item->Name, itemname) == 0) {
							cpyItem = item;
							break;
						}

					RenderEngine::ItemVariableValue ival(cpyVar);
					m_parseVariableValue(itemValueNode, ival.NewValue);
					ival.Item = cpyItem;

					m_renderer->AddItemVariableValue(ival);
				}
			}
		}

		// objects
		std::vector<PipelineItem*> passes = m_pipe->GetList();
		std::map<PipelineItem*, std::vector<std::string>> boundTextures;
		for (pugi::xml_node objectNode : projectNode.child("objects").children("object")) {
			const pugi::char_t* objType = objectNode.attribute("type").as_string();

			if (strcmp(objType, "texture") == 0) {
				pugi::char_t name[SHADERED_MAX_PATH];
				bool isCube = false;
				pugi::char_t cubeLeft[SHADERED_MAX_PATH], cubeRight[SHADERED_MAX_PATH], cubeTop[SHADERED_MAX_PATH],
					cubeBottom[SHADERED_MAX_PATH], cubeFront[SHADERED_MAX_PATH], cubeBack[SHADERED_MAX_PATH];
				if (!objectNode.attribute("cube").empty())
					isCube = objectNode.attribute("cube").as_bool();

				if (isCube) {
					strcpy(name, objectNode.attribute("name").as_string());

					strcpy(cubeLeft, toGenericPath(objectNode.attribute("left").as_string()).c_str());
					strcpy(cubeTop, toGenericPath(objectNode.attribute("top").as_string()).c_str());
					strcpy(cubeFront, toGenericPath(objectNode.attribute("front").as_string()).c_str());
					strcpy(cubeBottom, toGenericPath(objectNode.attribute("bottom").as_string()).c_str());
					strcpy(cubeRight, toGenericPath(objectNode.attribute("right").as_string()).c_str());
					strcpy(cubeBack, toGenericPath(objectNode.attribute("back").as_string()).c_str());
				} else
					strcpy(name, toGenericPath(objectNode.attribute("path").as_string()).c_str());

				if (isCube)
					m_objects->CreateCubemap(name, cubeLeft, cubeTop, cubeFront, cubeBottom, cubeRight, cubeBack);
				else
					m_objects->CreateTexture(name);

				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (const auto& pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundTextures[pass].size() <= slot)
								boundTextures[pass].resize(slot + 1);

							boundTextures[pass][slot] = name;
							break;
						}
					}
				}
			} else if (strcmp(objType, "rendertexture") == 0) {
				const pugi::char_t* objName = objectNode.attribute("name").as_string();

				m_objects->CreateRenderTexture(objName);

				ObjectManagerItem* rtData = m_objects->Get(objName);
				RenderTextureObject* rt = rtData->RT;
				rt->Format = GL_RGBA;

				// load size
				if (objectNode.attribute("fsize").empty()) { // load RatioSize if attribute fsize (FixedSize) doesnt exist
					std::string rtSize = objectNode.attribute("rsize").as_string();
					float rtSizeX = std::stof(rtSize.substr(0, rtSize.find(',')));
					float rtSizeY = std::stof(rtSize.substr(rtSize.find(',') + 1));

					rt->RatioSize = glm::vec2(rtSizeX, rtSizeY);
					rt->FixedSize = glm::ivec2(-1, -1);

					m_objects->ResizeRenderTexture(rtData, rt->CalculateSize(m_renderer->GetLastRenderSize().x, m_renderer->GetLastRenderSize().y));
				} else {
					std::string rtSize = objectNode.attribute("fsize").as_string();
					int rtSizeX = std::stoi(rtSize.substr(0, rtSize.find(',')));
					int rtSizeY = std::stoi(rtSize.substr(rtSize.find(',') + 1));

					rt->FixedSize = glm::ivec2(rtSizeX, rtSizeY);

					m_objects->ResizeRenderTexture(rtData, rt->FixedSize);
				}

				// load clear color
				rt->Clear = true;
				if (!objectNode.attribute("r").empty())
					rt->ClearColor.r = objectNode.attribute("r").as_int() / 255.0f;
				else
					rt->ClearColor.r = 0;
				if (!objectNode.attribute("g").empty())
					rt->ClearColor.g = objectNode.attribute("g").as_int() / 255.0f;
				else
					rt->ClearColor.g = 0;
				if (!objectNode.attribute("b").empty())
					rt->ClearColor.b = objectNode.attribute("b").as_int() / 255.0f;
				else
					rt->ClearColor.b = 0;
				if (!objectNode.attribute("a").empty())
					rt->ClearColor.a = objectNode.attribute("a").as_int() / 255.0f;
				else
					rt->ClearColor.a = 0;

				// load binds
				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (const auto& pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundTextures[pass].size() <= slot)
								boundTextures[pass].resize(slot + 1);

							boundTextures[pass][slot] = objName;
							break;
						}
					}
				}
			} else if (strcmp(objType, "audio") == 0) {
				pugi::char_t objPath[SHADERED_MAX_PATH];
				strcpy(objPath, toGenericPath(objectNode.attribute("path").as_string()).c_str());

				m_objects->CreateAudio(std::string(objPath));

				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (const auto& pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundTextures[pass].size() <= slot)
								boundTextures[pass].resize(slot + 1);

							boundTextures[pass][slot] = objPath;
							break;
						}
					}
				}
			}
		}

		// bind objects
		for (const auto& b : boundTextures)
			for (const auto& id : b.second)
				if (!id.empty())
					m_objects->Bind(m_objects->Get(id), b.first);

		// settings
		for (pugi::xml_node settingItem : projectNode.child("settings").children("entry")) {
			if (!settingItem.attribute("type").empty()) {
				std::string type = settingItem.attribute("type").as_string();
				if (type == "property") {
					PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
					if (!settingItem.attribute("name").empty()) {
						PipelineItem* item = m_pipe->Get(settingItem.attribute("name").as_string());
						props->Open(item);
					}
				} else if (type == "file" && Settings::Instance().General.ReopenShaders) {
					CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get(ViewID::Code));
					if (!settingItem.attribute("name").empty()) {
						PipelineItem* item = m_pipe->Get(settingItem.attribute("name").as_string());
						const pugi::char_t* shaderType = settingItem.attribute("shader").as_string();

						std::string path = ((ed::pipe::ShaderPass*)item->Data)->VSPath;

						if (strcmp(shaderType, "ps") == 0)
							path = ((ed::pipe::ShaderPass*)item->Data)->PSPath;
						else if (strcmp(shaderType, "gs") == 0)
							path = ((ed::pipe::ShaderPass*)item->Data)->GSPath;
						else if (strcmp(shaderType, "tcs") == 0)
							path = ((ed::pipe::ShaderPass*)item->Data)->TCSPath;
						else if (strcmp(shaderType, "tes") == 0)
							path = ((ed::pipe::ShaderPass*)item->Data)->TESPath;

						if (strcmp(shaderType, "vs") == 0 && FileExists(path))
							editor->Open(item, ShaderStage::Vertex);
						else if (strcmp(shaderType, "ps") == 0 && FileExists(path))
							editor->Open(item, ShaderStage::Pixel);
						else if (strcmp(shaderType, "gs") == 0 && FileExists(path))
							editor->Open(item, ShaderStage::Geometry);
						else if (strcmp(shaderType, "tcs") == 0 && FileExists(path))
							editor->Open(item, ShaderStage::TessellationControl);
						else if (strcmp(shaderType, "tes") == 0 && FileExists(path))
							editor->Open(item, ShaderStage::TessellationEvaluation);
					}
				} else if (type == "pinned") {
					PinnedUI* pinned = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
					if (!settingItem.attribute("name").empty()) {
						const pugi::char_t* item = settingItem.attribute("name").as_string();
						const pugi::char_t* shaderType = settingItem.attribute("from").as_string();
						pipe::ShaderPass* owner = (pipe::ShaderPass*)(m_pipe->Get(settingItem.attribute("owner").as_string())->Data);

						std::vector<ShaderVariable*> vars = owner->Variables.GetVariables();

						for (const auto& var : vars)
							if (strcmp(var->Name, item) == 0) {
								pinned->Add(var);
								break;
							}
					}
				} else if (type == "camera") {
					if (settingItem.attribute("fp").empty())
						Settings::Instance().Project.FPCamera = false;
					else
						Settings::Instance().Project.FPCamera = settingItem.attribute("fp").as_bool();

					SystemVariableManager::Instance().GetCamera()->Reset();

					bool fp = Settings::Instance().Project.FPCamera;

					if (fp) {
						ed::FirstPersonCamera* fpCam = (ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera();
						fpCam->Reset();
						fpCam->SetPosition(std::stof(settingItem.child("positionX").text().get()),
							std::stof(settingItem.child("positionY").text().get()),
							std::stof(settingItem.child("positionZ").text().get()));
						fpCam->SetYaw(std::stof(settingItem.child("yaw").text().get()));
						fpCam->SetPitch(std::stof(settingItem.child("pitch").text().get()));
					} else {
						ed::ArcBallCamera* ab = (ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera();
						ab->SetDistance(std::stof(settingItem.child("distance").text().get()));
						ab->SetYaw(std::stof(settingItem.child("rotationX").text().get()));
						ab->SetPitch(std::stof(settingItem.child("rotationY").text().get()));
						ab->SetRoll(std::stof(settingItem.child("rotationZ").text().get()));
					}

				} else if (type == "clearcolor") {
					if (!settingItem.attribute("r").empty())
						Settings::Instance().Project.ClearColor.r = settingItem.attribute("r").as_uint() / 255.0f;
					if (!settingItem.attribute("g").empty())
						Settings::Instance().Project.ClearColor.g = settingItem.attribute("g").as_uint() / 255.0f;
					if (!settingItem.attribute("b").empty())
						Settings::Instance().Project.ClearColor.b = settingItem.attribute("b").as_uint() / 255.0f;
					if (!settingItem.attribute("a").empty())
						Settings::Instance().Project.ClearColor.a = settingItem.attribute("a").as_uint() / 255.0f;
				}
			}
		}

		// set actual render texture IDs
		for (auto& pass : fbos) {
			int index = 0;
			for (auto& rtName : pass.second) {
				ObjectManagerItem* rtNameObj = m_objects->Get(rtName);

				GLuint rtID = (rtName == "Window") ? m_renderer->GetTexture() : rtNameObj->Texture;
				pass.first->RenderTextures[index] = rtID;
				index++;
			}
		}
	}
	void ProjectParser::m_parseV2(pugi::xml_node& projectNode)
	{
		Logger::Get().Log("Parsing a V2 project file...");

		Settings::Instance().Project.IncludePaths.clear();

		std::map<pipe::ShaderPass*, std::vector<std::string>> fbos;
		std::map<pipe::GeometryItem*, std::pair<std::string, pipe::ShaderPass*>> geoUBOs; // buffers that are bound to pipeline items
		std::map<pipe::Model*, std::pair<std::string, pipe::ShaderPass*>> modelUBOs;
		std::map<pipe::VertexBuffer*, std::pair<std::string, pipe::ShaderPass*>> vbUBOs;
		std::map<pipe::VertexBuffer*, std::pair<std::string, pipe::ShaderPass*>> vbInstanceUBOs;

		// shader passes
		for (pugi::xml_node passNode : projectNode.child("pipeline").children("pass")) {
			char name[PIPELINE_ITEM_NAME_LENGTH];

			// get pass name
			if (!passNode.attribute("name").empty())
				strcpy(name, passNode.attribute("name").as_string());

			ed::PipelineItem::ItemType type = ed::PipelineItem::ItemType::ShaderPass;
			if (!passNode.attribute("type").empty()) {
				if (strcmp(passNode.attribute("type").as_string(), "compute") == 0)
					type = ed::PipelineItem::ItemType::ComputePass;
				else if (strcmp(passNode.attribute("type").as_string(), "audio") == 0)
					type = ed::PipelineItem::ItemType::AudioPass;
				else if (strcmp(passNode.attribute("type").as_string(), "plugin") == 0)
					type = ed::PipelineItem::ItemType::PluginItem;
			}

			if (type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* data = new ed::pipe::ShaderPass();

				data->RenderTextures[0] = m_renderer->GetTexture();
				for (int i = 1; i < MAX_RENDER_TEXTURES; i++)
					data->RenderTextures[i] = 0;

				data->Active = true;
				if (!passNode.attribute("active").empty())
					data->Active = passNode.attribute("active").as_bool();

				// check if it should be collapsed
				if (!passNode.attribute("collapsed").empty()) {
					bool cs = passNode.attribute("collapsed").as_bool();
					if (cs)
						((PipelineUI*)m_ui->Get(ViewID::Pipeline))->Collapse(data);
				}

				// patch vertex count
				if (!passNode.attribute("patchverts").empty())
					data->TSPatchVertices = std::max<int>(1, std::min<int>(m_renderer->GetMaxPatchVertices(), passNode.attribute("patchverts").as_int()));

				// get render textures
				int rtCur = 0;
				for (pugi::xml_node rtNode : passNode.children("rendertexture")) {
					std::string rtName("");
					if (!rtNode.attribute("name").empty())
						rtName = std::string(rtNode.attribute("name").as_string());
					fbos[data].push_back(rtName);
					rtCur++;
				}
				data->RTCount = (rtCur == 0) ? 1 : rtCur;

				// add the item
				m_pipe->AddShaderPass(name, data);

				// get shader properties (NOTE: a shader must have TYPE, PATH and ENTRY)
				for (pugi::xml_node shaderNode : passNode.children("shader")) {
					std::string shaderNodeType(shaderNode.attribute("type").as_string()); // "vs" or "ps" or "gs"

					// parse path and type
					pugi::char_t shaderPath[SHADERED_MAX_PATH];
					strcpy(shaderPath, toGenericPath(shaderNode.attribute("path").as_string()).c_str());
					const pugi::char_t* shaderEntry = shaderNode.attribute("entry").as_string();

					if (shaderNodeType == "vs") {
						strcpy(data->VSPath, shaderPath);
						strcpy(data->VSEntry, shaderEntry);
					} else if (shaderNodeType == "ps") {
						strcpy(data->PSPath, shaderPath);
						strcpy(data->PSEntry, shaderEntry);
					} else if (shaderNodeType == "gs") {
						if (!shaderNode.attribute("used").empty())
							data->GSUsed = shaderNode.attribute("used").as_bool();
						else
							data->GSUsed = false;
						strcpy(data->GSPath, shaderPath);
						strcpy(data->GSEntry, shaderEntry);
					} else if (shaderNodeType == "tcs") {
						if (!shaderNode.attribute("used").empty())
							data->TSUsed = shaderNode.attribute("used").as_bool();
						else
							data->TSUsed = false;
						strcpy(data->TCSPath, shaderPath);
						strcpy(data->TCSEntry, shaderEntry);
					} else if (shaderNodeType == "tes") {
						if (!shaderNode.attribute("used").empty())
							data->TSUsed = shaderNode.attribute("used").as_bool();
						else
							data->TSUsed = false;
						strcpy(data->TESPath, shaderPath);
						strcpy(data->TESEntry, shaderEntry);
					}

					std::string type = ((shaderNodeType == "vs") ? "vertex" : ((shaderNodeType == "ps") ? "pixel" : "geometry"));
					if (!FileExists(shaderPath))
						m_msgs->Add(ed::MessageStack::Type::Error, name, type + " shader does not exist.");
				}

				// parse variables
				for (pugi::xml_node variableNode : passNode.child("variables").children("variable")) {
					ShaderVariable::ValueType type = ShaderVariable::ValueType::Float1;
					SystemShaderVariable system = SystemShaderVariable::None;
					FunctionShaderVariable func = FunctionShaderVariable::None;
					PluginSystemVariableData pluginSysData;
					PluginFunctionData pluginFuncData;

					char flags = 0;

					/* FLAGS */
					bool isInvert = false, isLastFrame = false;

					if (!variableNode.attribute("invert").empty())
						isInvert = variableNode.attribute("invert").as_bool();
					if (!variableNode.attribute("lastframe").empty())
						isLastFrame = variableNode.attribute("lastframe").as_bool();

					flags = (isInvert * (char)ShaderVariable::Flag::Inverse) | (isLastFrame * (char)ShaderVariable::Flag::LastFrame);

					/* TYPE */
					if (!variableNode.attribute("type").empty()) {
						const char* myType = variableNode.attribute("type").as_string();
						for (int i = 0; i < HARRAYSIZE(VARIABLE_TYPE_NAMES); i++)
							if (strcmp(myType, VARIABLE_TYPE_NAMES[i]) == 0) {
								type = (ed::ShaderVariable::ValueType)i;
								break;
							}
					}
					if (!variableNode.attribute("system").empty()) {
						const char* mySystem = variableNode.attribute("system").as_string();
						for (int i = 0; i < HARRAYSIZE(SYSTEM_VARIABLE_NAMES); i++)
							if (strcmp(mySystem, SYSTEM_VARIABLE_NAMES[i]) == 0) {
								system = (ed::SystemShaderVariable)i;
								break;
							}
						if (system == SystemShaderVariable::PluginVariable) {
							const char* ownerName = variableNode.attribute("plugin").as_string();
							const char* psVarName = variableNode.attribute("itemname").as_string();
							strcpy(pluginSysData.Name, psVarName);
							pluginSysData.Owner = m_plugins->GetPlugin(ownerName);
						} else if (SystemVariableManager::GetType(system) != type)
							system = ed::SystemShaderVariable::None;
					}
					if (!variableNode.attribute("function").empty()) {
						const char* myFunc = variableNode.attribute("function").as_string();
						for (int i = 0; i < HARRAYSIZE(FUNCTION_NAMES); i++)
							if (strcmp(myFunc, FUNCTION_NAMES[i]) == 0) {
								func = (FunctionShaderVariable)i;
								break;
							}
						if (func == FunctionShaderVariable::PluginFunction) {
							const char* ownerName = variableNode.attribute("plugin").as_string();
							const char* pfuncName = variableNode.attribute("funcname").as_string();
							strcpy(pluginFuncData.Name, pfuncName);
							pluginFuncData.Owner = m_plugins->GetPlugin(ownerName);
						} else {
							if (system != SystemShaderVariable::None || !FunctionVariableManager::HasValidReturnType(type, func))
								func = FunctionShaderVariable::None;
						}
					}

					ShaderVariable* var = new ShaderVariable(type, variableNode.attribute("name").as_string(), system);
					var->Flags = flags;
					memcpy(&var->PluginSystemVarData, &pluginSysData, sizeof(PluginSystemVariableData));
					memcpy(&var->PluginFuncData, &pluginFuncData, sizeof(PluginFunctionData));
					FunctionVariableManager::AllocateArgumentSpace(var, func);

					// parse value
					if (system == SystemShaderVariable::None)
						m_parseVariableValue(variableNode, var);

					data->Variables.Add(var);
				}

				// input layout
				for (pugi::xml_node lItemNode : passNode.child("inputlayout").children("item")) {
					char ITEM_VALUE_NAME[32];
					char ITEM_SEMANTIC_NAME[32];

					if (!lItemNode.attribute("value").empty())
						strcpy(ITEM_VALUE_NAME, lItemNode.attribute("value").as_string());

					if (!lItemNode.attribute("semantic").empty())
						strcpy(ITEM_SEMANTIC_NAME, lItemNode.attribute("semantic").as_string());

					InputLayoutValue lValue = InputLayoutValue::Position;
					for (int k = 0; k < (int)InputLayoutValue::MaxCount; k++)
						if (strcmp(ITEM_VALUE_NAME, ATTRIBUTE_VALUE_NAMES[k]) == 0)
							lValue = (InputLayoutValue)k;

					data->InputLayout.push_back({ lValue, std::string(ITEM_SEMANTIC_NAME) });
				}

				if (data->InputLayout.size() == 0)
					data->InputLayout = gl::CreateDefaultInputLayout();

				// macros
				for (pugi::xml_node macroNode : passNode.child("macros").children("define")) {
					ShaderMacro newMacro;
					if (!macroNode.attribute("name").empty())
						strcpy(newMacro.Name, macroNode.attribute("name").as_string());

					newMacro.Active = true;
					if (!macroNode.attribute("active").empty())
						newMacro.Active = macroNode.attribute("active").as_bool();
					strcpy(newMacro.Value, macroNode.text().get());
					data->Macros.push_back(newMacro);
				}

				// parse items
				m_importItems(name, data, passNode.child("items"), data->InputLayout, geoUBOs, modelUBOs, vbUBOs, vbInstanceUBOs);

				// parse item values
				for (pugi::xml_node itemValueNode : passNode.child("itemvalues").children("value")) {
					std::string type = itemValueNode.attribute("from").as_string();
					const pugi::char_t* valname = itemValueNode.attribute("variable").as_string();
					const pugi::char_t* itemname = itemValueNode.attribute("for").as_string();

					std::vector<ShaderVariable*> vars = data->Variables.GetVariables();

					ShaderVariable* cpyVar = nullptr;
					for (auto& var : vars)
						if (strcmp(var->Name, valname) == 0) {
							cpyVar = var;
							break;
						}

					if (cpyVar != nullptr) {
						PipelineItem* cpyItem = nullptr;
						for (auto& item : data->Items)
							if (strcmp(item->Name, itemname) == 0) {
								cpyItem = item;
								break;
							}

						RenderEngine::ItemVariableValue ival(cpyVar);
						m_parseVariableValue(itemValueNode, ival.NewValue);
						ival.Item = cpyItem;

						m_renderer->AddItemVariableValue(ival);
					}
				}
			} else if (type == PipelineItem::ItemType::ComputePass) {
				ed::pipe::ComputePass* data = new ed::pipe::ComputePass();

				data->Active = true;
				if (!passNode.attribute("active").empty())
					data->Active = passNode.attribute("active").as_bool();

				// get shader properties (NOTE: a shader must have TYPE, PATH and ENTRY)
				for (pugi::xml_node shaderNode : passNode.children("shader")) {
					// parse path and type
					pugi::char_t shaderPath[SHADERED_MAX_PATH];
					strcpy(shaderPath, toGenericPath(shaderNode.attribute("path").as_string()).c_str());
					const pugi::char_t* shaderEntry = shaderNode.attribute("entry").as_string();

					strcpy(data->Path, shaderPath);
					strcpy(data->Entry, shaderEntry);

					if (!FileExists(shaderPath))
						m_msgs->Add(ed::MessageStack::Type::Error, name, "Compute shader does not exist.");
				}

				// parse variables
				for (pugi::xml_node variableNode : passNode.child("variables").children("variable")) {
					ShaderVariable::ValueType type = ShaderVariable::ValueType::Float1;
					SystemShaderVariable system = SystemShaderVariable::None;
					FunctionShaderVariable func = FunctionShaderVariable::None;
					char flags = 0;

					/* FLAGS */
					bool isInvert = false, isLastFrame = false;

					if (!variableNode.attribute("invert").empty())
						isInvert = variableNode.attribute("invert").as_bool();
					if (!variableNode.attribute("lastframe").empty())
						isLastFrame = variableNode.attribute("lastframe").as_bool();

					flags = (isInvert * (char)ShaderVariable::Flag::Inverse) | (isLastFrame * (char)ShaderVariable::Flag::LastFrame);

					/* TYPE */
					if (!variableNode.attribute("type").empty()) {
						const char* myType = variableNode.attribute("type").as_string();
						for (int i = 0; i < HARRAYSIZE(VARIABLE_TYPE_NAMES); i++)
							if (strcmp(myType, VARIABLE_TYPE_NAMES[i]) == 0) {
								type = (ed::ShaderVariable::ValueType)i;
								break;
							}
					}
					if (!variableNode.attribute("system").empty()) {
						const char* mySystem = variableNode.attribute("system").as_string();
						for (int i = 0; i < HARRAYSIZE(SYSTEM_VARIABLE_NAMES); i++)
							if (strcmp(mySystem, SYSTEM_VARIABLE_NAMES[i]) == 0) {
								system = (ed::SystemShaderVariable)i;
								break;
							}
						if (SystemVariableManager::GetType(system) != type)
							system = ed::SystemShaderVariable::None;
					}
					if (!variableNode.attribute("function").empty()) {
						const char* myFunc = variableNode.attribute("function").as_string();
						for (int i = 0; i < HARRAYSIZE(FUNCTION_NAMES); i++)
							if (strcmp(myFunc, FUNCTION_NAMES[i]) == 0) {
								func = (FunctionShaderVariable)i;
								break;
							}
						if (system != SystemShaderVariable::None || !FunctionVariableManager::HasValidReturnType(type, func))
							func = FunctionShaderVariable::None;
					}

					ShaderVariable* var = new ShaderVariable(type, variableNode.attribute("name").as_string(), system);
					var->Flags = flags;
					FunctionVariableManager::AllocateArgumentSpace(var, func);

					// parse value
					if (system == SystemShaderVariable::None)
						m_parseVariableValue(variableNode, var);

					data->Variables.Add(var);
				}

				// macros
				for (pugi::xml_node macroNode : passNode.child("macros").children("define")) {
					ShaderMacro newMacro;
					if (!macroNode.attribute("name").empty())
						strcpy(newMacro.Name, macroNode.attribute("name").as_string());

					newMacro.Active = true;
					if (!macroNode.attribute("active").empty())
						newMacro.Active = macroNode.attribute("active").as_bool();
					strcpy(newMacro.Value, macroNode.text().get());
					data->Macros.push_back(newMacro);
				}

				// get group size
				pugi::xml_node workNode = passNode.child("groupsize");
				if (!workNode.attribute("x").empty())
					data->WorkX = workNode.attribute("x").as_uint();
				else
					data->WorkX = 1;
				if (!workNode.attribute("y").empty())
					data->WorkY = workNode.attribute("y").as_uint();
				else
					data->WorkY = 1;
				if (!workNode.attribute("z").empty())
					data->WorkZ = workNode.attribute("z").as_uint();
				else
					data->WorkZ = 1;

				// add the item
				m_pipe->AddComputePass(name, data);
			} else if (type == PipelineItem::ItemType::AudioPass) {
				ed::pipe::AudioPass* data = new ed::pipe::AudioPass();

				// get shader properties (NOTE: a shader must have TYPE, PATH and ENTRY)
				for (pugi::xml_node shaderNode : passNode.children("shader")) {
					// parse path and type
					pugi::char_t shaderPath[SHADERED_MAX_PATH];
					strcpy(shaderPath, toGenericPath(shaderNode.attribute("path").as_string()).c_str());

					strcpy(data->Path, shaderPath);

					if (!FileExists(shaderPath))
						m_msgs->Add(ed::MessageStack::Type::Error, name, "Audio shader does not exist.");
				}

				// parse variables
				for (pugi::xml_node variableNode : passNode.child("variables").children("variable")) {
					ShaderVariable::ValueType type = ShaderVariable::ValueType::Float1;
					SystemShaderVariable system = SystemShaderVariable::None;
					FunctionShaderVariable func = FunctionShaderVariable::None;
					char flags = 0;

					/* FLAGS */
					bool isInvert = false, isLastFrame = false;

					if (!variableNode.attribute("invert").empty())
						isInvert = variableNode.attribute("invert").as_bool();
					if (!variableNode.attribute("lastframe").empty())
						isLastFrame = variableNode.attribute("lastframe").as_bool();

					flags = (isInvert * (char)ShaderVariable::Flag::Inverse) | (isLastFrame * (char)ShaderVariable::Flag::LastFrame);

					/* TYPE */
					if (!variableNode.attribute("type").empty()) {
						const char* myType = variableNode.attribute("type").as_string();
						for (int i = 0; i < HARRAYSIZE(VARIABLE_TYPE_NAMES); i++)
							if (strcmp(myType, VARIABLE_TYPE_NAMES[i]) == 0) {
								type = (ed::ShaderVariable::ValueType)i;
								break;
							}
					}
					if (!variableNode.attribute("system").empty()) {
						const char* mySystem = variableNode.attribute("system").as_string();
						for (int i = 0; i < HARRAYSIZE(SYSTEM_VARIABLE_NAMES); i++)
							if (strcmp(mySystem, SYSTEM_VARIABLE_NAMES[i]) == 0) {
								system = (ed::SystemShaderVariable)i;
								break;
							}
						if (SystemVariableManager::GetType(system) != type)
							system = ed::SystemShaderVariable::None;
					}
					if (!variableNode.attribute("function").empty()) {
						const char* myFunc = variableNode.attribute("function").as_string();
						for (int i = 0; i < HARRAYSIZE(FUNCTION_NAMES); i++)
							if (strcmp(myFunc, FUNCTION_NAMES[i]) == 0) {
								func = (FunctionShaderVariable)i;
								break;
							}
						if (system != SystemShaderVariable::None || !FunctionVariableManager::HasValidReturnType(type, func))
							func = FunctionShaderVariable::None;
					}

					ShaderVariable* var = new ShaderVariable(type, variableNode.attribute("name").as_string(), system);
					var->Flags = flags;
					FunctionVariableManager::AllocateArgumentSpace(var, func);

					// parse value
					if (system == SystemShaderVariable::None)
						m_parseVariableValue(variableNode, var);

					data->Variables.Add(var);
				}

				// macros
				for (pugi::xml_node macroNode : passNode.child("macros").children("define")) {
					ShaderMacro newMacro;
					if (!macroNode.attribute("name").empty())
						strcpy(newMacro.Name, macroNode.attribute("name").as_string());

					newMacro.Active = true;
					if (!macroNode.attribute("active").empty())
						newMacro.Active = macroNode.attribute("active").as_bool();
					strcpy(newMacro.Value, macroNode.text().get());
					data->Macros.push_back(newMacro);
				}

				// add the item
				m_pipe->AddAudioPass(name, data);
			} else if (type == PipelineItem::ItemType::PluginItem) {
				IPlugin1* plugin = m_plugins->GetPlugin(passNode.attribute("plugin").as_string());
				std::string otype(passNode.attribute("itemtype").as_string());

				void* pluginData = plugin->PipelineItem_Import(nullptr, name, otype.c_str(), getInnerXML(passNode.child("data")).c_str());

				// add the item
				m_pipe->AddPluginItem(nullptr, name, otype.c_str(), pluginData, plugin);

				m_importItems(name, nullptr, passNode.child("items"), m_plugins->BuildInputLayout(plugin, otype.c_str(), pluginData), geoUBOs, modelUBOs, vbUBOs, vbInstanceUBOs);
			}
		}

		// camera snapshots
		for (pugi::xml_node camNode : projectNode.child("cameras").children("camera")) {
			std::string camName = "";
			glm::mat4 camMat(1);

			if (!camNode.attribute("name").empty())
				camName = camNode.attribute("name").as_string();

			int rowID = 0;
			for (pugi::xml_node row : camNode.children("row")) {
				int colID = 0;
				for (pugi::xml_node value : row.children("value")) {
					camMat[colID][rowID] = value.text().as_float();
					colID = (colID + 1) % 4;
				}
				rowID++;
			}

			CameraSnapshots::Add(camName, camMat);
		}

		// objects
		std::vector<PipelineItem*> passes = m_pipe->GetList();
		std::map<PipelineItem*, std::vector<std::string>> boundTextures, boundUBOs;
		for (pugi::xml_node objectNode : projectNode.child("objects").children("object")) {
			const pugi::char_t* objType = objectNode.attribute("type").as_string();

			if (strcmp(objType, "texture") == 0) {
				pugi::char_t name[SHADERED_MAX_PATH];
				bool isCube = false;
				bool isKeyboardTexture = false;
				bool is3D = false;
				pugi::char_t cubeLeft[SHADERED_MAX_PATH], cubeRight[SHADERED_MAX_PATH], cubeTop[SHADERED_MAX_PATH],
					cubeBottom[SHADERED_MAX_PATH], cubeFront[SHADERED_MAX_PATH], cubeBack[SHADERED_MAX_PATH];
				
				if (!objectNode.attribute("cube").empty())
					isCube = objectNode.attribute("cube").as_bool();
				if (!objectNode.attribute("keyboard_texture").empty())
					isKeyboardTexture = objectNode.attribute("keyboard_texture").as_bool();
				if (!objectNode.attribute("is_3d").empty())
					is3D = objectNode.attribute("is_3d").as_bool();

				if (isCube || isKeyboardTexture)
					strcpy(name, objectNode.attribute("name").as_string());
				else
					strcpy(name, toGenericPath(objectNode.attribute("path").as_string()).c_str());

				if (isCube) {
					strcpy(cubeLeft, toGenericPath(objectNode.attribute("left").as_string()).c_str());
					strcpy(cubeTop, toGenericPath(objectNode.attribute("top").as_string()).c_str());
					strcpy(cubeFront, toGenericPath(objectNode.attribute("front").as_string()).c_str());
					strcpy(cubeBottom, toGenericPath(objectNode.attribute("bottom").as_string()).c_str());
					strcpy(cubeRight, toGenericPath(objectNode.attribute("right").as_string()).c_str());
					strcpy(cubeBack, toGenericPath(objectNode.attribute("back").as_string()).c_str());
				}

				if (isCube)
					m_objects->CreateCubemap(name, cubeLeft, cubeTop, cubeFront, cubeBottom, cubeRight, cubeBack);
				else if (isKeyboardTexture)
					m_objects->CreateKeyboardTexture(name);
				else if (is3D)
					m_objects->CreateTexture3D(name);
				else
					m_objects->CreateTexture(name);

				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (const auto& pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundTextures[pass].size() <= slot)
								boundTextures[pass].resize(slot + 1);

							boundTextures[pass][slot] = name;

							break;
						}
					}
				}

				// texture properties
				if (!isCube) {
					ObjectManagerItem* itemData = m_objects->Get(name);
					if (itemData != nullptr) {
						// vflip
						bool vflip = false;
						if (!objectNode.attribute("vflip").empty())
							vflip = objectNode.attribute("vflip").as_bool();
						if (vflip)
							m_objects->FlipTexture(name);

						// min filter
						if (!objectNode.attribute("min_filter").empty()) {
							auto filterName = objectNode.attribute("min_filter").as_string();
							for (int i = 0; i < HARRAYSIZE(TEXTURE_MIN_FILTER_VALUES); i++)
								if (strcmp(filterName, TEXTURE_MIN_FILTER_NAMES[i]) == 0) {
									itemData->Texture_MinFilter = TEXTURE_MIN_FILTER_VALUES[i];
									break;
								}
						}

						// mag filter
						if (!objectNode.attribute("mag_filter").empty()) {
							auto filterName = objectNode.attribute("mag_filter").as_string();
							for (int i = 0; i < HARRAYSIZE(TEXTURE_MAG_FILTER_VALUES); i++)
								if (strcmp(filterName, TEXTURE_MAG_FILTER_NAMES[i]) == 0) {
									itemData->Texture_MagFilter = TEXTURE_MAG_FILTER_VALUES[i];
									break;
								}
						}

						// wrap x
						if (!objectNode.attribute("wrap_s").empty()) {
							auto filterName = objectNode.attribute("wrap_s").as_string();
							for (int i = 0; i < HARRAYSIZE(TEXTURE_WRAP_VALUES); i++)
								if (strcmp(filterName, TEXTURE_WRAP_NAMES[i]) == 0) {
									itemData->Texture_WrapS = TEXTURE_WRAP_VALUES[i];
									break;
								}
						}

						// wrap y
						if (!objectNode.attribute("wrap_t").empty()) {
							auto filterName = objectNode.attribute("wrap_t").as_string();
							for (int i = 0; i < HARRAYSIZE(TEXTURE_WRAP_VALUES); i++)
								if (strcmp(filterName, TEXTURE_WRAP_NAMES[i]) == 0) {
									itemData->Texture_WrapT = TEXTURE_WRAP_VALUES[i];
									break;
								}
						}

						if (is3D) {
							// wrap z
							if (!objectNode.attribute("wrap_r").empty()) {
								auto filterName = objectNode.attribute("wrap_r").as_string();
								for (int i = 0; i < HARRAYSIZE(TEXTURE_WRAP_VALUES); i++)
									if (strcmp(filterName, TEXTURE_WRAP_NAMES[i]) == 0) {
										itemData->Texture_WrapT = TEXTURE_WRAP_VALUES[i];
										break;
									}
							}
						}

						m_objects->UpdateTextureParameters(name);
					}
				}
			} else if (strcmp(objType, "rendertexture") == 0) {
				const pugi::char_t* objName = objectNode.attribute("name").as_string();

				m_objects->CreateRenderTexture(objName);

				ObjectManagerItem* rtData = m_objects->Get(objName);
				RenderTextureObject* rt = rtData->RT;

				// load format
				if (!objectNode.attribute("format").empty()) {
					auto formatName = objectNode.attribute("format").as_string();
					for (int i = 0; i < HARRAYSIZE(FORMAT_NAMES); i++) {
						if (strcmp(formatName, FORMAT_NAMES[i]) == 0) {
							rt->Format = FORMAT_VALUES[i];
							break;
						}
					}
				}

				// load size
				if (objectNode.attribute("fsize").empty()) { // load RatioSize if attribute fsize (FixedSize) doesnt exist
					std::string rtSize = objectNode.attribute("rsize").as_string();
					float rtSizeX = std::stof(rtSize.substr(0, rtSize.find(',')));
					float rtSizeY = std::stof(rtSize.substr(rtSize.find(',') + 1));

					rt->RatioSize = glm::vec2(rtSizeX, rtSizeY);
					rt->FixedSize = glm::ivec2(-1, -1);

					m_objects->ResizeRenderTexture(rtData, rt->CalculateSize(m_renderer->GetLastRenderSize().x, m_renderer->GetLastRenderSize().y));
				} else {
					std::string rtSize = objectNode.attribute("fsize").as_string();
					int rtSizeX = std::stoi(rtSize.substr(0, rtSize.find(',')));
					int rtSizeY = std::stoi(rtSize.substr(rtSize.find(',') + 1));

					rt->FixedSize = glm::ivec2(rtSizeX, rtSizeY);

					m_objects->ResizeRenderTexture(rtData, rt->FixedSize);
				}

				// load clear flag
				rt->Clear = true;
				if (!objectNode.attribute("clear").empty())
					rt->Clear = objectNode.attribute("clear").as_bool();

				// load clear color
				if (!objectNode.attribute("r").empty())
					rt->ClearColor.r = objectNode.attribute("r").as_float();
				else
					rt->ClearColor.r = 0;
				if (!objectNode.attribute("g").empty())
					rt->ClearColor.g = objectNode.attribute("g").as_float();
				else
					rt->ClearColor.g = 0;
				if (!objectNode.attribute("b").empty())
					rt->ClearColor.b = objectNode.attribute("b").as_float();
				else
					rt->ClearColor.b = 0;
				if (!objectNode.attribute("a").empty())
					rt->ClearColor.a = objectNode.attribute("a").as_float();
				else
					rt->ClearColor.a = 0;

				// load binds
				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (const auto& pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundTextures[pass].size() <= slot)
								boundTextures[pass].resize(slot + 1);

							boundTextures[pass][slot] = objName;
							break;
						}
					}
				}
			} else if (strcmp(objType, "image") == 0) {
				const pugi::char_t* objName = objectNode.attribute("name").as_string();

				m_objects->CreateImage(objName);
				ObjectManagerItem* iobjOwner = m_objects->Get(objName);
				ImageObject* iobj = iobjOwner->Image;

				// data path
				if (!objectNode.attribute("data").empty())
					strcpy(iobj->DataPath, objectNode.attribute("data").as_string());

				// load format
				if (!objectNode.attribute("format").empty()) {
					auto formatName = objectNode.attribute("format").as_string();
					for (int i = 0; i < HARRAYSIZE(FORMAT_NAMES); i++)
						if (strcmp(formatName, FORMAT_NAMES[i]) == 0) {
							iobj->Format = FORMAT_VALUES[i];
							break;
						}
				}

				// load size
				if (!objectNode.attribute("width").empty())
					iobj->Size.x = objectNode.attribute("width").as_int();
				if (!objectNode.attribute("height").empty())
					iobj->Size.y = objectNode.attribute("height").as_int();
				m_objects->ResizeImage(iobjOwner, iobj->Size);

				// load binds
				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();
					int isUAV = -1;
					if (!bindNode.attribute("uav").empty())
						isUAV = bindNode.attribute("uav").as_int();

					for (const auto& pass : passes) {
						// bind as sampler2D
						if ((isUAV == -1 && pass->Type == PipelineItem::ItemType::ShaderPass) || isUAV == 0) {
							if (strcmp(pass->Name, passBindName) == 0) {
								if (boundTextures[pass].size() <= slot)
									boundTextures[pass].resize(slot + 1);

								boundTextures[pass][slot] = objName;
								break;
							}
							// bind as image2D
						} else if ((isUAV == -1 && pass->Type == PipelineItem::ItemType::ComputePass) || isUAV == 1) {
							if (strcmp(pass->Name, passBindName) == 0) {
								if (boundUBOs[pass].size() <= slot)
									boundUBOs[pass].resize(slot + 1);

								boundUBOs[pass][slot] = objName;
								break;
							}
						}
					}
				}
			} else if (strcmp(objType, "image3d") == 0) {
				const pugi::char_t* objName = objectNode.attribute("name").as_string();

				m_objects->CreateImage3D(objName);
				ObjectManagerItem* iobjOwner = m_objects->Get(objName);
				Image3DObject* iobj = iobjOwner->Image3D;

				// load format
				if (!objectNode.attribute("format").empty()) {
					auto formatName = objectNode.attribute("format").as_string();
					for (int i = 0; i < HARRAYSIZE(FORMAT_NAMES); i++)
						if (strcmp(formatName, FORMAT_NAMES[i]) == 0) {
							iobj->Format = FORMAT_VALUES[i];
							break;
						}
				}

				// load size
				if (!objectNode.attribute("width").empty())
					iobj->Size.x = objectNode.attribute("width").as_int();
				if (!objectNode.attribute("height").empty())
					iobj->Size.y = objectNode.attribute("height").as_int();
				if (!objectNode.attribute("depth").empty())
					iobj->Size.z = objectNode.attribute("depth").as_int();
				m_objects->ResizeImage3D(iobjOwner, iobj->Size);

				// load binds
				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();
					int isUAV = -1;
					if (!bindNode.attribute("uav").empty())
						isUAV = bindNode.attribute("uav").as_int();

					for (const auto& pass : passes) {
						// bind as sampler2D
						if ((isUAV == -1 && pass->Type == PipelineItem::ItemType::ShaderPass) || isUAV == 0) {
							if (strcmp(pass->Name, passBindName) == 0) {
								if (boundTextures[pass].size() <= slot)
									boundTextures[pass].resize(slot + 1);

								boundTextures[pass][slot] = objName;
								break;
							}
							// bind as image2D
						} else if ((isUAV == -1 && pass->Type == PipelineItem::ItemType::ComputePass) || isUAV == 1) {
							if (strcmp(pass->Name, passBindName) == 0) {
								if (boundUBOs[pass].size() <= slot)
									boundUBOs[pass].resize(slot + 1);

								boundUBOs[pass][slot] = objName;
								break;
							}
						}
					}
				}
			} else if (strcmp(objType, "audio") == 0) {
				pugi::char_t objPath[SHADERED_MAX_PATH];
				strcpy(objPath, toGenericPath(objectNode.attribute("path").as_string()).c_str());

				m_objects->CreateAudio(std::string(objPath));

				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (const auto& pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundTextures[pass].size() <= slot)
								boundTextures[pass].resize(slot + 1);

							boundTextures[pass][slot] = objPath;
							break;
						}
					}
				}
			} else if (strcmp(objType, "buffer") == 0) {
				const pugi::char_t* objName = objectNode.attribute("name").as_string();

				m_objects->CreateBuffer(objName);
				ed::BufferObject* buf = m_objects->Get(objName)->Buffer;

				if (!objectNode.attribute("size").empty()) {
					buf->Size = objectNode.attribute("size").as_int();
					buf->Data = realloc(buf->Data, buf->Size);
				}
				if (!objectNode.attribute("format").empty())
					strcpy(buf->ViewFormat, objectNode.attribute("format").as_string());

				if (!objectNode.attribute("pausedpreview").empty())
					buf->PreviewPaused = objectNode.attribute("pausedpreview").as_bool();

				std::string bPath = GetProjectPath("buffers/" + std::string(objName) + ".buf");
				std::ifstream bufRead(bPath, std::ios::binary);
				if (bufRead.is_open())
					bufRead.read((char*)buf->Data, buf->Size);
				bufRead.close();

				glBindBuffer(GL_UNIFORM_BUFFER, buf->ID);
				glBufferData(GL_UNIFORM_BUFFER, buf->Size, buf->Data, GL_STATIC_DRAW); // allocate 0 bytes of memory
				glBindBuffer(GL_UNIFORM_BUFFER, 0);

				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (const auto& pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundUBOs[pass].size() <= slot)
								boundUBOs[pass].resize(slot + 1);

							boundUBOs[pass][slot] = objName;
							break;
						}
					}
				}
			} else if (strcmp(objType, "pluginobject") == 0) {
				const pugi::char_t* objName = objectNode.attribute("name").as_string();

				IPlugin1* plugin = m_plugins->GetPlugin(objectNode.attribute("plugin").as_string());
				std::string otype(objectNode.attribute("objecttype").as_string());

				plugin->Object_Import(objName, otype.c_str(), getInnerXML(objectNode).c_str());

				if (plugin->Object_IsBindableUAV(otype.c_str())) {
					for (pugi::xml_node bindNode : objectNode.children("bind")) {
						const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
						int slot = bindNode.attribute("slot").as_int();

						for (const auto& pass : passes) {
							if (strcmp(pass->Name, passBindName) == 0) {
								if (boundUBOs[pass].size() <= slot)
									boundUBOs[pass].resize(slot + 1);

								boundUBOs[pass][slot] = objName;
								break;
							}
						}
					}
				} else {
					for (pugi::xml_node bindNode : objectNode.children("bind")) {
						const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
						int slot = bindNode.attribute("slot").as_int();

						for (const auto& pass : passes) {
							if (strcmp(pass->Name, passBindName) == 0) {
								if (boundTextures[pass].size() <= slot)
									boundTextures[pass].resize(slot + 1);

								boundTextures[pass][slot] = objName;

								break;
							}
						}
					}
				}
			}
		}

		// bind ARRAY_BUFFERS
		for (auto& geo : geoUBOs) {
			BufferObject* bojb = m_objects->Get(geo.second.first)->Buffer;
			geo.first->InstanceBuffer = bojb;
			gl::CreateVAO(geo.first->VAO, geo.first->VBO, geo.second.second->InputLayout, 0, bojb->ID, m_objects->ParseBufferFormat(bojb->ViewFormat));
		}
		for (auto& mdl : modelUBOs) {
			if (mdl.second.first.size() > 0) {
				BufferObject* bobj = m_objects->Get(mdl.second.first)->Buffer;
				mdl.first->InstanceBuffer = bobj;

				for (auto& mesh : mdl.first->Data->Meshes)
					gl::CreateVAO(mesh.VAO, mesh.VBO, mdl.second.second->InputLayout, mesh.EBO, bobj->ID, m_objects->ParseBufferFormat(bobj->ViewFormat));
			} else { // recreate vao anyway
				for (auto& mesh : mdl.first->Data->Meshes)
					gl::CreateVAO(mesh.VAO, mesh.VBO, mdl.second.second->InputLayout, mesh.EBO);
			}
		}
		for (auto& vb : vbUBOs) {
			BufferObject* bobj = m_objects->Get(vb.second.first)->Buffer;
			vb.first->Buffer = bobj;

			if (bobj) {
				GLuint ibufID = 0;
				std::vector<ShaderVariable::ValueType> ibufFormat;

				if (vbInstanceUBOs.count(vb.first) != 0) {
					BufferObject* ibufobj = m_objects->Get(vbInstanceUBOs[vb.first].first)->Buffer;
					ibufID = ibufobj->ID;
					ibufFormat = m_objects->ParseBufferFormat(ibufobj->ViewFormat);
				}

				gl::CreateBufferVAO(vb.first->VAO, bobj->ID, m_objects->ParseBufferFormat(bobj->ViewFormat), ibufID, ibufFormat);
				
			}
		}

		// bind objects
		for (const auto& b : boundTextures)
			for (const auto& id : b.second)
				if (!id.empty())
					m_objects->Bind(m_objects->Get(id), b.first);
		// bind buffers
		for (const auto& b : boundUBOs)
			for (const auto& id : b.second)
				if (!id.empty())
					m_objects->BindUniform(m_objects->Get(id), b.first);

		// settings
		for (pugi::xml_node settingItem : projectNode.child("settings").children("entry")) {
			if (!settingItem.attribute("type").empty()) {
				std::string type = settingItem.attribute("type").as_string();
				if (type == "property") {
					PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
					if (!settingItem.attribute("name").empty()) {
						int type = 0; // pipeline item
						if (!settingItem.attribute("item").empty()) {
							const pugi::char_t* itemType = settingItem.attribute("item").as_string();
							if (strcmp(itemType, "pipe") == 0)
								type = 0;
							else
								type = 1;
						}

						const pugi::char_t* itemName = settingItem.attribute("name").as_string();
						if (type == 0) {
							PipelineItem* item = m_pipe->Get(itemName);
							props->Open(item);
						} else {
							ObjectManagerItem* item = m_objects->Get(itemName);
							props->Open(item);
						}
					}
				} else if (type == "file" && Settings::Instance().General.ReopenShaders) {
					CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get(ViewID::Code));
					if (!settingItem.attribute("name").empty()) {
						PipelineItem* item = m_pipe->Get(settingItem.attribute("name").as_string());
						const pugi::char_t* shaderType = settingItem.attribute("shader").as_string();

						if (item->Type == PipelineItem::ItemType::ShaderPass) {
							std::string path = ((ed::pipe::ShaderPass*)item->Data)->VSPath;

							if (strcmp(shaderType, "ps") == 0)
								path = ((ed::pipe::ShaderPass*)item->Data)->PSPath;
							else if (strcmp(shaderType, "gs") == 0)
								path = ((ed::pipe::ShaderPass*)item->Data)->GSPath;
							else if (strcmp(shaderType, "tcs") == 0)
								path = ((ed::pipe::ShaderPass*)item->Data)->TCSPath;
							else if (strcmp(shaderType, "tes") == 0)
								path = ((ed::pipe::ShaderPass*)item->Data)->TESPath;

							if (strcmp(shaderType, "vs") == 0 && FileExists(path))
								editor->Open(item, ShaderStage::Vertex);
							else if (strcmp(shaderType, "ps") == 0 && FileExists(path))
								editor->Open(item, ShaderStage::Pixel);
							else if (strcmp(shaderType, "gs") == 0 && FileExists(path))
								editor->Open(item, ShaderStage::Geometry);
							else if (strcmp(shaderType, "tcs") == 0 && FileExists(path))
								editor->Open(item, ShaderStage::TessellationControl);
							else if (strcmp(shaderType, "tes") == 0 && FileExists(path))
								editor->Open(item, ShaderStage::TessellationEvaluation);
						} else if (item->Type == PipelineItem::ItemType::ComputePass) {
							std::string path = ((ed::pipe::ComputePass*)item->Data)->Path;

							if (strcmp(shaderType, "cs") == 0 && FileExists(path))
								editor->Open(item, ShaderStage::Compute);
						} else if (item->Type == PipelineItem::ItemType::AudioPass) {
							std::string path = ((ed::pipe::AudioPass*)item->Data)->Path;
							editor->Open(item, ShaderStage::Pixel);
						}
					}
				} else if (type == "pinned") {
					PinnedUI* pinned = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
					if (!settingItem.attribute("name").empty()) {
						const pugi::char_t* item = settingItem.attribute("name").as_string();
						const pugi::char_t* shaderType = settingItem.attribute("from").as_string();
						PipelineItem* owner = (PipelineItem*)(m_pipe->Get(settingItem.attribute("owner").as_string()));

						if (owner) {
							std::vector<ShaderVariable*> vars;

							if (owner->Type == PipelineItem::ItemType::ShaderPass)
								vars = ((pipe::ShaderPass*)owner->Data)->Variables.GetVariables();
							else if (owner->Type == PipelineItem::ItemType::ComputePass)
								vars = ((pipe::ComputePass*)owner->Data)->Variables.GetVariables();
							else if (owner->Type == PipelineItem::ItemType::AudioPass)
								vars = ((pipe::AudioPass*)owner->Data)->Variables.GetVariables();

							for (const auto& var : vars)
								if (strcmp(var->Name, item) == 0) {
									pinned->Add(var);
									break;
								}
						}
					}
				} else if (type == "camera") {
					if (settingItem.attribute("fp").empty())
						Settings::Instance().Project.FPCamera = false;
					else
						Settings::Instance().Project.FPCamera = settingItem.attribute("fp").as_bool();

					SystemVariableManager::Instance().GetCamera()->Reset();

					bool fp = Settings::Instance().Project.FPCamera;

					if (fp) {
						ed::FirstPersonCamera* fpCam = (ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera();
						fpCam->Reset();
						fpCam->SetPosition(std::stof(settingItem.child("positionX").text().get()),
							std::stof(settingItem.child("positionY").text().get()),
							std::stof(settingItem.child("positionZ").text().get()));
						fpCam->SetYaw(std::stof(settingItem.child("yaw").text().get()));
						fpCam->SetPitch(std::stof(settingItem.child("pitch").text().get()));
					} else {
						ed::ArcBallCamera* ab = (ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera();
						ab->SetDistance(std::stof(settingItem.child("distance").text().get()));
						ab->SetYaw(std::stof(settingItem.child("yaw").text().get()));
						ab->SetPitch(std::stof(settingItem.child("pitch").text().get()));
						ab->SetRoll(std::stof(settingItem.child("roll").text().get()));
					}

				} else if (type == "clearcolor") {
					if (!settingItem.attribute("r").empty())
						Settings::Instance().Project.ClearColor.r = settingItem.attribute("r").as_float();
					if (!settingItem.attribute("g").empty())
						Settings::Instance().Project.ClearColor.g = settingItem.attribute("g").as_float();
					if (!settingItem.attribute("b").empty())
						Settings::Instance().Project.ClearColor.b = settingItem.attribute("b").as_float();
					if (!settingItem.attribute("a").empty())
						Settings::Instance().Project.ClearColor.a = settingItem.attribute("a").as_float();
				} else if (type == "usealpha") {
					if (!settingItem.attribute("val").empty())
						Settings::Instance().Project.UseAlphaChannel = settingItem.attribute("val").as_bool();
					else
						Settings::Instance().Project.UseAlphaChannel = false;
				} else if (type == "ipaths") {
					Settings::Instance().Project.IncludePaths.clear();
					for (pugi::xml_node pathNode : settingItem.children("path"))
						Settings::Instance().Project.IncludePaths.push_back(pathNode.text().as_string());
				} else if (type == "watch_expr") {
					if (!settingItem.attribute("expr").empty())
						m_debug->AddWatch(settingItem.attribute("expr").as_string(), false);
				} else if (type == "vecwatch_expr") {
					if (!settingItem.attribute("expr").empty()) {
						float cr = settingItem.attribute("color_r").as_float();
						float cg = settingItem.attribute("color_g").as_float();
						float cb = settingItem.attribute("color_b").as_float();
						m_debug->AddVectorWatch(settingItem.attribute("expr").as_string(), glm::vec4(cr, cg, cb, 1.0f), false);
					}
				} else if (type == "bkpt") {
					m_debug->AddBreakpoint(settingItem.attribute("file").as_string(),
						settingItem.attribute("line").as_int(),
						settingItem.attribute("use_cond").empty() || settingItem.attribute("use_cond").as_bool(),
						settingItem.attribute("cond").as_string(),
						settingItem.attribute("enabled").as_bool());
				}
			}
		}

		// plugin additional data
		for (pugi::xml_node pluginDataEntry : projectNode.child("plugindata").children("entry")) {
			std::string plName = "";
			if (!pluginDataEntry.attribute("owner").empty())
				plName = pluginDataEntry.attribute("owner").as_string();

			IPlugin1* pl = m_plugins->GetPlugin(plName.c_str());
			
			if (pl) {
				const auto& plName = m_plugins->GetPluginName(pl);
				if (std::count(m_pluginList.begin(), m_pluginList.end(), plName) == 0)
					m_pluginList.push_back(plName);

				std::string innerXML = getInnerXML(pluginDataEntry);
				pl->Project_ImportAdditionalData(innerXML.c_str());
			}
		}

		// set actual render texture IDs
		for (auto& pass : fbos) {
			int index = 0;
			for (auto& rtName : pass.second) {
				ObjectManagerItem* rtOwner = m_objects->Get(rtName);

				GLuint rtID = (rtName.size() == 0) ? m_renderer->GetTexture() : rtOwner->Texture;
				pass.first->RenderTextures[index] = rtID;
				index++;
			}
		}
	}
}