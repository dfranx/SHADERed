#include "ProjectParser.h"
#include "SystemVariableManager.h"
#include "FunctionVariableManager.h"
#include "Names.h"
#include "../pugixml/pugixml.hpp"

#include <MoonLight/Base/GeometryFactory.h>

namespace ed
{
	ProjectParser::ProjectParser(PipelineManager* pipeline) :
		m_pipe(pipeline), m_file("")
	{}
	ProjectParser::~ProjectParser()
	{}
	void ProjectParser::Open(const std::string & file)
	{
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(file.c_str());
		if (!result)
			return;

		m_pipe->Clear();

		for (pugi::xml_node pipeItem : doc.child("project").child("pipeline").children("item")) {
			char name[PIPELINE_ITEM_NAME_LENGTH];
			ed::PipelineItem type = ed::PipelineItem::ShaderFile;
			void* data = nullptr;

			// parse type
			if (!pipeItem.attribute("type").empty()) {
				if (strcmp(pipeItem.attribute("type").as_string(), "shader") == 0) {
					type = ed::PipelineItem::ShaderFile;
					data = new ed::pipe::ShaderItem();
				} else if (strcmp(pipeItem.attribute("type").as_string(), "geometry") == 0) {
					type = ed::PipelineItem::Geometry;
					data = new ed::pipe::GeometryItem();
				}
			}

			// parse name
			if (!pipeItem.attribute("type").empty())
				strcpy(name, pipeItem.attribute("name").as_string());
			
			// add the item
			m_pipe->Add(name, type, data);

			// check for more properties
			for (pugi::xml_node attrItem : pipeItem.children()) {
					// search for specific elements for each type
				if (type == ed::PipelineItem::ShaderFile) {
					ed::pipe::ShaderItem* tData = reinterpret_cast<ed::pipe::ShaderItem*>(data);

					if (strcmp(attrItem.name(), "path") == 0)
						strcpy(tData->FilePath, attrItem.text().as_string());
					else if (strcmp(attrItem.name(), "entry") == 0)
						strcpy(tData->Entry, attrItem.text().as_string());
					else if (strcmp(attrItem.name(), "type") == 0) {
						for (int k = 0; k < _ARRAYSIZE(SHADER_TYPE_NAMES); k++)
							if (strcmp(attrItem.text().as_string(), SHADER_TYPE_NAMES[k]) == 0) {
								tData->Type = (ed::pipe::ShaderItem::ShaderType)k;
								break;
							}
					}
					else if (strcmp(attrItem.name(), "input") == 0) {
						for (pugi::xml_node entry : attrItem.children()) {
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
							tData->InputLayout.Add(entry.text().as_string(), sID, format, offset);
						}
					}
					else if (strcmp(attrItem.name(), "variables") == 0) {
						for (pugi::xml_node entry : attrItem.children("variable")) {
							ShaderVariable::ValueType type = ShaderVariable::ValueType::Float1;
							SystemShaderVariable system = SystemShaderVariable::None;
							FunctionShaderVariable func = FunctionShaderVariable::None;
							int slot = 0;

							if (!entry.attribute("slot").empty()) slot = entry.attribute("slot").as_int();
							if (!entry.attribute("type").empty()) {
								const char* myType = entry.attribute("type").as_string();
								for (int i = 0; i < _ARRAYSIZE(VARIABLE_TYPE_NAMES); i++)
									if (strcmp(myType, VARIABLE_TYPE_NAMES[i]) == 0) {
										type = (ed::ShaderVariable::ValueType)i;
										break;
									}
							}
							if (!entry.attribute("system").empty()) {
								const char* mySystem = entry.attribute("system").as_string();
								for (int i = 0; i < _ARRAYSIZE(SYSTEM_VARIABLE_NAMES); i++)
									if (strcmp(mySystem, SYSTEM_VARIABLE_NAMES[i]) == 0) {
										system = (ed::SystemShaderVariable)i;
										break;
									}
								if (SystemVariableManager::GetType(system) != type)
									system = ed::SystemShaderVariable::None;
							}
							if (!entry.attribute("function").empty()) {
								const char* myFunc = entry.attribute("function").as_string();
								for (int i = 0; i < _ARRAYSIZE(FUNCTION_NAMES); i++)
									if (strcmp(myFunc, FUNCTION_NAMES[i]) == 0) {
										func = (FunctionShaderVariable)i;
										break;
									}
								if (system != SystemShaderVariable::None || !FunctionVariableManager::HasValidReturnType(type, func))
									func = FunctionShaderVariable::None;
							}

							ShaderVariable var(type, entry.attribute("name").as_string(), system, slot);
							FunctionVariableManager::AllocateArgumentSpace(var, func);

							// parse value
							if (system == SystemShaderVariable::None) {
								int rowID = 0;
								for (pugi::xml_node row : entry.children("row")) {
									int colID = 0;
									for (pugi::xml_node value : row.children("value")) {
										if (func != FunctionShaderVariable::None)
											*FunctionVariableManager::LoadFloat(var.Arguments, colID++) = value.text().as_float();
										else {
											if (type >= ShaderVariable::ValueType::Boolean1 && type <= ShaderVariable::ValueType::Boolean4)
												var.SetBooleanValue(value.text().as_bool(), colID++);
											else if (type >= ShaderVariable::ValueType::Integer1 && type <= ShaderVariable::ValueType::Integer4)
												var.SetIntegerValue(value.text().as_int(), colID++);
											else
												var.SetFloat(value.text().as_float(), colID++, rowID);
										}

									}
									rowID++;
								}
							}

							tData->Variables.Add(var);
						}
					}
				} else if (type == ed::PipelineItem::Geometry) {
					ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(data);

					if (strcmp(attrItem.name(), "width") == 0)
						tData->Size.x = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "height") == 0)
						tData->Size.y = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "depth") == 0)
						tData->Size.z = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "scaleX") == 0)
						tData->Scale.x = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "scaleY") == 0)
						tData->Scale.y = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "scaleZ") == 0)
						tData->Scale.z = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "roll") == 0)
						tData->Rotation.z = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "yaw") == 0)
						tData->Rotation.y = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "pitch") == 0)
						tData->Rotation.x = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "x") == 0)
						tData->Position.x = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "y") == 0)
						tData->Position.y = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "z") == 0)
						tData->Position.z = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "topology") == 0) {
						for (int k = 0; k < _ARRAYSIZE(TOPOLOGY_ITEM_NAMES); k++)
							if (strcmp(attrItem.text().as_string(), TOPOLOGY_ITEM_NAMES[k]) == 0)
								tData->Topology = (ml::Topology::Type)k;
					} else if (strcmp(attrItem.name(), "type") == 0) {
						for (int k = 0; k < _ARRAYSIZE(GEOMETRY_NAMES); k++)
							if (strcmp(attrItem.text().as_string(), GEOMETRY_NAMES[k]) == 0)
								tData->Type = (pipe::GeometryItem::GeometryType)k;
					}
				}
			}

			if (type == ed::PipelineItem::Geometry) {
				ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(data);
				if (tData->Type == pipe::GeometryItem::Cube)
					tData->Geometry = ml::GeometryFactory::CreateCube(tData->Size.x, tData->Size.y, tData->Size.z, *m_pipe->GetOwner());
			}
		}
	}
	void ProjectParser::Save()
	{
		SaveAs(m_file);
	}
	void ProjectParser::SaveAs(const std::string & file)
	{
		m_file = file;

		pugi::xml_document doc;
		pugi::xml_node projectNode = doc.append_child("project");
		pugi::xml_node pipelineNode = projectNode.append_child("pipeline");

		std::vector<PipelineManager::Item> pipelineItems = m_pipe->GetList();
		for (PipelineManager::Item& item : pipelineItems) {
			pugi::xml_node itemNode = pipelineNode.append_child("item");

			itemNode.append_attribute("name").set_value(item.Name);
			
			pugi::xml_attribute typeAttr = itemNode.append_attribute("type");
			if (item.Type == PipelineItem::ShaderFile) {
				typeAttr.set_value("shader");

				pipe::ShaderItem* tData = reinterpret_cast<pipe::ShaderItem*>(item.Data);

				itemNode.append_child("path").text().set(tData->FilePath);
				itemNode.append_child("entry").text().set(tData->Entry);
				itemNode.append_child("type").text().set(SHADER_TYPE_NAMES[(int)tData->Type]);

				// export <input> layout tag
				if (tData->Type == pipe::ShaderItem::VertexShader) {
					pugi::xml_node layoutNode = itemNode.append_child("input");

					std::vector<D3D11_INPUT_ELEMENT_DESC> layoutItems = tData->InputLayout.GetInputElements();
					for (D3D11_INPUT_ELEMENT_DESC layoutItem : layoutItems) {
						pugi::xml_node layoutItemNode = layoutNode.append_child("element");

						layoutItemNode.text().set(layoutItem.SemanticName);
						layoutItemNode.append_attribute("id").set_value(layoutItem.SemanticIndex);
						layoutItemNode.append_attribute("format").set_value(FORMAT_NAMES[(int)layoutItem.Format]);
						layoutItemNode.append_attribute("offset").set_value(layoutItem.AlignedByteOffset);
					}
				}

				// export variables
				std::vector<ShaderVariable> vars = tData->Variables.GetVariables();
				if (vars.size() > 0) {
					pugi::xml_node varsNodes = itemNode.append_child("variables");
					for (ShaderVariable var : vars) {
						pugi::xml_node varNode = varsNodes.append_child("variable");
						varNode.append_attribute("type").set_value(VARIABLE_TYPE_NAMES[(int)var.GetType()]);
						varNode.append_attribute("name").set_value(var.Name);

						if (var.System != SystemShaderVariable::None)
							varNode.append_attribute("system").set_value(SYSTEM_VARIABLE_NAMES[(int)var.System]);
						else if (var.Function != FunctionShaderVariable::None)
							varNode.append_attribute("function").set_value(FUNCTION_NAMES[(int)var.Function]);

						varNode.append_attribute("slot").set_value(var.Slot);

						if (var.System == SystemShaderVariable::None) {
							pugi::xml_node valueRowNode = varNode.append_child("row");

							if (var.Function == FunctionShaderVariable::None) {
								int rowID = 0;
								for (int i = 0; i < ShaderVariable::GetSize(var.GetType()) / 4; i++) {
									if (var.GetType() >= ShaderVariable::ValueType::Boolean1 && var.GetType() <= ShaderVariable::ValueType::Boolean4)
										valueRowNode.append_child("value").text().set(var.AsBoolean(i));
									else if (var.GetType() >= ShaderVariable::ValueType::Integer1 && var.GetType() <= ShaderVariable::ValueType::Integer4)
										valueRowNode.append_child("value").text().set(var.AsInteger(i));
									else
										valueRowNode.append_child("value").text().set(var.AsFloat(i%var.GetColumnCount(), rowID));

									if (i%var.GetColumnCount() == 0 && i != 0) {
										valueRowNode = varNode.append_child("row");
										rowID++;
									}
								}
							} else {
								// save arguments
								for (int i = 0; i < FunctionVariableManager::GetArgumentCount(var.Function); i++) {
									valueRowNode.append_child("value").text().set(*FunctionVariableManager::LoadFloat(var.Arguments, i));
								}
							}
						}
					}
				}
			}
			else if (item.Type == PipelineItem::Geometry) {
				typeAttr.set_value("geometry");

				ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(item.Data);

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
		}

		doc.save_file(file.c_str());
	}
}