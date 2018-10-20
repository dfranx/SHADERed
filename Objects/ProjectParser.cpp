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
			std::string geoType = "cube";

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
								auto myFormat = entry.attribute("format").as_string();
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
								auto myType = entry.attribute("type").as_string();
								for (int i = 0; i < _ARRAYSIZE(VARIABLE_TYPE_NAMES); i++)
									if (strcmp(myType, VARIABLE_TYPE_NAMES[i]) == 0) {
										type = (ed::ShaderVariable::ValueType)i;
										break;
									}
							}
							if (!entry.attribute("system").empty()) {
								auto mySystem = entry.attribute("system").as_string();
								for (int i = 0; i < _ARRAYSIZE(SYSTEM_VARIABLE_NAMES); i++)
									if (strcmp(mySystem, SYSTEM_VARIABLE_NAMES[i]) == 0) {
										system = (ed::SystemShaderVariable)i;
										break;
									}
								if (SystemVariableManager::GetType(system) != type)
									system = ed::SystemShaderVariable::None;
							}
							if (!entry.attribute("function").empty()) {
								auto myFunc = entry.attribute("function").as_string();
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
								for (pugi::xml_node row : attrItem.children("row")) {
									int colID = 0;
									for (pugi::xml_node value : row.children("value")) {
										if (func != FunctionShaderVariable::None)
											var.SetFloat(value.text().as_float(), colID++, rowID);
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

							tData->Variables.Add(entry.text().as_string(), type, system, slot);
						}
					}
				} else if (type == ed::PipelineItem::Geometry) {
					ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(data);

					if (strcmp(attrItem.name(), "width") == 0)
						tData->Scale.x = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "height") == 0)
						tData->Scale.y = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "depth") == 0)
						tData->Scale.z = attrItem.text().as_float();
					else if (strcmp(attrItem.name(), "topology") == 0) {
						for (int k = 0; k < _ARRAYSIZE(TOPOLOGY_ITEM_NAMES); k++)
							if (strcmp(attrItem.text().as_string(), TOPOLOGY_ITEM_NAMES[k]) == 0)
								tData->Topology = (ml::Topology::Type)k;
					} else if (strcmp(attrItem.name(), "type") == 0)
						geoType = attrItem.text().as_string();
				}
			}

			if (type == ed::PipelineItem::Geometry) {
				ed::pipe::GeometryItem* tData = reinterpret_cast<ed::pipe::GeometryItem*>(data);
				if (geoType == "cube")
					tData->Geometry = ml::GeometryFactory::CreateCube(tData->Scale.x, tData->Scale.y, tData->Scale.z, *m_pipe->GetOwner());
			}
		}
	}
	void ProjectParser::Save()
	{}
	void ProjectParser::SaveAs(const std::string & file)
	{
		
	}
}