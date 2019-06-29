#include "ProjectParser.h"
#include "RenderEngine.h"
#include "ObjectManager.h"
#include "PipelineManager.h"
#include "SystemVariableManager.h"
#include "FunctionVariableManager.h"
#include "GLSL2HLSL.h"
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
	ProjectParser::ProjectParser(PipelineManager* pipeline, ObjectManager* objects, RenderEngine* rend, MessageStack* msgs, GUIManager* gui) :
		m_pipe(pipeline), m_file(""), m_renderer(rend), m_objects(objects), m_msgs(msgs)
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

		m_msgs->Clear();
		for (auto& mdl : m_models)
			if (mdl.second != nullptr)
				delete mdl.second;
		m_models.clear();

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(file.c_str());
		if (!result)
			return;

		m_pipe->Clear();
		m_objects->Clear();

		strcpy(Settings::Instance().Project.GLSLVS_Extenstion, "vert");
		strcpy(Settings::Instance().Project.GLSLPS_Extenstion, "frag");
		strcpy(Settings::Instance().Project.GLSLGS_Extenstion, "geom");
		Settings::Instance().Project.FPCamera = false;
		Settings::Instance().Project.ClearColor = ml::Color(0, 0, 0, 0);

		// shader passes
		for (pugi::xml_node passNode : doc.child("project").child("pipeline").children("pass")) {
			char name[PIPELINE_ITEM_NAME_LENGTH];
			ed::PipelineItem::ItemType type = ed::PipelineItem::ItemType::ShaderPass;
			ed::pipe::ShaderPass* data = new ed::pipe::ShaderPass();

			strcpy(data->RenderTexture[0], "Window");
			for (int i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
				data->RenderTexture[i][0] = 0;

			// get pass name
			if (!passNode.attribute("name").empty())
				strcpy(name, passNode.attribute("name").as_string());

			// get render textures
			int rtCur = 0;
			for (pugi::xml_node rtNode : passNode.children("rendertexture")) {
				std::string rtName(rtNode.attribute("name").as_string());

				strcpy(data->RenderTexture[rtCur], rtName.c_str());

				rtCur++;
			}

			// add the item
			m_pipe->AddPass(name, data);

			// get shader properties (NOTE: a shader must have TYPE, PATH and ENTRY 
			for (pugi::xml_node shaderNode : passNode.children("shader")) {
				std::string shaderNodeType(shaderNode.attribute("type").as_string()); // "vs" or "ps" or "gs"

				// parse path and type
				const pugi::char_t* shaderPath = shaderNode.child("path").text().as_string();
				const pugi::char_t* shaderEntry = shaderNode.child("entry").text().as_string();
				if (shaderNodeType == "vs") {
					strcpy(data->VSPath, shaderPath);
					strcpy(data->VSEntry, shaderEntry);
				} else if (shaderNodeType == "ps") {
					strcpy(data->PSPath, shaderPath);
					strcpy(data->PSEntry, shaderEntry);
				} else if (shaderNodeType == "gs") {
					if (!shaderNode.attribute("used").empty()) data->GSUsed = shaderNode.attribute("used").as_bool();
					else data->GSUsed = false;
					strcpy(data->GSPath, shaderPath);
					strcpy(data->GSEntry, shaderEntry);
				}

				std::string type = ((shaderNodeType == "vs") ? "vertex" : ((shaderNodeType == "ps") ? "pixel" : "geometry"));
				if (!FileExists(shaderPath))
					m_msgs->Add(ed::MessageStack::Type::Error, name, "Failed to load " + type + " shader.");

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
					if (system == SystemShaderVariable::None)
						m_parseVariableValue(variableNode, var);

					if (shaderNodeType == "vs")
						data->VSVariables.Add(var);
					else if (shaderNodeType == "ps")
						data->PSVariables.Add(var);
					else if (shaderNodeType == "gs")
						data->GSVariables.Add(var);
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

					tData->Scale = DirectX::XMFLOAT3(1, 1, 1);
					tData->Position = DirectX::XMFLOAT3(0, 0, 0);
					tData->Rotation = DirectX::XMFLOAT3(0, 0, 0);

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
				else if (strcmp(itemNode.attribute("type").as_string(), "depthstencil") == 0) {
					itemType = ed::PipelineItem::ItemType::DepthStencilState;
					itemData = new pipe::DepthStencilState;

					pipe::DepthStencilState* tData = (pipe::DepthStencilState*)itemData;
					D3D11_DEPTH_STENCIL_DESC* dDesc = &tData->State.Info;

					tData->StencilReference = 0;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "depthenable") == 0)
							dDesc->DepthEnable = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "depthfunc") == 0)
							dDesc->DepthFunc = m_toComparisonFunc(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "stencilenable") == 0)
							dDesc->StencilEnable = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "frontfunc") == 0)
							dDesc->FrontFace.StencilFunc = m_toComparisonFunc(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "frontpass") == 0)
							dDesc->FrontFace.StencilPassOp = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "frontfail") == 0)
							dDesc->FrontFace.StencilFailOp = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "backfunc") == 0)
							dDesc->BackFace.StencilFunc = m_toComparisonFunc(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "backpass") == 0)
							dDesc->BackFace.StencilPassOp = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "backfail") == 0)
							dDesc->BackFace.StencilFailOp = m_toStencilOp(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "sref") == 0)
							tData->StencilReference = attrNode.text().as_uint();
					}
				}
				else if (strcmp(itemNode.attribute("type").as_string(), "rasterizer") == 0) {
					itemType = ed::PipelineItem::ItemType::RasterizerState;
					itemData = new pipe::RasterizerState;

					pipe::RasterizerState* tData = (pipe::RasterizerState*)itemData;
					D3D11_RASTERIZER_DESC* rDesc = &tData->State.Info;

					for (pugi::xml_node attrNode : itemNode.children()) {
						if (strcmp(attrNode.name(), "wireframe") == 0)
							rDesc->FillMode = attrNode.text().as_bool() ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
						else if (strcmp(attrNode.name(), "cull") == 0)
							rDesc->CullMode = m_toCullMode(attrNode.text().as_string());
						else if (strcmp(attrNode.name(), "ccw") == 0)
							rDesc->FrontCounterClockwise = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "depthbias") == 0)
							rDesc->DepthBias = attrNode.text().as_int();
						else if (strcmp(attrNode.name(), "depthbiasclamp") == 0)
							rDesc->DepthBias = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "slopebias") == 0)
							rDesc->SlopeScaledDepthBias = attrNode.text().as_float();
						else if (strcmp(attrNode.name(), "depthclip") == 0)
							rDesc->DepthClipEnable = attrNode.text().as_bool();
						else if (strcmp(attrNode.name(), "aa") == 0)
							rDesc->AntialiasedLineEnable = attrNode.text().as_bool();
					}
				}
				else if (strcmp(itemNode.attribute("type").as_string(), "model") == 0) {
					itemType = ed::PipelineItem::ItemType::OBJModel;
					itemData = new pipe::OBJModel;

					pipe::OBJModel* mdata = (pipe::OBJModel*)itemData;

					mdata->OnlyGroup = false;
					mdata->Scale = DirectX::XMFLOAT3(1, 1, 1);
					mdata->Position = DirectX::XMFLOAT3(0, 0, 0);
					mdata->Rotation = DirectX::XMFLOAT3(0, 0, 0);

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
				}

				// create and modify if needed
				if (itemType == ed::PipelineItem::ItemType::Geometry) {
					ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(itemData);
					if (tData->Type == pipe::GeometryItem::Cube)
						tData->Geometry = ml::GeometryFactory::CreateCube(tData->Size.x, tData->Size.y, tData->Size.z, *m_pipe->GetOwner());
					else if (tData->Type == pipe::GeometryItem::Circle)
						tData->Geometry = ml::GeometryFactory::CreateCircle(0, 0, tData->Size.x, tData->Size.y, *m_pipe->GetOwner());
					else if (tData->Type == pipe::GeometryItem::Plane)
						tData->Geometry = ml::GeometryFactory::CreatePlane(tData->Size.x, tData->Size.y, *m_pipe->GetOwner());
					else if (tData->Type == pipe::GeometryItem::Rectangle)
						tData->Geometry = ml::GeometryFactory::CreatePlane(1, 1, *m_pipe->GetOwner());
					else if (tData->Type == pipe::GeometryItem::Sphere)
						tData->Geometry = ml::GeometryFactory::CreateSphere(tData->Size.x, *m_pipe->GetOwner());
					else if (tData->Type == pipe::GeometryItem::Triangle)
						tData->Geometry = ml::GeometryFactory::CreateTriangle(0, 0, tData->Size.x, *m_pipe->GetOwner());
				}
				else if (itemType == ed::PipelineItem::ItemType::BlendState) {
					ed::pipe::BlendState* tData = reinterpret_cast<ed::pipe::BlendState*>(itemData);
					tData->State.Create(*m_pipe->GetOwner());
				}
				else if (itemType == ed::PipelineItem::ItemType::DepthStencilState) {
					ed::pipe::DepthStencilState* tData = reinterpret_cast<ed::pipe::DepthStencilState*>(itemData);
					tData->State.Create(*m_pipe->GetOwner());
				}
				else if (itemType == ed::PipelineItem::ItemType::RasterizerState) {
					ed::pipe::RasterizerState* tData = reinterpret_cast<ed::pipe::RasterizerState*>(itemData);
					tData->State.Create(*m_pipe->GetOwner());
				}
				else if (itemType == ed::PipelineItem::ItemType::OBJModel) {
					ed::pipe::OBJModel* tData = reinterpret_cast<ed::pipe::OBJModel*>(itemData);

					//std::string objMem = LoadProjectFile(tData->Filename);
					ml::OBJModel* ptrObject = LoadModel(tData->Filename);
					bool loaded = ptrObject != nullptr;

					if (loaded)
						tData->Mesh = *ptrObject;// tData->Mesh.LoadFromMemory(objMem.c_str(), objMem.size());
					else m_msgs->Add(ed::MessageStack::Type::Error, name, "Failed to load .obj model " + std::string(itemName));

					if (loaded) {
						ml::OBJModel::Vertex* verts = tData->Mesh.GetVertexData();
						ml::UInt32 vertCount = tData->Mesh.GetVertexCount();

						if (tData->OnlyGroup) {
							verts = tData->Mesh.GetGroupVertices(tData->GroupName);
							vertCount = tData->Mesh.GetGroupVertexCount(tData->GroupName);

							if (verts == nullptr) {
								verts = tData->Mesh.GetObjectVertices(tData->GroupName);
								vertCount = tData->Mesh.GetObjectVertexCount(tData->GroupName);

								// TODO: if (verts == nullptr) error "failed to find a group with that name"
							}
						}

						tData->VertCount = vertCount;
						tData->Vertices.Create(*m_pipe->GetOwner(), verts, vertCount, ml::Resource::Immutable);
					}
				}

				m_pipe->AddItem(name, itemName, itemType, itemData);
			}

			// parse item values
			for (pugi::xml_node itemValueNode : passNode.child("itemvalues").children("value")) {
				std::string type = itemValueNode.attribute("from").as_string();
				const pugi::char_t* valname = itemValueNode.attribute("variable").as_string();
				const pugi::char_t* itemname = itemValueNode.attribute("for").as_string();

				std::vector<ShaderVariable*> vars = data->VSVariables.GetVariables();
				if (type == "ps") vars = data->PSVariables.GetVariables();
				else if (type == "gs") vars = data->GSVariables.GetVariables();

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
		for (pugi::xml_node objectNode : doc.child("project").child("objects").children("object")) {
			const pugi::char_t* objType = objectNode.attribute("type").as_string();

			if (strcmp(objType, "texture") == 0) {
				const pugi::char_t* objPath = objectNode.attribute("path").as_string();
				bool isCube = false;
				if (!objectNode.attribute("cube").empty())
					isCube = objectNode.attribute("cube").as_bool();

				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (auto pass : passes) {
						if (strcmp(pass->Name, passBindName) == 0) {
							if (boundTextures[pass].size() <= slot)
								boundTextures[pass].resize(slot+1);

							boundTextures[pass][slot] = objPath;
							m_objects->CreateTexture(objPath, isCube);
							break;
						}
					}
				}
			} else if (strcmp(objType, "rendertexture") == 0) {
				const pugi::char_t* objName = objectNode.attribute("name").as_string();

				m_objects->CreateRenderTexture(objName);
				ed::RenderTextureObject* rt = m_objects->GetRenderTexture(objName);

				// load size
				if (objectNode.attribute("fsize").empty()) { // load RatioSize if attribute fsize (FixedSize) doesnt exist
					std::string rtSize = objectNode.attribute("rsize").as_string();
					float rtSizeX = std::stof(rtSize.substr(0, rtSize.find(',')));
					float rtSizeY = std::stof(rtSize.substr(rtSize.find(',') + 1));

					rt->RatioSize = DirectX::XMFLOAT2(rtSizeX, rtSizeY);
					rt->FixedSize = DirectX::XMINT2(-1, -1);

					m_objects->ResizeRenderTexture(objName, rt->CalculateSize(m_renderer->GetLastRenderSize().x, m_renderer->GetLastRenderSize().y));
				} else {
					std::string rtSize = objectNode.attribute("fsize").as_string();
					int rtSizeX = std::stoi(rtSize.substr(0, rtSize.find(',')));
					int rtSizeY = std::stoi(rtSize.substr(rtSize.find(',') + 1));

					rt->FixedSize = DirectX::XMINT2(rtSizeX, rtSizeY);

					m_objects->ResizeRenderTexture(objName, rt->FixedSize);
				}

				// load clear color
				if (!objectNode.attribute("r").empty()) rt->ClearColor.R = objectNode.attribute("r").as_int();
				else rt->ClearColor.R = 0;
				if (!objectNode.attribute("g").empty()) rt->ClearColor.G = objectNode.attribute("g").as_int();
				else rt->ClearColor.G = 0;
				if (!objectNode.attribute("b").empty()) rt->ClearColor.B = objectNode.attribute("b").as_int();
				else rt->ClearColor.B = 0;
				if (!objectNode.attribute("a").empty()) rt->ClearColor.A = objectNode.attribute("a").as_int();
				else rt->ClearColor.A = 0;

				// load binds
				for (pugi::xml_node bindNode : objectNode.children("bind")) {
					const pugi::char_t* passBindName = bindNode.attribute("name").as_string();
					int slot = bindNode.attribute("slot").as_int();

					for (auto pass : passes) {
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
		
		// bind objects
		for (auto b : boundTextures)
			for (auto id : b.second)
				if (!id.empty()) m_objects->Bind(id, b.first);

		// settings
		for (pugi::xml_node settingItem : doc.child("project").child("settings").children("entry")) {
			if (!settingItem.attribute("type").empty()) {
				std::string type = settingItem.attribute("type").as_string();
				if (type == "property") {
					PropertyUI* props = ((PropertyUI*)m_ui->Get(ViewID::Properties));
					if (!settingItem.attribute("name").empty()) {
						PipelineItem* item = m_pipe->Get(settingItem.attribute("name").as_string());
						props->Open(item);
					}
				}
				else if (type == "file" && Settings::Instance().General.ReopenShaders) {
					CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get(ViewID::Code));
					if (!settingItem.attribute("name").empty()) {
						PipelineItem* item = m_pipe->Get(settingItem.attribute("name").as_string());
						const pugi::char_t* shaderType = settingItem.attribute("shader").as_string();

						std::string type = ((strcmp(shaderType, "vs") == 0) ? "vertex" : ((strcmp(shaderType, "ps") == 0) ? "pixel" : "geometry"));
						std::string path = ((ed::pipe::ShaderPass*)item->Data)->VSPath;
						
						if (strcmp(shaderType, "ps") == 0)
							path = ((ed::pipe::ShaderPass*)item->Data)->PSPath;
						else if (strcmp(shaderType, "gs") == 0)
							path = ((ed::pipe::ShaderPass*)item->Data)->GSPath;

						if (strcmp(shaderType, "vs") == 0 && FileExists(path))
							editor->OpenVS(*item);
						else if (strcmp(shaderType, "ps") == 0 && FileExists(path))
							editor->OpenPS(*item);
						else if (strcmp(shaderType, "gs") == 0 && FileExists(path))
							editor->OpenGS(*item);
					}
				}
				else if (type == "pinned") {
					PinnedUI* pinned = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
					if (!settingItem.attribute("name").empty()) {
						const pugi::char_t* item = settingItem.attribute("name").as_string();
						const pugi::char_t* shaderType = settingItem.attribute("from").as_string();
						pipe::ShaderPass* owner = (pipe::ShaderPass*)m_pipe->Get(settingItem.attribute("owner").as_string())->Data;

						std::vector<ShaderVariable*> vars = owner->VSVariables.GetVariables();
						if (strcmp(shaderType, "ps") == 0)
							vars = owner->PSVariables.GetVariables();
						else if (strcmp(shaderType, "gs") == 0)
							vars = owner->GSVariables.GetVariables();

						for (auto var : vars)
							if (strcmp(var->Name, item) == 0) {
								pinned->Add(var);
								break;
							}
					}
				}
				else if (type == "camera") {
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
									   std::stof(settingItem.child("positionZ").text().get())
						);
						fpCam->SetYaw(std::stof(settingItem.child("yaw").text().get()));
						fpCam->SetPitch(std::stof(settingItem.child("pitch").text().get()));
					} else {
						ed::ArcBallCamera* ab = (ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera();
						ab->SetDistance(std::stof(settingItem.child("distance").text().get()));
						ab->RotateX(std::stof(settingItem.child("rotationX").text().get()));
						ab->RotateY(std::stof(settingItem.child("rotationY").text().get()));
						ab->RotateZ(std::stof(settingItem.child("rotationZ").text().get()));
					}

				}
				else if (type == "extension") {
					std::string stage = settingItem.attribute("stage").as_string();

					if (stage == "vertex")
						strcpy(Settings::Instance().Project.GLSLVS_Extenstion, settingItem.text().get());
					if (stage == "pixel")
						strcpy(Settings::Instance().Project.GLSLPS_Extenstion, settingItem.text().get());
					if (stage == "geometry")
						strcpy(Settings::Instance().Project.GLSLGS_Extenstion, settingItem.text().get());
				}
				else if (type == "clearcolor") {
					if (!settingItem.attribute("r").empty())
						Settings::Instance().Project.ClearColor.R = settingItem.attribute("r").as_uint();
					if (!settingItem.attribute("g").empty())
						Settings::Instance().Project.ClearColor.G = settingItem.attribute("g").as_uint();
					if (!settingItem.attribute("b").empty())
						Settings::Instance().Project.ClearColor.B = settingItem.attribute("b").as_uint();
					if (!settingItem.attribute("a").empty())
						Settings::Instance().Project.ClearColor.A = settingItem.attribute("a").as_uint();
				}
			}
		}
	}
	void ProjectParser::OpenTemplate()
	{
		char appPath[FILENAME_MAX];

		GetCurrentDirectoryA(sizeof(appPath), appPath);


		Open(std::string(appPath) + "\\templates\\" + m_template + "\\template.sprj");
		m_file = ""; // disallow overwriting template.sprj project file
	}
	void ProjectParser::Save()
	{
		SaveAs(m_file);
	}
	void ProjectParser::SaveAs(const std::string & file, bool copyFiles)
	{
		m_file = file;
		std::string oldProjectPath = m_projectPath;
		SetProjectDirectory(file.substr(0, file.find_last_of("/\\")));

		std::vector<PipelineItem*> passItems = m_pipe->GetList();

		std::string shadersDir = m_projectPath + "\\shaders";
		if (copyFiles) {
			CreateDirectoryA(shadersDir.c_str(), NULL);

			std::string proj = oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '\\') ? "" : "\\");
			
			for (PipelineItem* passItem : passItems) {
				pipe::ShaderPass* passData = (pipe::ShaderPass*)passItem->Data;

				std::string vs = proj + std::string(passData->VSPath);
				std::string ps = proj + std::string(passData->PSPath);

				std::string vsExt = ed::GLSL2HLSL::IsGLSL(vs) ? Settings::Instance().Project.GLSLVS_Extenstion : "hlsl";
				std::string psExt = ed::GLSL2HLSL::IsGLSL(ps) ? Settings::Instance().Project.GLSLPS_Extenstion : "hlsl";

				CopyFileA(vs.c_str(), (shadersDir + "\\" + passItem->Name + "VS." + vsExt).c_str(), false);
				CopyFileA(ps.c_str(), (shadersDir + "\\" + passItem->Name + "PS." + psExt).c_str(), false);

				if (passData->GSUsed) {
					std::string gs = proj + std::string(passData->GSPath);
					std::string gsExt = ed::GLSL2HLSL::IsGLSL(gs) ? Settings::Instance().Project.GLSLGS_Extenstion : "hlsl";
					CopyFileA(gs.c_str(), (shadersDir + "\\" + passItem->Name + "GS." + gsExt).c_str(), false);
				}
			}
		}

		pugi::xml_document doc;
		pugi::xml_node projectNode = doc.append_child("project");
		pugi::xml_node pipelineNode = projectNode.append_child("pipeline");
		pugi::xml_node objectsNode = projectNode.append_child("objects");
		pugi::xml_node settingsNode = projectNode.append_child("settings");

		// shader passes
		for (PipelineItem* passItem : passItems) {
			pipe::ShaderPass* passData = (pipe::ShaderPass*)passItem->Data;

			pugi::xml_node passNode = pipelineNode.append_child("pass");
			passNode.append_attribute("name").set_value(passItem->Name);

			for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
				if (passData->RenderTexture[i][0] == 0)
					break;

				passNode.append_child("rendertexture").append_attribute("name").set_value(passData->RenderTexture[i]);
			}

			// vertex shader
			pugi::xml_node vsNode = passNode.append_child("shader");
			std::string relativePath = GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '\\') ? "" : "\\") + std::string(passData->VSPath));
			if (copyFiles) {
				std::string vsExt = ed::GLSL2HLSL::IsGLSL(passData->VSPath) ? Settings::Instance().Project.GLSLVS_Extenstion : "hlsl";
				relativePath = "shaders\\" + std::string(passItem->Name) + "VS." +vsExt;
			}

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
			if (copyFiles) {
				std::string psExt = ed::GLSL2HLSL::IsGLSL(passData->PSPath) ? Settings::Instance().Project.GLSLPS_Extenstion : "hlsl";
				relativePath = "shaders\\" + std::string(passItem->Name) + "PS." + psExt;
			}

			psNode.append_attribute("type").set_value("ps");
			psNode.append_child("path").text().set(relativePath.c_str());
			psNode.append_child("entry").text().set(passData->PSEntry);
			m_exportShaderVariables(psNode, passData->PSVariables.GetVariables());

			// geometry shader
			if (strlen(passData->GSEntry) > 0 && strlen(passData->GSPath) > 0) {
				pugi::xml_node gsNode = passNode.append_child("shader");
				relativePath = GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '\\') ? "" : "\\") + std::string(passData->GSPath));
				if (copyFiles) {
					std::string vsExt = ed::GLSL2HLSL::IsGLSL(passData->GSPath) ? Settings::Instance().Project.GLSLGS_Extenstion : "hlsl";
					relativePath = "shaders\\" + std::string(passItem->Name) + "GS.hlsl";
				}

				gsNode.append_attribute("used").set_value(passData->GSUsed);

				gsNode.append_attribute("type").set_value("gs");
				gsNode.append_child("path").text().set(relativePath.c_str());
				gsNode.append_child("entry").text().set(passData->GSEntry);
				m_exportShaderVariables(gsNode, passData->GSVariables.GetVariables());

			}


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
					if (tData->Position.x != 0.0f) itemNode.append_child("x").text().set(tData->Position.x);
					if (tData->Position.y != 0.0f) itemNode.append_child("y").text().set(tData->Position.y);
					if (tData->Position.z != 0.0f) itemNode.append_child("z").text().set(tData->Position.z);
					itemNode.append_child("topology").text().set(TOPOLOGY_ITEM_NAMES[(int)tData->Topology]);
				}
				else if (item->Type == PipelineItem::ItemType::BlendState) {
					itemNode.append_attribute("type").set_value("blend");

					ed::pipe::BlendState* tData = reinterpret_cast<ed::pipe::BlendState*>(item->Data);
					D3D11_RENDER_TARGET_BLEND_DESC* bDesc = &tData->State.Info.RenderTarget[0];
					ml::Color blendFactor = tData->State.GetBlendFactor();

					itemNode.append_child("srcblend").text().set(BLEND_NAMES[bDesc->SrcBlend]);
					itemNode.append_child("blendop").text().set(BLEND_OPERATOR_NAMES[bDesc->BlendOp]);
					itemNode.append_child("destblend").text().set(BLEND_NAMES[bDesc->DestBlend]);
					itemNode.append_child("srcblendalpha").text().set(BLEND_NAMES[bDesc->SrcBlendAlpha]);
					itemNode.append_child("alphablendop").text().set(BLEND_OPERATOR_NAMES[bDesc->BlendOpAlpha]);
					itemNode.append_child("destblendalpha").text().set(BLEND_NAMES[bDesc->DestBlendAlpha]);
					itemNode.append_child("alpha2cov").text().set(tData->State.Info.AlphaToCoverageEnable);
					if (blendFactor.R != 0) itemNode.append_child("bf_red").text().set(blendFactor.R);
					if (blendFactor.G != 0) itemNode.append_child("bf_green").text().set(blendFactor.G);
					if (blendFactor.B != 0) itemNode.append_child("bf_blue").text().set(blendFactor.B);
					if (blendFactor.A != 0) itemNode.append_child("bf_alpha").text().set(blendFactor.A);
				}
				else if (item->Type == PipelineItem::ItemType::DepthStencilState) {
					itemNode.append_attribute("type").set_value("depthstencil");

					ed::pipe::DepthStencilState* tData = reinterpret_cast<ed::pipe::DepthStencilState*>(item->Data);
					D3D11_DEPTH_STENCIL_DESC* bDesc = &tData->State.Info;

					itemNode.append_child("depthenable").text().set(bDesc->DepthEnable);
					itemNode.append_child("depthfunc").text().set(COMPARISON_FUNCTION_NAMES[bDesc->DepthFunc]);
					itemNode.append_child("stencilenable").text().set(bDesc->StencilEnable);
					itemNode.append_child("frontfunc").text().set(COMPARISON_FUNCTION_NAMES[bDesc->FrontFace.StencilFunc]);
					itemNode.append_child("frontpass").text().set(STENCIL_OPERATION_NAMES[bDesc->FrontFace.StencilPassOp]);
					itemNode.append_child("frontfail").text().set(STENCIL_OPERATION_NAMES[bDesc->FrontFace.StencilFailOp]);
					itemNode.append_child("backfunc").text().set(COMPARISON_FUNCTION_NAMES[bDesc->BackFace.StencilFunc]);
					itemNode.append_child("backpass").text().set(STENCIL_OPERATION_NAMES[bDesc->BackFace.StencilPassOp]);
					itemNode.append_child("backfail").text().set(STENCIL_OPERATION_NAMES[bDesc->BackFace.StencilFailOp]);
					if (tData->StencilReference != 0) itemNode.append_child("sref").text().set(tData->StencilReference);
				}
				else if (item->Type == PipelineItem::ItemType::RasterizerState) {
					itemNode.append_attribute("type").set_value("rasterizer");

					ed::pipe::RasterizerState* tData = reinterpret_cast<ed::pipe::RasterizerState*>(item->Data);
					D3D11_RASTERIZER_DESC* bDesc = &tData->State.Info;

					itemNode.append_child("wireframe").text().set(bDesc->FillMode == D3D11_FILL_WIREFRAME);
					itemNode.append_child("cull").text().set(CULL_MODE_NAMES[bDesc->CullMode]);
					itemNode.append_child("ccw").text().set(bDesc->FrontCounterClockwise);
					itemNode.append_child("depthbias").text().set(bDesc->DepthBias);
					itemNode.append_child("depthbiasclamp").text().set(bDesc->DepthBiasClamp);
					itemNode.append_child("slopebias").text().set(bDesc->SlopeScaledDepthBias);
					itemNode.append_child("depthclip").text().set(bDesc->DepthClipEnable);
					itemNode.append_child("aa").text().set(bDesc->AntialiasedLineEnable);
				}
				else if (item->Type == PipelineItem::ItemType::OBJModel) {
					itemNode.append_attribute("type").set_value("model");

					ed::pipe::OBJModel* data = reinterpret_cast<ed::pipe::OBJModel*>(item->Data);

					std::string opath = GetRelativePath(oldProjectPath + ((oldProjectPath[oldProjectPath.size() - 1] == '\\') ? "" : "\\") + std::string(data->Filename));;

					itemNode.append_child("filepath").text().set(opath.c_str());
					itemNode.append_child("grouponly").text().set(data->OnlyGroup);
					if (data->OnlyGroup) itemNode.append_child("group").text().set(data->GroupName);
					if (data->Scale.x != 1.0f) itemNode.append_child("scaleX").text().set(data->Scale.x);
					if (data->Scale.y != 1.0f) itemNode.append_child("scaleY").text().set(data->Scale.y);
					if (data->Scale.z != 1.0f) itemNode.append_child("scaleZ").text().set(data->Scale.z);
					if (data->Rotation.z != 0.0f) itemNode.append_child("roll").text().set(data->Rotation.z);
					if (data->Rotation.x != 0.0f) itemNode.append_child("pitch").text().set(data->Rotation.x);
					if (data->Rotation.y != 0.0f) itemNode.append_child("yaw").text().set(data->Rotation.y);
					if (data->Position.x != 0.0f) itemNode.append_child("x").text().set(data->Position.x);
					if (data->Position.y != 0.0f) itemNode.append_child("y").text().set(data->Position.y);
					if (data->Position.z != 0.0f) itemNode.append_child("z").text().set(data->Position.z);
				}
			}


			// item variable values
			pugi::xml_node itemValuesNode = passNode.append_child("itemvalues");
			std::vector<RenderEngine::ItemVariableValue> itemValues = m_renderer->GetItemVariableValues();
			std::vector<ShaderVariable*>& psVars = passData->PSVariables.GetVariables();
			std::vector<ShaderVariable*>& gsVars = passData->GSVariables.GetVariables();
			for (auto itemVal : itemValues) {
				bool found = false;
				for (auto passChild : passData->Items)
					if (passChild == itemVal.Item) {
						found = true;
						break;
					}
				if (!found) continue;

				pugi::xml_node itemValueNode = itemValuesNode.append_child("value");

				std::string from = "vs";
				for (auto psVar : psVars)
					if (strcmp(psVar->Name, itemVal.Variable->Name) == 0) {
						from = "ps";
						break;
					}
				for (auto gsVar : gsVars)
					if (strcmp(gsVar->Name, itemVal.Variable->Name) == 0) {
						from = "gs";
						break;
					}

				itemValueNode.append_attribute("from").set_value(from.c_str());
				itemValueNode.append_attribute("variable").set_value(itemVal.Variable->Name);
				itemValueNode.append_attribute("for").set_value(itemVal.Item->Name);

				m_exportVariableValue(itemValueNode, itemVal.NewValue);
			}
		}

		// objects
		{
			// textures
			std::vector<std::string> texs = m_objects->GetObjects();
			for (int i = 0; i < texs.size(); i++) {
				bool isRT = m_objects->IsRenderTexture(texs[i]);

				pugi::xml_node textureNode = objectsNode.append_child("object");
				textureNode.append_attribute("type").set_value(isRT ? "rendertexture" : "texture");
				textureNode.append_attribute(isRT ? "name" : "path").set_value(texs[i].c_str());

				if (!isRT) {
					bool isCube = m_objects->IsCubeMap(texs[i]);
					if (isCube)
						textureNode.append_attribute("cube").set_value(isCube);
				}

				if (isRT) {
					ed::RenderTextureObject* rtObj = m_objects->GetRenderTexture(texs[i]);
					if (rtObj->FixedSize.x != -1)
						textureNode.append_attribute("fsize").set_value((std::to_string(rtObj->FixedSize.x) + "," + std::to_string(rtObj->FixedSize.y)).c_str());
					else
						textureNode.append_attribute("rsize").set_value((std::to_string(rtObj->RatioSize.x) + "," + std::to_string(rtObj->RatioSize.y)).c_str());

					if (rtObj->ClearColor.R != 0) textureNode.append_attribute("r").set_value(rtObj->ClearColor.R);
					if (rtObj->ClearColor.G != 0) textureNode.append_attribute("g").set_value(rtObj->ClearColor.G);
					if (rtObj->ClearColor.B != 0) textureNode.append_attribute("b").set_value(rtObj->ClearColor.B);
					if (rtObj->ClearColor.A != 0) textureNode.append_attribute("a").set_value(rtObj->ClearColor.A);
				}

				ml::ShaderResourceView* mySRV = m_objects->GetSRV(texs[i]);

				for (int j = 0; j < passItems.size(); j++) {
					std::vector<ml::ShaderResourceView*> bound = m_objects->GetBindList(passItems[j]);

					for (int slot = 0; slot < bound.size(); slot++)
						if (bound[slot] == mySRV) {
							pugi::xml_node bindNode = textureNode.append_child("bind");
							bindNode.append_attribute("slot").set_value(slot);
							bindNode.append_attribute("name").set_value(passItems[j]->Name);
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
			}

			// code editor ui
			CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get(ViewID::Code));
			std::vector<std::pair<std::string, int>> files = editor->GetOpenedFiles();
			for (const auto& file : files) {
				pugi::xml_node fileNode = settingsNode.append_child("entry");
				fileNode.append_attribute("type").set_value("file");
				fileNode.append_attribute("name").set_value(file.first.c_str());
				fileNode.append_attribute("shader").set_value(file.second == 0 ? "vs" : (file.second == 1 ? "ps" : "gs"));
			}

			// pinned ui
			PinnedUI* pinned = ((PinnedUI*)m_ui->Get(ViewID::Pinned));
			std::vector<ShaderVariable*> pinnedVars = pinned->GetAll();
			for (auto var : pinnedVars) {
				pugi::xml_node varNode = settingsNode.append_child("entry");
				varNode.append_attribute("type").set_value("pinned");
				varNode.append_attribute("name").set_value(var->Name);

				for (PipelineItem* passItem : passItems) {
					pipe::ShaderPass* data = (pipe::ShaderPass*)passItem->Data;
					bool found = false;

					std::vector<ShaderVariable*> vsVars = data->VSVariables.GetVariables();
					for (int i = 0; i < vsVars.size(); i++) {
						if (vsVars[i] == var) {
							varNode.append_attribute("from").set_value("vs");
							varNode.append_attribute("owner").set_value(passItem->Name);
							found = true;
						}
					}

					if (!found) {
						std::vector<ShaderVariable*> psVars = data->PSVariables.GetVariables();
						for (int i = 0; i < psVars.size(); i++) {
							if (psVars[i] == var) {
								varNode.append_attribute("from").set_value("ps");
								varNode.append_attribute("owner").set_value(passItem->Name);
								found = true;
							}
						}
					}

					if (!found) {
						std::vector<ShaderVariable*> gsVars = data->GSVariables.GetVariables();
						for (int i = 0; i < gsVars.size(); i++) {
							if (gsVars[i] == var) {
								varNode.append_attribute("from").set_value("gs");
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
				if (Settings::Instance().Project.FPCamera) {
					ed::FirstPersonCamera* cam = (ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera();
					pugi::xml_node camNode = settingsNode.append_child("entry");
					camNode.append_attribute("type").set_value("camera");
					camNode.append_attribute("fp").set_value(true);

					DirectX::XMFLOAT3 rota = cam->GetRotation();

					camNode.append_child("positionX").text().set(DirectX::XMVectorGetX(cam->GetPosition()));
					camNode.append_child("positionY").text().set(DirectX::XMVectorGetY(cam->GetPosition()));
					camNode.append_child("positionZ").text().set(DirectX::XMVectorGetZ(cam->GetPosition()));
					camNode.append_child("yaw").text().set(rota.x);
					camNode.append_child("pitch").text().set(rota.y);
				} else {
					ed::ArcBallCamera* cam = (ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera();
					pugi::xml_node camNode = settingsNode.append_child("entry");
					camNode.append_attribute("type").set_value("camera");
					camNode.append_attribute("fp").set_value(false);

					DirectX::XMFLOAT3 rota = cam->GetRotation();

					camNode.append_child("distance").text().set(cam->GetDistance());
					camNode.append_child("rotationX").text().set(rota.x);
					camNode.append_child("rotationY").text().set(rota.y);
					camNode.append_child("rotationZ").text().set(rota.z);
				}
			}

			// extensions
			{
				pugi::xml_node extNodeVS = settingsNode.append_child("extension");
				extNodeVS.append_attribute("stage").set_value("vertex");
				extNodeVS.text().set(Settings::Instance().Project.GLSLVS_Extenstion);

				pugi::xml_node extNodePS = settingsNode.append_child("extension");
				extNodePS.append_attribute("stage").set_value("pixel");
				extNodePS.text().set(Settings::Instance().Project.GLSLPS_Extenstion);

				pugi::xml_node extNodeGS = settingsNode.append_child("extension");
				extNodeGS.append_attribute("stage").set_value("geometry");
				extNodeGS.text().set(Settings::Instance().Project.GLSLGS_Extenstion);
			}

			// clear color
			{
				pugi::xml_node colorNode = settingsNode.append_child("entry");
				colorNode.append_attribute("type").set_value("clearcolor");
				colorNode.append_attribute("r").set_value(Settings::Instance().Project.ClearColor.R);
				colorNode.append_attribute("g").set_value(Settings::Instance().Project.ClearColor.G);
				colorNode.append_attribute("b").set_value(Settings::Instance().Project.ClearColor.B);
				colorNode.append_attribute("a").set_value(Settings::Instance().Project.ClearColor.A);
			}
		}

		doc.save_file(file.c_str());
	}
	std::string ProjectParser::LoadProjectFile(const std::string & file)
	{
		std::ifstream in(m_projectPath + ((m_projectPath[m_projectPath.size() - 1] == '\\') ? "" : "\\") + file);
		if (in.is_open()) {
			in.seekg(0, std::ios::beg);

			std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
			in.close();
			return content;
		}
		return "";
	}
	char * ProjectParser::LoadProjectFile(const std::string& file, size_t& fsize)
	{
		std::string actual = m_projectPath + ((m_projectPath[m_projectPath.size() - 1] == '\\') ? "" : "\\") + file;

		FILE *f = fopen(actual.c_str(), "rb");
		fseek(f, 0, SEEK_END);
		fsize = ftell(f);
		fseek(f, 0, SEEK_SET);  //same as rewind(f);

		char *string = (char*)malloc(fsize + 1);
		fread(string, fsize, 1, f);
		fclose(f);

		string[fsize] = 0;

		return string;
	}
	ml::OBJModel* ProjectParser::LoadModel(const std::string& file)
	{
		// return already loaded model
		for (auto& mdl : m_models)
			if (mdl.first == file)
				return mdl.second;

		// load the model
		std::string data = LoadProjectFile(file);
		ml::OBJModel* model = new ml::OBJModel();
		bool loaded = model->LoadFromMemory(data.c_str(), data.size());
		if (!loaded)
			return nullptr;

		m_models.push_back(std::make_pair(file, model));

		return m_models[m_models.size() - 1].second;
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
		
		// remove ./
		std::string ret(relativePath);
		for (int i = 1; i < ret.size(); i++)
			if (ret[i] == '/' || ret[i] == '\\')
				if (ret[i - 1] == '.')
					if (i == 1 || ret[i - 2] != '.') {
						int ind = i - 1;
						ret.erase(ind, 2);
						i -= 2;
					}

		return ret;
	}
	std::string ProjectParser::GetProjectPath(const std::string& to)
	{
		return m_projectPath + ((m_projectPath[m_projectPath.size() - 1] == '\\') ? "" : "\\") + to;
	}
	bool ProjectParser::FileExists(const std::string& str)
	{
		DWORD dwAttrib = GetFileAttributesA(GetProjectPath(str).c_str());

		return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}
	void ProjectParser::ResetProjectDirectory()
	{
		m_file = "";
		char currentPath[FILENAME_MAX] = { 0 };
		_getcwd(currentPath, FILENAME_MAX);
		m_projectPath = currentPath;
	}


	void ProjectParser::m_parseVariableValue(pugi::xml_node& node, ShaderVariable* var)
	{
		int rowID = 0;
		for (pugi::xml_node row : node.children("row")) {
			int colID = 0;
			for (pugi::xml_node value : row.children("value")) {
				if (var->Function != FunctionShaderVariable::None)
					*FunctionVariableManager::LoadFloat(var->Arguments, colID++) = value.text().as_float();
				else {
					if (var->GetType() >= ShaderVariable::ValueType::Boolean1 && var->GetType() <= ShaderVariable::ValueType::Boolean4)
						var->SetBooleanValue(value.text().as_bool(), colID++);
					else if (var->GetType() >= ShaderVariable::ValueType::Integer1 && var->GetType() <= ShaderVariable::ValueType::Integer4)
						var->SetIntegerValue(value.text().as_int(), colID++);
					else
						var->SetFloat(value.text().as_float(), colID++, rowID);
				}

			}
			rowID++;
		}
	}
	void ProjectParser::m_exportVariableValue(pugi::xml_node& node, ShaderVariable* var)
	{
		pugi::xml_node valueRowNode = node.append_child("row");

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
					valueRowNode = node.append_child("row");
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

				if (var->System == SystemShaderVariable::None)
					m_exportVariableValue(varNode, var);
			}
		}
	}
	D3D11_BLEND ProjectParser::m_toBlend(const char* text)
	{
		for (int k = 0; k < _ARRAYSIZE(BLEND_NAMES); k++)
			if (strcmp(text, BLEND_NAMES[k]) == 0)
				return (D3D11_BLEND)k;
		return D3D11_BLEND_BLEND_FACTOR;
	}
	D3D11_BLEND_OP ProjectParser::m_toBlendOp(const char* text)
	{
		for (int k = 0; k < _ARRAYSIZE(BLEND_OPERATOR_NAMES); k++)
			if (strcmp(text, BLEND_OPERATOR_NAMES[k]) == 0)
				return (D3D11_BLEND_OP)k;
		return D3D11_BLEND_OP_ADD;
	}
	D3D11_COMPARISON_FUNC ProjectParser::m_toComparisonFunc(const char * str)
	{
		for (int k = 0; k < _ARRAYSIZE(COMPARISON_FUNCTION_NAMES); k++)
			if (strcmp(str, COMPARISON_FUNCTION_NAMES[k]) == 0)
				return (D3D11_COMPARISON_FUNC)k;
		return D3D11_COMPARISON_ALWAYS;
	}
	D3D11_STENCIL_OP ProjectParser::m_toStencilOp(const char * str)
	{
		for (int k = 0; k < _ARRAYSIZE(STENCIL_OPERATION_NAMES); k++)
			if (strcmp(str, STENCIL_OPERATION_NAMES[k]) == 0)
				return (D3D11_STENCIL_OP)k;
		return D3D11_STENCIL_OP_KEEP;
	}
	D3D11_CULL_MODE ProjectParser::m_toCullMode(const char* str)
	{
		for (int k = 0; k < _ARRAYSIZE(CULL_MODE_NAMES); k++)
			if (strcmp(str, CULL_MODE_NAMES[k]) == 0)
				return (D3D11_CULL_MODE)k;
		return D3D11_CULL_BACK;
	}
}