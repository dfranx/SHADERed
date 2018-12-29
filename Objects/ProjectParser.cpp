#include "ProjectParser.h"
#include "PipelineManager.h"
#include "SystemVariableManager.h"
#include "FunctionVariableManager.h"
#include "Names.h"

#include "../UI/PinnedUI.h"
#include "../UI/PropertyUI.h"
#include "../UI/CodeEditorUI.h"

#include <fstream>
#include <direct.h>
#include <Shlwapi.h>
#include <MoonLight/Base/GeometryFactory.h>

namespace ed
{
	ProjectParser::ProjectParser(PipelineManager* pipeline, GUIManager* gui) :
		m_pipe(pipeline), m_file("")
	{
		ResetProjectDirectory();
		m_ui = gui;
	}
	ProjectParser::~ProjectParser()
	{}
	void ProjectParser::Open(const std::string & file)
	{
		m_file = file;
		SetProjectDirectory(file.substr(0, file.find_last_of("/\\")));

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(file.c_str());
		if (!result)
			return;

		m_pipe->Clear();

		// shader passes
		for (pugi::xml_node passNode : doc.child("project").child("pipeline").children("pass")) {
			char name[PIPELINE_ITEM_NAME_LENGTH];
			ed::PipelineItem::ItemType type = ed::PipelineItem::ItemType::ShaderPass;
			ed::pipe::ShaderPass* data = new ed::pipe::ShaderPass();

			// get pass name
			if (!passNode.attribute("name").empty())
				strcpy(name, passNode.attribute("name").as_string());

			// add the item
			m_pipe->AddPass(name, data);

			// get shader properties (NOTE: a shader must have TYPE, PATH and ENTRY 
			for (pugi::xml_node shaderNode : passNode.children("shader")) {
				std::string shaderNodeType(shaderNode.attribute("type").as_string()); // "vs" or "ps"

				// parse path and type
				auto shaderPath = shaderNode.child("path").text().as_string();
				auto shaderEntry = shaderNode.child("entry").text().as_string();
				if (shaderNodeType == "vs") {
					strcpy(data->VSPath, shaderPath);
					strcpy(data->VSEntry, shaderEntry);
				}
				else if (shaderNodeType == "ps") {
					strcpy(data->PSPath, shaderPath);
					strcpy(data->PSEntry, shaderEntry);
				}

				// parse input layout
				if (shaderNodeType == "vs") {
					for (pugi::xml_node entry : shaderNode.child("input").children()) {
						int sID = 0, offset = 0;
						DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

						if (!entry.attribute("id").empty()) sID = entry.attribute("id").as_int();
						if (!entry.attribute("offset").empty()) offset = entry.attribute("offset").as_int();
						if (!entry.attribute("format").empty()) {
							const char* myFormat = entry.attribute("format").as_string();

							for (int i = 0; i < _ARRAYSIZE(FORMAT_NAMES); i++)
								if (strcmp(myFormat, FORMAT_NAMES[i]) == 0) {
									format = (DXGI_FORMAT)i;
									break;
								}
						}

						data->VSInputLayout.Add(entry.text().as_string(), sID, format, offset);
					}
				}

				// parse variables
				for (pugi::xml_node variableNode : shaderNode.child("variables").children("variable")) {
					ShaderVariable::ValueType type = ShaderVariable::ValueType::Float1;
					SystemShaderVariable system = SystemShaderVariable::None;
					FunctionShaderVariable func = FunctionShaderVariable::None;
					int slot = 0;

					if (!variableNode.attribute("slot").empty()) slot = variableNode.attribute("slot").as_int();
					if (!variableNode.attribute("type").empty()) {
						const char* myType = variableNode.attribute("type").as_string();
						for (int i = 0; i < _ARRAYSIZE(VARIABLE_TYPE_NAMES); i++)
							if (strcmp(myType, VARIABLE_TYPE_NAMES[i]) == 0) {
								type = (ed::ShaderVariable::ValueType)i;
								break;
							}
					}
					if (!variableNode.attribute("system").empty()) {
						const char* mySystem = variableNode.attribute("system").as_string();
						for (int i = 0; i < _ARRAYSIZE(SYSTEM_VARIABLE_NAMES); i++)
							if (strcmp(mySystem, SYSTEM_VARIABLE_NAMES[i]) == 0) {
								system = (ed::SystemShaderVariable)i;
								break;
							}
						if (SystemVariableManager::GetType(system) != type)
							system = ed::SystemShaderVariable::None;
					}
					if (!variableNode.attribute("function").empty()) {
						const char* myFunc = variableNode.attribute("function").as_string();
						for (int i = 0; i < _ARRAYSIZE(FUNCTION_NAMES); i++)
							if (strcmp(myFunc, FUNCTION_NAMES[i]) == 0) {
								func = (FunctionShaderVariable)i;
								break;
							}
						if (system != SystemShaderVariable::None || !FunctionVariableManager::HasValidReturnType(type, func))
							func = FunctionShaderVariable::None;
					}

					ShaderVariable* var = new ShaderVariable(type, variableNode.attribute("name").as_string(), system, slot);
					FunctionVariableManager::AllocateArgumentSpace(var, func);

					// parse value
					if (system == SystemShaderVariable::None) {
						int rowID = 0;
						for (pugi::xml_node row : variableNode.children("row")) {
							int colID = 0;
							for (pugi::xml_node value : row.children("value")) {
								if (func != FunctionShaderVariable::None)
									*FunctionVariableManager::LoadFloat(var->Arguments, colID++) = value.text().as_float();
								else {
									if (type >= ShaderVariable::ValueType::Boolean1 && type <= ShaderVariable::ValueType::Boolean4)
										var->SetBooleanValue(value.text().as_bool(), colID++);
									else if (type >= ShaderVariable::ValueType::Integer1 && type <= ShaderVariable::ValueType::Integer4)
										var->SetIntegerValue(value.text().as_int(), colID++);
									else
										var->SetFloat(value.text().as_float(), colID++, rowID);
								}

							}
							rowID++;
						}
					}

					if (shaderNodeType == "vs")
						data->VSVariables.Add(var);
					else
						data->PSVariables.Add(var);
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
							for (int k = 0; k < _ARRAYSIZE(TOPOLOGY_ITEM_NAMES); k++)
								if (strcmp(attrNode.text().as_string(), TOPOLOGY_ITEM_NAMES[k]) == 0)
									tData->Topology = (ml::Topology::Type)k;
						}
						else if (strcmp(attrNode.name(), "type") == 0) {
							for (int k = 0; k < _ARRAYSIZE(GEOMETRY_NAMES); k++)
								if (strcmp(attrNode.text().as_string(), GEOMETRY_NAMES[k]) == 0)
									tData->Type = (pipe::GeometryItem::GeometryType)k;
						}
					}
				}
				else if (strcmp(itemNode.attribute("type").as_string(), "blend") == 0) {
					itemType = ed::PipelineItem::ItemType::BlendState;
					itemData = new pipe::BlendState;

					pipe::BlendState* tData = (pipe::BlendState*)itemData;
					D3D11_RENDER_TARGET_BLEND_DESC* bDesc = &tData->State.Info.RenderTarget[0];
					ml::Color blendFactor(0, 0, 0, 0);

					bDesc->BlendEnable = true;
					tData->State.Info.IndependentBlendEnable = false;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "srcblend") == 0)
							bDesc->SrcBlend = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "blendop") == 0)
							bDesc->BlendOp = m_toBlendOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "destblend") == 0)
							bDesc->DestBlend = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "srcblendalpha") == 0)
							bDesc->SrcBlendAlpha = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "alphablendop") == 0)
							bDesc->BlendOpAlpha = m_toBlendOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "destblendalpha") == 0)
							bDesc->DestBlendAlpha = m_toBlend(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "alpha2cov") == 0)
							tData->State.Info.AlphaToCoverageEnable = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "bf_red") == 0)
							blendFactor.R = attrNode.text().as_uint();
						else if (strcmp(attrNode.name(), "bf_green") == 0)
							blendFactor.G = attrNode.text().as_uint();
						else if (strcmp(attrNode.name(), "bf_blue") == 0)
							blendFactor.B = attrNode.text().as_uint();
						else if (strcmp(attrNode.name(), "bf_alpha") == 0)
							blendFactor.A = attrNode.text().as_uint();
					}

					tData->State.SetBlendFactor(blendFactor);
				}

				// create and modify if needed
				if (itemType == ed::PipelineItem::ItemType::Geometry) {
					ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(itemData);
					if (tData->Type == pipe::GeometryItem::Cube)
						tData->Geometry = ml::GeometryFactory::CreateCube(tData->Size.x, tData->Size.y, tData->Size.z, *m_pipe->GetOwner());
				}
				else if (itemType == ed::PipelineItem::ItemType::BlendState) {
					ed::pipe::BlendState* tData = reinterpret_cast<ed::pipe::BlendState*>(itemData);
					tData->State.Create(*m_pipe->GetOwner());
				}

				m_pipe->AddItem(name, itemName, itemType, itemData);
			}
		}

		// settings
		for (pugi::xml_node settingItem : doc.child("project").child("settings").children("entry")) {
			if (!settingItem.attribute("type").empty()) {
				std::string type = settingItem.attribute("type").as_string();
				if (type == "property") {
					PropertyUI* props = ((PropertyUI*)m_ui->Get("Properties"));
					if (!settingItem.attribute("name").empty()) {
						auto item = m_pipe->Get(settingItem.attribute("name").as_string());
						props->Open(item);
					}
				}
				else if (type == "file") {
					CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get("Code"));
					if (!settingItem.attribute("name").empty()) {
						auto item = m_pipe->Get(settingItem.attribute("name").as_string());
						auto shaderType = settingItem.attribute("shader").as_string();

						if (strcmp(shaderType, "vs") == 0)
							editor->OpenVS(*item);
						else
							editor->OpenPS(*item);
					}
				}
				else if (type == "pinned") {
					PinnedUI* pinned = ((PinnedUI*)m_ui->Get("Pinned"));
					if (!settingItem.attribute("name").empty()) {
						auto item = settingItem.attribute("name").as_string();
						auto shaderType = settingItem.attribute("from").as_string();
						auto owner = (pipe::ShaderPass*)m_pipe->Get(settingItem.attribute("owner").as_string())->Data;

						if (strcmp(shaderType, "vs") == 0) {
							auto vars = owner->VSVariables.GetVariables();
							for (auto var : vars)
								if (strcmp(var->Name, item) == 0) {
									pinned->Add(var);
									break;
								}
						} else {
							auto vars = owner->PSVariables.GetVariables();
							for (auto var : vars)
								if (strcmp(var->Name, item) == 0) {
									pinned->Add(var);
									break;
								}
						}
					}
				}
				else if (type == "camera") {
					SystemVariableManager::Instance().GetCamera().Reset();
					SystemVariableManager::Instance().GetCamera().SetDistance(std::stof(settingItem.child("distance").text().get()));
					SystemVariableManager::Instance().GetCamera().RotateX(std::stof(settingItem.child("rotationX").text().get()));
					SystemVariableManager::Instance().GetCamera().RotateY(std::stof(settingItem.child("rotationY").text().get()));
					SystemVariableManager::Instance().GetCamera().RotateZ(std::stof(settingItem.child("rotationZ").text().get()));
				}
			}
		}
	}
	void ProjectParser::OpenTemplate()
	{
		char appPath[FILENAME_MAX];

		GetCurrentDirectoryA(sizeof(appPath), appPath);


		Open(std::string(appPath) + "\\template\\template.sprj");
		m_file = ""; // disallow overwriting template.sprj project file
	}
	void ProjectParser::Save()
	{
		SaveAs(m_file);
	}
	void ProjectParser::SaveAs(const std::string & file)
	{
		m_file = file;
		std::string oldProjectPath = m_projectPath;
		SetProjectDirectory(file.substr(0, file.find_last_of("/\\")));

		pugi::xml_document doc;
		pugi::xml_node projectNode = doc.append_child("project");
		pugi::xml_node pipelineNode = projectNode.append_child("pipeline");
		pugi::xml_node settingsNode = projectNode.append_child("settings");

		// shader passes
		std::vector<PipelineItem*> passItems = m_pipe->GetList();
		for (PipelineItem* passItem : passItems) {
			pipe::ShaderPass* passData = (pipe::ShaderPass*)passItem->Data;

			pugi::xml_node passNode = pipelineNode.append_child("pass");
			passNode.append_attribute("name").set_value(passItem->Name);

			// vertex shader
			pugi::xml_node vsNode = passNode.append_child("shader");
			std::string relativePath = GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '\\') ? "" : "\\") + std::string(passData->VSPath));

			vsNode.append_attribute("type").set_value("vs");
			vsNode.append_child("path").text().set(relativePath.c_str());
			vsNode.append_child("entry").text().set(passData->VSEntry);


			pugi::xml_node layoutNode = vsNode.append_child("input");
			std::vector<D3D11_INPUT_ELEMENT_DESC> layoutItems = passData->VSInputLayout.GetInputElements();
			for (D3D11_INPUT_ELEMENT_DESC layoutItem : layoutItems) { // input layout
				pugi::xml_node layoutItemNode = layoutNode.append_child("element");

				layoutItemNode.text().set(layoutItem.SemanticName);
				layoutItemNode.append_attribute("id").set_value(layoutItem.SemanticIndex);
				layoutItemNode.append_attribute("format").set_value(FORMAT_NAMES[(int)layoutItem.Format]);
				layoutItemNode.append_attribute("offset").set_value(layoutItem.AlignedByteOffset);
			}

			m_exportShaderVariables(vsNode, passData->VSVariables.GetVariables());

			// pixel shader
			pugi::xml_node psNode = passNode.append_child("shader");
			relativePath = GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '\\') ? "" : "\\") + std::string(passData->PSPath));

			psNode.append_attribute("type").set_value("ps");
			psNode.append_child("path").text().set(relativePath.c_str());
			psNode.append_child("entry").text().set(passData->PSEntry);
			m_exportShaderVariables(psNode, passData->PSVariables.GetVariables());


			// pass items
			pugi::xml_node itemsNode = passNode.append_child("items");
			for (PipelineItem* item : passData->Items) {
				pugi::xml_node itemNode = itemsNode.append_child("item");
				itemNode.append_attribute("name").set_value(item->Name);

				if (item->Type == PipelineItem::ItemType::Geometry) {
					itemNode.append_attribute("type").set_value("geometry");

					ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(item->Data);

					itemNode.append_child("type").text().set(GEOMETRY_NAMES[tData->Type]);
					itemNode.append_child("width").text().set(tData->Size.x);
					itemNode.append_child("height").text().set(tData->Size.y);
					itemNode.append_child("depth").text().set(tData->Size.z);
					if (tData->Scale.x != 1.0f) itemNode.append_child("scaleX").text().set(tData->Scale.x);
					if (tData->Scale.y != 1.0f) itemNode.append_child("scaleY").text().set(tData->Scale.y);
					if (tData->Scale.z != 1.0f) itemNode.append_child("scaleZ").text().set(tData->Scale.z);
					if (tData->Rotation.z != 0.0f) itemNode.append_child("roll").text().set(tData->Rotation.z);
					if (tData->Rotation.x != 0.0f) itemNode.append_child("pitch").text().set(tData->Rotation.x);
					if (tData->Rotation.y != 0.0f) itemNode.append_child("yaw").text().set(tData->Rotation.y);
					if (tData->Position.x != 0.0f) itemNode.append_child("x").text().set(tData->Position.z);
					if (tData->Position.y != 0.0f) itemNode.append_child("y").text().set(tData->Position.x);
					if (tData->Position.z != 0.0f) itemNode.append_child("z").text().set(tData->Position.y);
					itemNode.append_child("topology").text().set(TOPOLOGY_ITEM_NAMES[(int)tData->Topology]);
				}
				else if (item->Type == PipelineItem::ItemType::BlendState) {
					itemNode.append_attribute("type").set_value("blend");

					ed::pipe::BlendState* tData = reinterpret_cast<ed::pipe::BlendState*>(item->Data);
					D3D11_RENDER_TARGET_BLEND_DESC* bDesc = &tData->State.Info.RenderTarget[0];
					ml::Color blendFactor = tData->State.GetBlendFactor();

					itemNode.append_child("srcblend").text().set(BLEND_NAME[bDesc->SrcBlend]);
					itemNode.append_child("blendop").text().set(BLEND_OPERATOR[bDesc->BlendOp]);
					itemNode.append_child("destblend").text().set(BLEND_NAME[bDesc->DestBlend]);
					itemNode.append_child("srcblendalpha").text().set(BLEND_NAME[bDesc->SrcBlendAlpha]);
					itemNode.append_child("alphablendop").text().set(BLEND_OPERATOR[bDesc->BlendOpAlpha]);
					itemNode.append_child("destblendalpha").text().set(BLEND_NAME[bDesc->DestBlendAlpha]);
					itemNode.append_child("alpha2cov").text().set(tData->State.Info.AlphaToCoverageEnable);
					if (blendFactor.R != 0) itemNode.append_child("bf_red").text().set(blendFactor.R);
					if (blendFactor.G != 0) itemNode.append_child("bf_green").text().set(blendFactor.G);
					if (blendFactor.B != 0) itemNode.append_child("bf_blue").text().set(blendFactor.B);
					if (blendFactor.A != 0) itemNode.append_child("bf_alpha").text().set(blendFactor.A);
				}
			}
		}

		// settings
		{
			// property ui
			PropertyUI* props = ((PropertyUI*)m_ui->Get("Properties"));
			if (props->HasItemSelected()) {
				std::string name = props->CurrentItemName();

				pugi::xml_node propNode = settingsNode.append_child("entry");
				propNode.append_attribute("type").set_value("property");
				propNode.append_attribute("name").set_value(name.c_str());
			}

			// code editor ui
			CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get("Code"));
			std::vector<std::pair<std::string, bool>> files = editor->GetOpenedFiles();
			for (const auto& file : files) {
				pugi::xml_node fileNode = settingsNode.append_child("entry");
				fileNode.append_attribute("type").set_value("file");
				fileNode.append_attribute("name").set_value(file.first.c_str());
				fileNode.append_attribute("shader").set_value(file.second ? "vs" : "ps");
			}

			// pinned ui
			PinnedUI* pinned = ((PinnedUI*)m_ui->Get("Pinned"));
			auto pinnedVars = pinned->GetAll();
			for (auto var : pinnedVars) {
				pugi::xml_node varNode = settingsNode.append_child("entry");
				varNode.append_attribute("type").set_value("pinned");
				varNode.append_attribute("name").set_value(var->Name);

				for (PipelineItem* passItem : passItems) {
					auto data = (pipe::ShaderPass*)passItem->Data;
					bool found = false;

					auto vsVars = data->VSVariables.GetVariables();
					for (int i = 0; i < vsVars.size(); i++) {
						if (vsVars[i] == var) {
							varNode.append_attribute("from").set_value("vs");
							varNode.append_attribute("owner").set_value(passItem->Name);
							found = true;
						}
					}

					if (!found) {
						auto psVars = data->PSVariables.GetVariables();
						for (int i = 0; i < psVars.size(); i++) {
							if (psVars[i] == var) {
								varNode.append_attribute("from").set_value("ps");
								varNode.append_attribute("owner").set_value(passItem->Name);
								found = true;
							}
						}
					} 
					
					if (found) break;
				}
			}

			// camera settings
			{
				ed::Camera cam = SystemVariableManager::Instance().GetCamera();
				pugi::xml_node camNode = settingsNode.append_child("entry");
				camNode.append_attribute("type").set_value("camera");

				DirectX::XMFLOAT3 rota = cam.GetRotation();

				camNode.append_child("distance").text().set(cam.GetDistance());
				camNode.append_child("rotationX").text().set(rota.x);
				camNode.append_child("rotationY").text().set(rota.y);
				camNode.append_child("rotationZ").text().set(rota.z);
			}
		}

		doc.save_file(file.c_str());
	}
	std::string ProjectParser::LoadProjectFile(const std::string & file)
	{
		std::ifstream in(m_projectPath + ((m_projectPath[m_projectPath.size() - 1] == '\\') ? "" : "\\") + file);
		if (in.is_open()) {
			std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
			in.close();
			return content;
		}
		return "";
	}
	void ProjectParser::SaveProjectFile(const std::string & file, const std::string & data)
	{
		std::ofstream out(m_projectPath + ((m_projectPath[m_projectPath.size() - 1] == '\\') ? "" : "\\") + file);
		out << data;
		out.close();
	}
	std::string ProjectParser::GetRelativePath(const std::string& to)
	{
		char relativePath[MAX_PATH] = { 0 };
		PathRelativePathToA(relativePath,
			m_projectPath.c_str(),
			FILE_ATTRIBUTE_DIRECTORY,
			to.c_str(),
			FILE_ATTRIBUTE_NORMAL);
		return std::string(relativePath);
	}
	void ProjectParser::ResetProjectDirectory()
	{
		m_file = "";
		char currentPath[FILENAME_MAX] = { 0 };
		_getcwd(currentPath, FILENAME_MAX);
		m_projectPath = currentPath;
	}

	void ProjectParser::m_exportShaderVariables(pugi::xml_node& node, std::vector<ShaderVariable*>& vars)
	{
		if (vars.size() > 0) {
			pugi::xml_node varsNodes = node.append_child("variables");
			for (auto var : vars) {
				pugi::xml_node varNode = varsNodes.append_child("variable");
				varNode.append_attribute("type").set_value(VARIABLE_TYPE_NAMES[(int)var->GetType()]);
				varNode.append_attribute("name").set_value(var->Name);

				if (var->System != SystemShaderVariable::None)
					varNode.append_attribute("system").set_value(SYSTEM_VARIABLE_NAMES[(int)var->System]);
				else if (var->Function != FunctionShaderVariable::None)
					varNode.append_attribute("function").set_value(FUNCTION_NAMES[(int)var->Function]);

				varNode.append_attribute("slot").set_value(var->Slot);

				if (var->System == SystemShaderVariable::None) {
					pugi::xml_node valueRowNode = varNode.append_child("row");

					if (var->Function == FunctionShaderVariable::None) {
						int rowID = 0;
						for (int i = 0; i < ShaderVariable::GetSize(var->GetType()) / 4; i++) {
							if (var->GetType() >= ShaderVariable::ValueType::Boolean1 && var->GetType() <= ShaderVariable::ValueType::Boolean4)
								valueRowNode.append_child("value").text().set(var->AsBoolean(i));
							else if (var->GetType() >= ShaderVariable::ValueType::Integer1 && var->GetType() <= ShaderVariable::ValueType::Integer4)
								valueRowNode.append_child("value").text().set(var->AsInteger(i));
							else
								valueRowNode.append_child("value").text().set(var->AsFloat(i%var->GetColumnCount(), rowID));

							if (i%var->GetColumnCount() == 0 && i != 0) {
								valueRowNode = varNode.append_child("row");
								rowID++;
							}
						}
					}
					else {
						// save arguments
						for (int i = 0; i < FunctionVariableManager::GetArgumentCount(var->Function); i++) {
							valueRowNode.append_child("value").text().set(*FunctionVariableManager::LoadFloat(var->Arguments, i));
						}
					}
				}
			}
		}
	}
	D3D11_BLEND ProjectParser::m_toBlend(const char* text)
	{
		for (int k = 0; k < _ARRAYSIZE(BLEND_NAME); k++)
			if (strcmp(text, BLEND_NAME[k]) == 0)
				return (D3D11_BLEND)k;
		return D3D11_BLEND_BLEND_FACTOR;
	}
	D3D11_BLEND_OP ProjectParser::m_toBlendOp(const char* text)
	{
		for (int k = 0; k < _ARRAYSIZE(BLEND_OPERATOR); k++)
			if (strcmp(text, BLEND_OPERATOR[k]) == 0)
				return (D3D11_BLEND_OP)k;
		return D3D11_BLEND_OP_ADD;
	}
}