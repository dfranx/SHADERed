#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/SystemVariableManager.h>

#include <iomanip>

#define GET_VALUE_WITH_CHECK_FLOAT(val, c) (val == nullptr ? 0.0f : val->members[c].value.f)
#define GET_VALUE2_WITH_CHECK_FLOAT(val, c, r) (val == nullptr ? 0.0f : val->members[c].members[r].value.f)
#define GET_VALUE_WITH_CHECK_DOUBLE(val, c) (val == nullptr ? 0.0 : val->members[c].value.d)
#define GET_VALUE2_WITH_CHECK_DOUBLE(val, c, r) (val == nullptr ? 0.0 : val->members[c].members[r].value.d)
#define GET_VALUE_WITH_CHECK_INT(val, c) (val == nullptr ? 0 : val->members[c].value.s)
#define GET_VALUE2_WITH_CHECK_INT(val, c, r) (val == nullptr ? 0 : val->members[c].members[r].value.s)



namespace ed {
	DebugInformation::DebugInformation(ObjectManager* objs, RenderEngine* renderer, MessageStack* msgs)
	{
		m_objs = objs;
		m_renderer = renderer;
		m_isDebugging = false;
		m_vm = nullptr;
		m_shader = nullptr;
		m_pixel = nullptr;
		m_vmImmediate = nullptr;
		m_shaderImmediate = nullptr;
		m_msgs = msgs;

		m_vmContext = spvm_context_initialize();
		m_vmGLSL = spvm_build_glsl450_ext();
	}
	DebugInformation::~DebugInformation()
	{
		m_resetVM();

		free(m_vmGLSL);
		spvm_context_deinitialize(m_vmContext);
	}
	
	void DebugInformation::m_resetVM()
	{
		for (spvm_image_t img : m_images) {
			free(img->data);
			free(img);
		}
		m_images.clear();

		if (m_vm) {
			spvm_state_delete(m_vm);
			m_vm = nullptr;
		}
		if (m_shader) {
			spvm_program_delete(m_shader);
			m_shader = nullptr;
		}
	}
	void DebugInformation::m_setupVM(std::vector<unsigned int>& spv)
	{
		m_spv = spv;
		
		// create program & state
		m_shader = spvm_program_create(m_vmContext, (spvm_source)m_spv.data(), m_spv.size());
		m_vm = _spvm_state_create_base(m_shader, m_stage == ShaderStage::Pixel, 0);

		// link GLSL.std.450
		spvm_state_set_extension(m_vm, "GLSL.std.450", m_vmGLSL);
	}
	void DebugInformation::m_copyUniforms(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		bool pluginUsesCustomTextures = false;
		bool requiresVarCleanup = false;
		std::vector<ed::ShaderVariable*> vars;
		if (owner->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* pass = (pipe::ShaderPass*)owner->Data;
			vars = pass->Variables.GetVariables();
		} else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

			pluginUsesCustomTextures = plData->Owner->PipelineItem_DebugUsesCustomTextures(plData->Type, plData->PluginData);

			plData->Owner->PipelineItem_DebugPrepareVariables(plData->Type, plData->PluginData, item->Name);
			
			int varCount = plData->Owner->PipelineItem_GetVariableCount(plData->Type, plData->PluginData);
			vars.resize(varCount);

			for (int i = 0; i < varCount; i++) {
				const char* varName = plData->Owner->PipelineItem_GetVariableName(plData->Type, plData->PluginData, i);
				ShaderVariable::ValueType type = (ShaderVariable::ValueType)plData->Owner->PipelineItem_GetVariableType(plData->Type, plData->PluginData, i);

				ed::ShaderVariable* var = new ed::ShaderVariable(type, varName);
				vars[i] = var;
				int cm = var->GetColumnCount();
				int rm = var->GetRowCount();

				ShaderVariable::ValueType baseType = var->GetBaseType();

				for (int c = 0; c < cm; c++) {
					for (int r = 0; r < rm; r++) {
						switch (baseType) {
						case ShaderVariable::ValueType::Float1: {
							float valFloat = plData->Owner->PipelineItem_GetVariableValueFloat(plData->Type, plData->PluginData, i, c, r);
							var->SetFloat(valFloat, c, r);
						} break;
						case ShaderVariable::ValueType::Integer1: {
							int valInteger = plData->Owner->PipelineItem_GetVariableValueInteger(plData->Type, plData->PluginData, i, c);
							var->SetIntegerValue(valInteger, c);
						} break;
						case ShaderVariable::ValueType::Boolean1: {
							bool valBoolean = plData->Owner->PipelineItem_GetVariableValueBoolean(plData->Type, plData->PluginData, i, c);
							var->SetBooleanValue(valBoolean, c);
						} break;
						}
					}
				}
			}

			requiresVarCleanup = true;
		}

		const auto& itemVars = m_renderer->GetItemVariableValues();
		const std::vector<GLuint>& srvs = m_objs->GetBindList(owner);
		const std::vector<GLuint>& ubos = m_objs->GetUniformBindList(owner);

		if (px)
			SystemVariableManager::Instance().SetViewportSize(px->RenderTextureSize.x, px->RenderTextureSize.y);

		// update variables
		// update system variables
		if (item->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(item->Data);

			if (geoData->Type == pipe::GeometryItem::Rectangle) {
				glm::vec3 scaleRect(geoData->Scale.x * SystemVariableManager::Instance().GetViewportSize().x, geoData->Scale.y * SystemVariableManager::Instance().GetViewportSize().y, 1.0f);
				glm::vec3 posRect((geoData->Position.x + 0.5f) * m_renderer->GetLastRenderSize().x, (geoData->Position.y + 0.5f) * m_renderer->GetLastRenderSize().y, -1000.0f);
				SystemVariableManager::Instance().SetGeometryTransform(item, scaleRect, geoData->Rotation, posRect);
			} else
				SystemVariableManager::Instance().SetGeometryTransform(item, geoData->Scale, geoData->Rotation, geoData->Position);

			SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
		} else if (item->Type == PipelineItem::ItemType::Model) {
			pipe::Model* objData = reinterpret_cast<pipe::Model*>(item->Data);

			SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
			SystemVariableManager::Instance().SetGeometryTransform(item, objData->Scale, objData->Rotation, objData->Position);
		} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
			pipe::VertexBuffer* vbData = reinterpret_cast<pipe::VertexBuffer*>(item->Data);

			SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
			SystemVariableManager::Instance().SetGeometryTransform(item, vbData->Scale, vbData->Rotation, vbData->Position);
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = reinterpret_cast<pipe::PluginItemData*>(item->Data);
			
			float scale[3], pos[3], rota[3];
			plData->Owner->PipelineItem_GetTransform(plData->Type, plData->PluginData, pos, scale, rota);

			SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
			SystemVariableManager::Instance().SetGeometryTransform(item, glm::make_vec3(scale), glm::make_vec3(rota), glm::make_vec3(pos));
		}

		// copy variable values
		for (auto& var : vars) {
			std::string vname(var->Name); // example: "var.lights[0].diffuse", "lights[0].diffuse", etc...
			
			size_t curloc = 0;
			size_t seploc = vname.find_first_of(".[");

			std::string tokenName = "";
			if (seploc == std::string::npos) tokenName = vname;
			else tokenName = vname.substr(0, seploc);

			spvm_result_t result = spvm_state_get_result(m_vm, (const spvm_string)tokenName.c_str());
			
			spvm_member_t pointer = nullptr;
			spvm_result_t pointerType = nullptr;
			spvm_word pointerMCount = 0;

			if (result) {
				pointer = result->members;
				pointerMCount = result->member_count;
				
				if (result->pointer)
					pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[result->pointer]);
			}
			else { // search in the nameless buffers
				bool foundBufferVariable = false;
				for (spvm_word i = 0; i < m_shader->bound; i++) {
					if (m_vm->results[i].name && strcmp(m_vm->results[i].name, "") == 0 && m_vm->results[i].type == spvm_result_type_variable) {
						spvm_result_t anonBuffer = &m_vm->results[i];
						spvm_result_t anonBufferType = spvm_state_get_type_info(m_vm->results, &m_vm->results[anonBuffer->pointer]);
						
						
						for (spvm_word j = 0; j < anonBufferType->member_name_count; j++)
							if (strcmp(anonBufferType->member_name[j], tokenName.c_str()) == 0) {
								pointer = anonBuffer->members[j].members;
								pointerMCount = anonBuffer->members[j].member_count;

								if (anonBuffer->members[j].type)
									pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[anonBuffer->members[j].type]);
								
								foundBufferVariable = true;

								break;
							}
					}

					if (foundBufferVariable) break;
				}

			}

			if (!pointer)
				continue;

			curloc = seploc == std::string::npos ? std::string::npos : seploc+1;
			if (seploc != std::string::npos)
				seploc = vname.find_first_of(".[", curloc+1);

			while (seploc != std::string::npos) {
				tokenName = vname.substr(curloc, seploc-curloc);

				// index
				if (tokenName.size() > 0 && isdigit(tokenName[0])) {
					tokenName = tokenName.substr(0, tokenName.size() - 1);

					bool allDigits = true;
					for (int i = 0; i < tokenName.size(); i++)
						if (!isdigit(tokenName[i])) {
							allDigits = false;
							break;
						}

					int index = 0;
					if (allDigits && !tokenName.empty())
						index = std::stoi(tokenName);

					if (index < pointerMCount) {
						spvm_member_t mem = pointer + index;
						
						pointerMCount = mem->member_count;
						if (mem->type)
							pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
						pointer = mem->members;
					}
				} 
				// member name
				else if (pointerType) {
					for (int i = 0; i < pointerType->member_name_count; i++) {
						if (strcmp(pointerType->member_name[i], tokenName.c_str()) == 0) {
							spvm_member_t mem = pointer + i;
							
							pointerMCount = mem->member_count;
							if (mem->type)
								pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
							pointer = mem->members;
						}
					}
				}

				curloc = seploc+1;
				seploc = vname.find_first_of(".[", curloc + 1);
			}

			// leftover
			if (curloc < vname.size() && pointerType) {
				tokenName = vname.substr(curloc);

				// index
				if (tokenName.size() > 0 && isdigit(tokenName[0])) {
					tokenName = tokenName.substr(0, tokenName.size() - 1);

					bool allDigits = true;
					for (int i = 0; i < tokenName.size(); i++)
						if (!isdigit(tokenName[i])) {
							allDigits = false;
							break;
						}

					int index = 0;
					if (allDigits && !tokenName.empty())
						index = std::stoi(tokenName);

					if (index < pointerMCount) {
						spvm_member_t mem = pointer + index;

						pointerMCount = mem->member_count;
						if (mem->type)
							pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
						pointer = mem->members;
					}
				} 
				// member name
				else {
					for (int i = 0; i < pointerType->member_name_count; i++) {
						if (strcmp(pointerType->member_name[i], tokenName.c_str()) == 0) {
							spvm_member_t mem = pointer + i;

							pointerMCount = mem->member_count;
							if (mem->type)
								pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
							pointer = mem->members;
						}
					}
				}
			}

			// variable type
			bool useFloat = false, useInt = false, useBool = false;
			switch (var->GetType()) {
			case ShaderVariable::ValueType::Boolean1:
			case ShaderVariable::ValueType::Boolean2:
			case ShaderVariable::ValueType::Boolean3:
			case ShaderVariable::ValueType::Boolean4: 
				useBool = true;
				break;

			case ShaderVariable::ValueType::Integer1:
			case ShaderVariable::ValueType::Integer2:
			case ShaderVariable::ValueType::Integer3:
			case ShaderVariable::ValueType::Integer4: 
				useInt = true;
				break;

			case ShaderVariable::ValueType::Float1:
			case ShaderVariable::ValueType::Float2:
			case ShaderVariable::ValueType::Float3:
			case ShaderVariable::ValueType::Float4:
			case ShaderVariable::ValueType::Float2x2:
			case ShaderVariable::ValueType::Float3x3:
			case ShaderVariable::ValueType::Float4x4:
				useFloat = true;
				break;
			}

			// update system variable value
			SystemVariableManager::Instance().Update(var, item);

			// item variable value
			ShaderVariable* actualVar = var;
			for (const auto& iVar : itemVars) {
				if (iVar.Item == item && iVar.Variable == var) {
					actualVar = iVar.NewValue;
					break;
				}
			}

			// copy the value now that we have a pointer to actual memory part
			int cCount = std::min<int>(actualVar->GetColumnCount(), pointerMCount);
			for (int c = 0; c < cCount; c++) {
				if (pointer[c].member_count == 0) {
					if (useFloat)
						pointer[c].value.f = actualVar->AsFloat(c);
					else if (useInt)
						pointer[c].value.s = actualVar->AsInteger(c);
					else if (useBool)
						pointer[c].value.b = actualVar->AsBoolean(c);
				} else {
					for (int r = 0; r < pointer[c].member_count; r++)
						pointer[c].members[r].value.f = actualVar->AsFloat(r, c);
				}
			}
		}

		// textures, buffers [TODO], etc...
		int sampler2Dloc = 0;
		for (spvm_word i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointerInfo = nullptr;
			if (slot->pointer)
				pointerInfo = &m_vm->results[slot->pointer];

			if (pointerInfo && pointerInfo->value_type == spvm_value_type_pointer) {
				spvm_result_t type_info = spvm_state_get_type_info(m_vm->results, pointerInfo);
				bool isBufferBlock = false;
				
				for (int i = 0; i < type_info->decoration_count; i++)
					if (type_info->decorations[i].type == SpvDecorationBufferBlock) {
						isBufferBlock = true;
						break;
					}

				if (pointerInfo->storage_class == SpvStorageClassUniformConstant && !isBufferBlock) {
					// textures
					if (type_info->value_type == spvm_value_type_sampled_image || type_info->value_type == spvm_value_type_image) {
						for (int j = 0; j < slot->decoration_count; j++) {
							if (slot->decorations[j].type == SpvDecorationLocation) {
								sampler2Dloc = slot->decorations[j].literal1;
								break;
							}
						}

						if (sampler2Dloc >= srvs.size() && !pluginUsesCustomTextures) {
							spvm_image_t img = (spvm_image_t)malloc(sizeof(spvm_image));

							// get texture size
							glm::ivec2 size(1, 1);
							float* imgData = (float*)calloc(1, sizeof(float) * size.x * size.y * 4);

							img->user_data = nullptr;

							spvm_image_create(img, imgData, size.x, size.y, 1);
							free(imgData);

							slot->members[0].image_data = img;

							m_images.push_back(img);

							sampler2Dloc++;
						} 
						else {
							spvm_image_t img = (spvm_image_t)malloc(sizeof(spvm_image));
							
							if (type_info->image_info == NULL)
								type_info = &m_vm->results[type_info->pointer];
							
							glm::ivec3 imgSize(1, 1, 1);
							float* imgData = nullptr;
							GLuint pluginCustomTexture = 0;
							if (pluginUsesCustomTextures) {
								// cubemaps
								if (type_info->image_info->dim == SpvDimCube) {
									pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

									pluginCustomTexture = plData->Owner->PipelineItem_DebugGetTexture(plData->Type, plData->PluginData, sampler2Dloc, slot->name);
									plData->Owner->PipelineItem_DebugGetTextureSize(plData->Type, plData->PluginData, sampler2Dloc, slot->name, imgSize.x, imgSize.y, imgSize.z);
									imgSize.z = 6;

									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * 6 * 4); // 6 faces

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_CUBE_MAP, pluginCustomTexture);
									for (int i = 0; i < 6; i++)
										glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, imgData + imgSize.x * imgSize.y * 4 * i);
									glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
								}
								// 3d textures
								else if (type_info->image_info->dim == SpvDim3D) {
									pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

									pluginCustomTexture = plData->Owner->PipelineItem_DebugGetTexture(plData->Type, plData->PluginData, sampler2Dloc, slot->name);
									plData->Owner->PipelineItem_DebugGetTextureSize(plData->Type, plData->PluginData, sampler2Dloc, slot->name, imgSize.x, imgSize.y, imgSize.z);
									
									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * imgSize.z * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_3D, pluginCustomTexture);
									glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_3D, 0);
								}
								// 2d textures
								else {
									pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

									pluginCustomTexture = plData->Owner->PipelineItem_DebugGetTexture(plData->Type, plData->PluginData, sampler2Dloc, slot->name);
									plData->Owner->PipelineItem_DebugGetTextureSize(plData->Type, plData->PluginData, sampler2Dloc, slot->name, imgSize.x, imgSize.y, imgSize.z);
									imgSize.z = 1;

									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_2D, pluginCustomTexture);
									glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_2D, 0);
								}
							} else {
								// cubemaps
								if (type_info->image_info->dim == SpvDimCube) {
									std::string itemName = m_objs->GetItemNameByTextureID(srvs[sampler2Dloc]);
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
									imgSize.x = size.x;
									imgSize.y = size.y;
									imgSize.z = 6;

									imgData = (float*)malloc(sizeof(float) * size.x * size.y * 6 * 4); // 6 faces

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[sampler2Dloc]);
									for (int i = 0; i < 6; i++)
										glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, imgData + size.x * size.y * 4 * i);
									glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
								}
								// 3d textures
								else if (type_info->image_info->dim == SpvDim3D) {
									// get texture size
									std::string itemName = m_objs->GetItemNameByTextureID(srvs[sampler2Dloc]);
									ObjectManagerItem* itemData = m_objs->GetObjectManagerItem(itemName);
									
									if (itemData != nullptr && itemData->Image3D != nullptr)
										imgSize = itemData->Image3D->Size;

									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * imgSize.z * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_3D, srvs[sampler2Dloc]);
									glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_3D, 0);
								}
								// 2d textures
								else {
									// get texture size
									std::string itemName = m_objs->GetItemNameByTextureID(srvs[sampler2Dloc]);
									ObjectManagerItem* itemData = m_objs->GetObjectManagerItem(itemName);
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
									imgSize.x = size.x;
									imgSize.y = size.y;

									imgData = (float*)malloc(sizeof(float) * size.x * size.y * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_2D, srvs[sampler2Dloc]);
									glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_2D, 0);
								}
							}

							if (imgData != nullptr) {
								spvm_image_create(img, imgData, imgSize.x, imgSize.y, imgSize.z);
								free(imgData);
							}

							if (pluginUsesCustomTextures)
								img->user_data = (void*)pluginCustomTexture;
							else
								img->user_data = (void*)srvs[sampler2Dloc];

							slot->members[0].image_data = img;
							m_images.push_back(img);
							sampler2Dloc++;
						}
					}
				} 
				else if (pointerInfo->storage_class == SpvStorageClassStorageBuffer || isBufferBlock) {
					int binding = 0;
					for (int i = 0; i < slot->decoration_count; i++) {
						if (slot->decorations[i].type == SpvDecorationBinding) {
							binding = slot->decorations[i].literal1;
							break;
						}
					}

					if (binding < ubos.size()) {
						ed::BufferObject* obj = m_objs->GetBuffer(m_objs->GetBufferNameByID(ubos[binding]));

						float* data = (float*)calloc(1, obj->Size);
						glBindBuffer(GL_SHADER_STORAGE_BUFFER, obj->ID);
						glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, obj->Size, data);
						glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

						spvm_word offset = 0;
						for (spvm_word j = 0; j < slot->member_count; j++) {
							spvm_result_t arrayType = spvm_state_get_type_info(m_vm->results, &m_vm->results[slot->members[j].type]);
							spvm_result_t elementType = spvm_state_get_type_info(m_vm->results, &m_vm->results[arrayType->pointer]);
							
							spvm_word bufferElementMCount = slot->members[j].member_count;
							spvm_member_t bufferElement = &slot->members[j];

							if (arrayType->value_type == spvm_value_type_runtime_array) {
								int elCount = obj->Size / 4 - offset;
								int elSize = spvm_result_calculate_size(m_vm->results, arrayType->pointer);
							
								int memCount = elCount / elSize;
								bufferElement->member_count = memCount;
								bufferElement->members = (spvm_member*)calloc(memCount, sizeof(spvm_member));
								for (int k = 0; k < memCount; k++)
									spvm_member_allocate_typed_value(&bufferElement->members[k], m_vm->results, arrayType->pointer);
							}

							spvm_member_recursive_fill(m_vm->results, data, slot->members[j].members, slot->members[j].member_count, arrayType->pointer, &offset);
						}

						free(data);
					}
					// create 1 element sized empty buffer
					else {
						for (spvm_word j = 0; j < slot->member_count; j++) {
							spvm_result_t arrayType = spvm_state_get_type_info(m_vm->results, &m_vm->results[slot->members[j].type]);
							spvm_result_t elementType = spvm_state_get_type_info(m_vm->results, &m_vm->results[arrayType->pointer]);

							spvm_word bufferElementMCount = slot->members[j].member_count;
							spvm_member_t bufferElement = &slot->members[j];

							if (arrayType->value_type == spvm_value_type_runtime_array) {
								bufferElement->member_count = 1;
								bufferElement->members = (spvm_member*)calloc(1, sizeof(spvm_member));
								spvm_member_allocate_typed_value(&bufferElement->members[0], m_vm->results, arrayType->pointer);
							}
						}
					}
				}
			}
		}

		if (requiresVarCleanup) {
			for (int i = 0; i < vars.size(); i++)
				delete vars[i];
			vars.clear();
		}
	}
	
	spvm_member_t DebugInformation::GetVariable(const std::string& vname, size_t& outCount, spvm_result_t& outType)
	{
		return GetVariableFromState(m_vm, vname, outCount, outType);
	}
	spvm_member_t DebugInformation::GetVariable(const std::string& str, size_t& count)
	{
		return GetVariableFromState(m_vm, str, count);
	}
	spvm_member_t DebugInformation::GetVariableFromState(spvm_state_t state, const std::string& vname, size_t& outCount, spvm_result_t& outType)
	{
		spvm_word mem_index = 0, mem_count = 0;
		spvm_result_t val = spvm_state_get_local_result(state, state->current_function, (spvm_string)vname.c_str());
		if (val == nullptr)
			val = spvm_state_get_result_with_value(state, (spvm_string)vname.c_str());

		if (val == nullptr) { // anon buffer object uniforms
			val = spvm_state_get_result(state, "");

			bool foundBufferVariable = false;
			for (spvm_word i = 0; i < m_shader->bound; i++) {
				if (state->results[i].name) {
					if (strcmp(state->results[i].name, "") == 0 && state->results[i].type == spvm_result_type_variable) {
						val = &state->results[i];
						spvm_result_t buffer_info = spvm_state_get_type_info(state->results, &state->results[val->pointer]);
						for (spvm_word i = 0; i < buffer_info->member_name_count; i++) {
							if (strcmp(vname.c_str(), buffer_info->member_name[i]) == 0) {
								mem_index = i;
								mem_count = 1;

								outType = spvm_state_get_type_info(state->results, &state->results[val->members[mem_index].type]);

								foundBufferVariable = true;
								break;
							}
						}
					}
				}

				if (foundBufferVariable) break;
			}
		} else {
			mem_index = 0;
			mem_count = val->member_count;
		}

		if (val && val->members != nullptr && mem_count != 0) {
			/*if (mem_count == 1 && val->members[mem_index].member_count != 0) {
				outType = spvm_state_get_type_info(state->results, &state->results[val->members[mem_index].type]);
				outCount = val->members[mem_index].member_count;
				return &val->members[mem_index].members[0];
			}*/

			outType = spvm_state_get_type_info(state->results, &state->results[val->pointer]);
			outCount = mem_count;
			return &val->members[mem_index];
		}

		return nullptr;
	}
	spvm_member_t DebugInformation::GetVariableFromState(spvm_state_t state, const std::string& str, size_t& count)
	{
		spvm_result_t type;
		return GetVariableFromState(state, str, count, type);
	}
	void DebugInformation::GetVariableValueAsString(std::stringstream& outString, spvm_state_t state, spvm_result_t type, spvm_member_t mems, spvm_word mem_count, const std::string& prefix)
	{
		/*
			TODO: use BFS - might have more control over output + faster
			(was lazy, copied from github.com/dfranx/sdbg)
		*/
		
		if (!mems) return;

		bool outPrefix = false;
		bool allRec = true;
		for (spvm_word i = 0; i < mem_count; i++) {

			spvm_result_t vtype = type;
			if (mems[i].type != 0)
				vtype = spvm_state_get_type_info(state->results, &state->results[mems[i].type]);
			if (vtype->member_count > 1 && vtype->pointer != 0 && vtype->value_type != spvm_value_type_matrix && vtype->value_type != spvm_value_type_array)
				vtype = spvm_state_get_type_info(state->results, &state->results[vtype->pointer]);

			if (mems[i].member_count == 0) {
				if (!outPrefix && !prefix.empty()) {
					outPrefix = true;
					outString << prefix << " = ";
				}

				if (vtype->value_type == spvm_value_type_float && type->value_bitcount <= 32)
					outString << std::setw(6) << std::setprecision(3) << std::fixed << mems[i].value.f << " ";
				else if (vtype->value_type == spvm_value_type_float)
					outString << std::setw(6) << std::setprecision(3) << std::fixed << mems[i].value.d << " ";
				else
					outString << mems[i].value.s << " ";
				allRec = false;
			} else {
				if (type->value_type == spvm_value_type_runtime_array && i > 2 && i < mem_count - 3)
					continue;

				std::string newPrefix = prefix;
				if (type->value_type == spvm_value_type_struct)
					newPrefix += "." + std::string(type->member_name[i]);
				if (type->value_type == spvm_value_type_matrix || type->value_type == spvm_value_type_array || type->value_type == spvm_value_type_runtime_array)
					newPrefix += "[" + std::to_string(i) + "]";

				if (type->value_type == spvm_value_type_runtime_array && mem_count > 6 && i == mem_count - 3)
					outString << "...\n";

				GetVariableValueAsString(outString, state, vtype, mems[i].members, mems[i].member_count, newPrefix);
			}
		}
		if (!allRec) outString << "\n";
	}

	spvm_result_t DebugInformation::Immediate(const std::string& entry, spvm_result_t& outType)
	{
		if (m_vm == nullptr)
			return nullptr;

		bool usePlugin = false;
		ed::IPlugin2* plugin2 = nullptr;
		if (m_pixel->Pass->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)m_pixel->Pass->Data;
			if (plData->Owner->GetVersion() >= 2) {
				plugin2 = ((ed::IPlugin2*)plData->Owner);
				
				if (!plugin2->PipelineItem_SupportsImmediateMode(plData->Type, plData->PluginData, (ed::plugin::ShaderStage)m_stage))
					return nullptr;

				if (plugin2->PipelineItem_HasCustomImmediateModeCompiler(plData->Type, plData->PluginData, (ed::plugin::ShaderStage)m_stage)) {
					if (!plugin2->PipelineItem_ImmediateModeCompile(plData->Type, plData->PluginData, (ed::plugin::ShaderStage)m_stage, entry.c_str()))
						return nullptr;
					else
						usePlugin = true;
				}
			}
		}

#ifdef BUILD_IMMEDIATE_MODE // TODO
		int resultID = 0;

		std::vector<std::string> varList;
		if (!usePlugin) {
			std::string curFunction = "";
			if (m_vm != nullptr && m_vm->current_function != nullptr)
				curFunction = m_vm->current_function->name;

			// compile the expression
			resultID = m_compiler.Compile(entry, curFunction);
			m_compiler.GetSPIRV(m_spvImmediate);

			varList = m_compiler.GetVariableList();
		} else if (plugin2) {
			unsigned int spvSize = plugin2->ImmediateMode_GetSPIRVSize();
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plugin2->ImmediateMode_GetSPIRV();
				m_spvImmediate = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			resultID = plugin2->ImmediateMode_GetResultID();

			for (unsigned int i = 0u; i < plugin2->ImmediateMode_GetVariableCount(); i++)
				varList.push_back(plugin2->ImmediateMode_GetVariableName(i));
		}

		// error occured
		if (resultID <= 0)
			return nullptr;

		// delete old program & state
		if (m_vmImmediate) {
			spvm_state_delete(m_vmImmediate);
			m_vmImmediate = nullptr;
		}
		if (m_shaderImmediate) {
			spvm_program_delete(m_shaderImmediate);
			m_shaderImmediate = nullptr;
		}

		// create program & state
		m_shaderImmediate = spvm_program_create(m_vmContext, (spvm_source)m_spvImmediate.data(), m_spvImmediate.size());
		m_vmImmediate = _spvm_state_create_base(m_shaderImmediate, m_stage == ShaderStage::Pixel, 0);
		
		// can't use set_extenstion() function because for some reason two GLSL.std.450 instructions are generated with spvgentwo
		for (int i = 0; i < m_shaderImmediate->bound; i++)
			if (m_vmImmediate->results[i].name)
				if (strcmp(m_vmImmediate->results[i].name, "GLSL.std.450") == 0)
					m_vmImmediate->results[i].extension = m_vmGLSL;

		// copy variable values
		spvm_state_group_sync(m_vm);
		for (int i = 0; i < varList.size(); i++) {
			size_t varValueCount = 0;
			spvm_result_t varType = nullptr;
			spvm_member_t varValue = GetVariable(varList[i], varValueCount, varType);

			if (varValue == nullptr)
				continue;

			spvm_result_t pointerToVariable[4] = { nullptr };

			for (int j = 0; j < m_shaderImmediate->bound; j++) {
				if (m_vmImmediate->results[j].name == nullptr)
					continue;
				
				spvm_result_t res = &m_vmImmediate->results[j];
				spvm_result_t resType = spvm_state_get_type_info(m_vmImmediate->results, &m_vmImmediate->results[res->pointer]);

				// TODO: also check for the type, or there might be some crashes caused by two vars with different type (?) (mat4 and vec4 for example)
				
				if (res->member_count == varValueCount && resType->value_type == varType->value_type && res->members != nullptr && strcmp(varList[i].c_str(), res->name) == 0) {
					spvm_member_memcpy(res->members, varValue, varValueCount);

					pointerToVariable[0] = res;

					if (m_vmImmediate->derivative_used) {
						if (m_vmImmediate->derivative_group_x) {
							spvm_member_memcpy(m_vmImmediate->derivative_group_x->results[j].members, GetVariableFromState(m_vm->derivative_group_x, varList[i], varValueCount), varValueCount);
							pointerToVariable[1] = &m_vmImmediate->derivative_group_x->results[j];
						}
						if (m_vmImmediate->derivative_group_y) {
							spvm_member_memcpy(m_vmImmediate->derivative_group_y->results[j].members, GetVariableFromState(m_vm->derivative_group_y, varList[i], varValueCount), varValueCount);
							pointerToVariable[2] = &m_vmImmediate->derivative_group_y->results[j];
						}
						if (m_vmImmediate->derivative_group_d) {
							spvm_member_memcpy(m_vmImmediate->derivative_group_d->results[j].members, GetVariableFromState(m_vm->derivative_group_d, varList[i], varValueCount), varValueCount);
							pointerToVariable[3] = &m_vmImmediate->derivative_group_d->results[j];
						}
					}
				}
			}
			
			// function parameters (which are pointers)
			if (pointerToVariable[0] != nullptr) {
				for (int j = 0; j < m_shaderImmediate->bound; j++) {
					if (m_vmImmediate->results[j].name == nullptr)
						continue;

					spvm_result_t res = &m_vmImmediate->results[j];

					// TODO: also check for the type, or there might be some crashes caused by two vars with different type (?) (mat4 and vec4 for example)

					if (res->member_count == varValueCount && res->members == nullptr && strcmp(varList[i].c_str(), res->name) == 0) {
						res->members = pointerToVariable[0]->members;

						if (m_vmImmediate->derivative_used) {
							if (m_vmImmediate->derivative_group_x) m_vmImmediate->derivative_group_x->results[j].members = pointerToVariable[1]->members;
							if (m_vmImmediate->derivative_group_y) m_vmImmediate->derivative_group_y->results[j].members = pointerToVariable[2]->members;
							if (m_vmImmediate->derivative_group_d) m_vmImmediate->derivative_group_d->results[j].members = pointerToVariable[3]->members;
						}
					}
				}
			}
		}

		// execute $$_shadered_immediate
		spvm_word fnImmediate = spvm_state_get_result_location(m_vmImmediate, "$$_shadered_immediate");
		spvm_state_prepare(m_vmImmediate, fnImmediate);
		spvm_state_call_function(m_vmImmediate);

		// get type and return value
		spvm_result_t val = &m_vmImmediate->results[resultID];
		outType = spvm_state_get_type_info(m_vmImmediate->results, &m_vmImmediate->results[val->pointer]);
		return val;
#else
		return nullptr; // TODO: enable DebugInformation::Immediate() for macOS devices
#endif
	}

	void DebugInformation::PrepareVertexShader(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		m_stage = ShaderStage::Vertex;

		m_resetVM();
		if (owner->Type == PipelineItem::ItemType::ShaderPass)
			m_setupVM(((pipe::ShaderPass*)owner->Data)->VSSPV);
		else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;
			
			unsigned int spvSize = plData->Owner->PipelineItem_GetSPIRVSize(plData->Type, plData->PluginData, plugin::ShaderStage::Vertex);
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plData->Owner->PipelineItem_GetSPIRV(plData->Type, plData->PluginData, plugin::ShaderStage::Vertex);
				spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			m_setupVM(spv);
 		}

		// uniforms
		m_copyUniforms(owner, item, px);
	}
	void DebugInformation::SetVertexShaderInput(PipelineItem* pass, eng::Model::Mesh::Vertex vertex, int vertexID, int instanceID, ed::BufferObject* instanceBuffer)
	{
		// input variables
		for (spvm_word i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &m_vm->results[m_vm->results[i].pointer];

			// input variable
			if (pointer && pointer->storage_class == SpvStorageClassInput) {
				spvm_word location = 0, builtinType = 0;
				bool hasLocation = false, isBuiltin = false;
				for (spvm_word j = 0; j < slot->decoration_count; j++) {
					if (slot->decorations[j].type == SpvDecorationLocation) {
						location = slot->decorations[j].literal1;
						hasLocation = true;
					}
					else if (slot->decorations[j].type == SpvDecorationBuiltIn) {
						builtinType = slot->decorations[j].literal1;
						isBuiltin = true;
					}
				}

				if (hasLocation) {
					glm::vec4 value(0.0f);

					InputLayoutValue inputType = InputLayoutValue::MaxCount;

					if (pass->Type == PipelineItem::ItemType::ShaderPass) {
						pipe::ShaderPass* passData = (pipe::ShaderPass*)pass->Data;
						if (location < passData->InputLayout.size())
							inputType = passData->InputLayout[location].Value;
						else if (instanceBuffer != nullptr) {
							int bufferLocation = location - passData->InputLayout.size();

							std::vector<ShaderVariable::ValueType> tData = m_objs->ParseBufferFormat(instanceBuffer->ViewFormat);

							int stride = 0;
							for (const auto& dataEl : tData)
								stride += ShaderVariable::GetSize(dataEl, true);

							GLfloat* bufPtr = (GLfloat*)malloc(stride);

							glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer->ID);
							glGetBufferSubData(GL_ARRAY_BUFFER, instanceID * stride, stride, &bufPtr[0]);
							glBindBuffer(GL_ARRAY_BUFFER, 0);

							int iOffset = 0;
							for (int j = 0; j < tData.size(); j++) {
								int elCount = ShaderVariable::GetSize(tData[j]) / 4;

								if (j == bufferLocation) {
									if (elCount == 4)
										value = glm::make_vec4(bufPtr + iOffset);
									else if (elCount == 3)
										value = glm::vec4(glm::make_vec3(bufPtr + iOffset), 0.0f);
									else if (elCount == 2)
										value = glm::vec4(glm::make_vec2(bufPtr + iOffset), 0.0f, 0.0f);
									else
										value = glm::vec4(*(bufPtr + iOffset), 0.0f, 0.0f, 0.0f);
									break;
								}

								iOffset += elCount;
							}

							free(bufPtr);
						}
					} else if (pass->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* plData = (pipe::PluginItemData*)pass->Data;
						if (location < plData->Owner->PipelineItem_GetInputLayoutSize(plData->Type, plData->PluginData)) {
							plugin::InputLayoutItem inputItem;
							plData->Owner->PipelineItem_GetInputLayoutItem(plData->Type, plData->PluginData, location, inputItem);

							inputType = (InputLayoutValue)inputItem.Value;
						}
					}

					switch (inputType) {
					case InputLayoutValue::Position: value = glm::vec4(vertex.Position, 0.0f); break;
					case InputLayoutValue::Normal: value = glm::vec4(vertex.Normal, 0.0f); break;
					case InputLayoutValue::Texcoord: value = glm::vec4(vertex.TexCoords, 0.0f, 0.0f); break;
					case InputLayoutValue::Tangent: value = glm::vec4(vertex.Tangent, 0.0f); break;
					case InputLayoutValue::Binormal: value = glm::vec4(vertex.Binormal, 0.0f); break;
					case InputLayoutValue::Color: value = glm::vec4(vertex.Color); break;
					default: break;
					}

					for (spvm_word j = 0; j < slot->member_count; j++)
						slot->members[j].value.f = value[j];
				} else if (isBuiltin) {
					if (builtinType == SpvBuiltInVertexId)
						slot->members[0].value.s = vertexID;
					else if (builtinType == SpvBuiltInInstanceId)
						slot->members[0].value.s = instanceID;
				}
			}
		}

	}
	glm::vec4 DebugInformation::ExecuteVertexShader()
	{
		if (m_vm == nullptr)
			return glm::vec4(0.0f);

		spvm_word fnMain = spvm_state_get_result_location(m_vm, "main");
		if (fnMain == 0)
			return glm::vec4(0.0f);

		spvm_state_prepare(m_vm, fnMain);
		spvm_state_call_function(m_vm);

		glm::vec4 ret(0.0f);

		spvm_word mCount = 0;
		spvm_member_t pointer = nullptr;

		spvm_word mem_count = 0;
		spvm_member_t glPosition = spvm_state_get_builtin(m_vm, SpvBuiltInPosition, &mem_count);

		if (glPosition != nullptr) {
			for (int j = 0; j < mem_count; j++)
				ret[j] = glPosition[j].value.f;
		}

		return ret;
	}
	void DebugInformation::CopyVertexShaderOutput(PixelInformation& px, int vertexIndex)
	{
		px.VertexShaderOutput[vertexIndex].clear();
		for (int i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &m_vm->results[m_vm->results[i].pointer];

			// output variable
			if (pointer && pointer->storage_class == SpvStorageClassOutput) {
				int loc = -1;
				for (int j = 0; j < slot->decoration_count; j++)
					if (slot->decorations[j].type == SpvDecorationLocation) {
						loc = slot->decorations[j].literal1;
						break;
					}

				struct spvm_result copy;
				memcpy(&copy, slot, sizeof(struct spvm_result));
				copy.decorations = nullptr;
				copy.decoration_count = 0;
				copy.members = nullptr;

				if (slot->name) { // TODO: mem leak
					copy.name = (spvm_string)calloc(1, strlen(slot->name) + 1);
					strcpy(copy.name, slot->name);
				}

				// hax: store location in spvm_result::return_type -> this can be done in a better way, but im lazy right now [TODO]
				copy.return_type = loc;

				spvm_result_allocate_typed_value(&copy, m_vm->results, copy.pointer);
				spvm_member_memcpy(copy.members, slot->members, copy.member_count);

				px.VertexShaderOutput[vertexIndex].push_back(copy);
			}
		}
	}

	glm::vec3 DebugInformation::m_getWeights(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 p)
	{
		glm::vec2 ab = b - a;
		glm::vec2 ac = c - a;
		glm::vec2 ap = p - a;
		float factor = 1 / (ab.x * ac.y - ab.y * ac.x);
		float s = (ac.y * ap.x - ac.x * ap.y) * factor;
		float t = (ab.x * ap.y - ab.y * ap.x) * factor;
		return glm::vec3(1 - s - t, s, t);
	}
	void DebugInformation::PreparePixelShader(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		m_stage = ShaderStage::Pixel;

		m_resetVM();
		if (owner->Type == PipelineItem::ItemType::ShaderPass)
			m_setupVM(((pipe::ShaderPass*)owner->Data)->PSSPV);
		else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

			unsigned int spvSize = plData->Owner->PipelineItem_GetSPIRVSize(plData->Type, plData->PluginData, plugin::ShaderStage::Pixel);
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plData->Owner->PipelineItem_GetSPIRV(plData->Type, plData->PluginData, plugin::ShaderStage::Pixel);
				spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			m_setupVM(spv);
		}

		// uniforms
		m_copyUniforms(owner, item, px);

		// dfdx/y
		if (m_vm->derivative_used) {
			for (spvm_word i = 0; i < m_vm->owner->bound; i++) {
				spvm_result_t slot = &m_vm->results[i];

				spvm_result_t ptrInfo = nullptr;
				if (slot->pointer)
					ptrInfo = &m_vm->results[slot->pointer];

				bool needsCopy = false;

				if (ptrInfo)
					needsCopy = (ptrInfo->storage_class == SpvStorageClassUniform || ptrInfo->storage_class == SpvStorageClassUniformConstant) && ptrInfo->value_type == spvm_value_type_pointer;

				if (needsCopy && slot->members != nullptr) {
					if (m_vm->derivative_group_x) spvm_member_memcpy(m_vm->derivative_group_x->results[i].members, slot->members, slot->member_count);
					if (m_vm->derivative_group_y) spvm_member_memcpy(m_vm->derivative_group_y->results[i].members, slot->members, slot->member_count);
					if (m_vm->derivative_group_d) spvm_member_memcpy(m_vm->derivative_group_d->results[i].members, slot->members, slot->member_count);
				}
			}
		}
	}
	void DebugInformation::SetPixelShaderInput(PixelInformation& pixel)
	{
		m_pixel = &pixel;

		glm::vec3 weights = m_processWeight(glm::ivec2(0, 0));
		m_interpolateValues(m_vm, weights);

		if (m_vm->derivative_used && !m_vm->_derivative_is_group_member) {
			spvm_byte isOddX = ((int)pixel.Coordinate.x) % 2 != 0;
			spvm_byte isOddY = ((int)pixel.Coordinate.y) % 2 != 0;
			float modX = 1, modY = 1;

			// setup frag_coord
			if (isOddX) modX = -1;
			if (isOddY) modY = -1;
			
			if (m_vm->derivative_group_x) {
				weights = m_processWeight(glm::ivec2(modX, 0));
				m_interpolateValues(m_vm->derivative_group_x, weights);
			}
			if (m_vm->derivative_group_y) {
				weights = m_processWeight(glm::ivec2(0, modY));
				m_interpolateValues(m_vm->derivative_group_y, weights);
			}
			if (m_vm->derivative_group_d) {
				weights = m_processWeight(glm::ivec2(modX, modY));
				m_interpolateValues(m_vm->derivative_group_d, weights);
			}
		}
	}
	glm::vec3 DebugInformation::m_processWeight(glm::ivec2 offset)
	{
		// !!! m_pixel must be set !!!

		glm::vec2 pxPosition = glm::vec2(m_pixel->Coordinate + offset) / glm::vec2(m_pixel->RenderTextureSize - 1);

		// weigths
		glm::vec2 scrnPos1 = m_getScreenCoord(m_pixel->glPosition[0]);
		glm::vec2 scrnPos2 = m_getScreenCoord(m_pixel->glPosition[1]);
		glm::vec2 scrnPos3 = m_getScreenCoord(m_pixel->glPosition[2]);
		glm::vec3 weights = m_getWeights(scrnPos1, scrnPos2, scrnPos3, pxPosition);
		weights *= glm::vec3(m_pixel->glPosition[0].w == 0.0f ? 0.0f : (1.0f / m_pixel->glPosition[0].w), m_pixel->glPosition[1].w == 0.0f ? 0.0f : (1.0f / m_pixel->glPosition[1].w), m_pixel->glPosition[2].w == 0.0f ? 0.0f : (1.0f / m_pixel->glPosition[2].w));
	
		return weights;
	}
	void DebugInformation::m_interpolateValues(spvm_state_t state, glm::vec3 weights)
	{
		// !!! m_pixel must be set !!!

		float weightSum = weights.x + weights.y + weights.z;

		// match the ps input with vs output
		for (int i = 0; i < state->owner->bound; i++) {
			spvm_result_t slot = &state->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &state->results[state->results[i].pointer];

			// input variable
			if (pointer && pointer->storage_class == SpvStorageClassInput) {
				int loc = -1, outputIndex = -1;

				// get location
				for (int j = 0; j < slot->decoration_count; j++)
					if (slot->decorations[j].type == SpvDecorationLocation) {
						loc = slot->decorations[j].literal1;
						break;
					}

				// get vs output index
				for (int j = 0; j < m_pixel->VertexShaderOutput[0].size(); j++) {
					const struct spvm_result* vsOutput = &m_pixel->VertexShaderOutput[0][j];
					if (vsOutput->return_type == loc) {
						if (loc == -1) {
							if (vsOutput->name && slot->name)
								if (strcmp(vsOutput->name, slot->name) == 0) {
									outputIndex = j;
									break;
								}
						} else {
							outputIndex = j;
							break;
						}
					}
				}

				// copy and interpolate values
				if (outputIndex >= 0) {
					const struct spvm_result* value0 = m_pixel->VertexShaderOutput[0].empty() ? nullptr : &m_pixel->VertexShaderOutput[0][outputIndex];
					const struct spvm_result* value1 = m_pixel->VertexShaderOutput[1].empty() ? nullptr : &m_pixel->VertexShaderOutput[1][outputIndex];
					const struct spvm_result* value2 = m_pixel->VertexShaderOutput[2].empty() ? nullptr : &m_pixel->VertexShaderOutput[2][outputIndex];

					// get type
					spvm_result_t memType = spvm_state_get_type_info(state->results, pointer);
					spvm_value_type elType = (spvm_value_type)memType->value_type;
					spvm_word vbcount = memType->value_bitcount;
					if (elType == spvm_value_type_vector || elType == spvm_value_type_matrix) {
						memType = spvm_state_get_type_info(state->results, &state->results[memType->pointer]);
						elType = (spvm_value_type)memType->value_type;
						vbcount = memType->value_bitcount;
					}

					// apply values
					for (int c = 0; c < slot->member_count; c++) {
						if (slot->members[c].member_count == 0) {
							if (elType == spvm_value_type_float && vbcount > 32)
								slot->members[c].value.d = (GET_VALUE_WITH_CHECK_DOUBLE(value0, c) * weights.x + GET_VALUE_WITH_CHECK_DOUBLE(value1, c) * weights.y + GET_VALUE_WITH_CHECK_DOUBLE(value2, c) * weights.z) / weightSum;
							else if (elType == spvm_value_type_float)
								slot->members[c].value.f = (GET_VALUE_WITH_CHECK_FLOAT(value0, c) * weights.x + GET_VALUE_WITH_CHECK_FLOAT(value1, c) * weights.y + GET_VALUE_WITH_CHECK_FLOAT(value2, c) * weights.z) / weightSum;
							else
								slot->members[c].value.s = (GET_VALUE_WITH_CHECK_INT(value0, c) * weights.x + GET_VALUE_WITH_CHECK_INT(value1, c) * weights.y + GET_VALUE_WITH_CHECK_INT(value2, c) * weights.z) / weightSum;
						} else {
							for (int r = 0; r < slot->members[c].member_count; c++) {
								if (elType == spvm_value_type_float && vbcount > 32)
									slot->members[c].members[r].value.d = (GET_VALUE2_WITH_CHECK_DOUBLE(value0, c, r) * weights.x + GET_VALUE2_WITH_CHECK_DOUBLE(value1, c, r) * weights.y + GET_VALUE2_WITH_CHECK_DOUBLE(value2, c, r) * weights.z) / weightSum;
								else if (elType == spvm_value_type_float)
									slot->members[c].members[r].value.f = (GET_VALUE2_WITH_CHECK_FLOAT(value0, c, r) * weights.x + GET_VALUE2_WITH_CHECK_FLOAT(value1, c, r) * weights.y + GET_VALUE2_WITH_CHECK_FLOAT(value2, c, r) * weights.z) / weightSum;
								else
									slot->members[c].members[r].value.s = (GET_VALUE2_WITH_CHECK_INT(value0, c, r) * weights.x + GET_VALUE2_WITH_CHECK_INT(value1, c, r) * weights.y + GET_VALUE2_WITH_CHECK_INT(value2, c, r) * weights.z) / weightSum;
							}
						}
					}
				}
			}
		}

	}
	glm::vec4 DebugInformation::ExecutePixelShader(int x, int y, int loc)
	{
		if (m_vm == nullptr)
			return glm::vec4(0.0f);

		spvm_word fnMain = spvm_state_get_result_location(m_vm, "main");
		if (fnMain == 0)
			return glm::vec4(0.0f);

		spvm_state_prepare(m_vm, fnMain);
		spvm_state_set_frag_coord(m_vm, x + 0.5f, y + 0.5f, 1.0f, 1.0f); // TODO: z and w components
		spvm_state_call_function(m_vm);

		glm::vec4 ret(0.0f);

		spvm_word mCount = 0;
		spvm_member_t pointer = nullptr;

		for (spvm_word i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t type = nullptr, pointerType = nullptr;
			if (slot->pointer) {
				type = spvm_state_get_type_info(m_vm->results, &m_vm->results[slot->pointer]);
				pointerType = &m_vm->results[slot->pointer];
			}

			if (slot->member_count == 0 || pointerType == nullptr || pointerType->storage_class != SpvStorageClassOutput)
				continue;

			int decLoc = -1;
			for (spvm_word j = 0; j < slot->decoration_count; j++) {
				if (slot->decorations[j].type == SpvDecorationLocation) {
					decLoc = slot->decorations[j].literal1;
					break;
				}
			}

			if (decLoc != loc && decLoc != -1)
				continue;

			for (int j = 0; j < slot->member_count; j++)
				ret[j] = slot->members[j].value.f;

			break;
		}

		return glm::clamp(ret, 0.0f, 1.0f);
	}

	void DebugInformation::PrepareDebugger()
	{
		spvm_word fnMain = spvm_state_get_result_location(m_vm, "main");
		if (fnMain == 0)
			return;

		m_funcStackLines.clear();
		m_funcStackLines.push_back(-1);

		spvm_state_prepare(m_vm, fnMain);

		if (m_stage == ShaderStage::Pixel && m_pixel != nullptr)
			spvm_state_set_frag_coord(m_vm, m_pixel->Coordinate.x + 0.5f, m_pixel->Coordinate.y + 0.5f, 1.0f, 1.0f); // TODO: z and w components

		// move to cursor to first line in the function
		spvm_state_step_into(m_vm);
		if (m_vm->owner->language == SpvSourceLanguageHLSL) {
			// since actual main is encapsulated into another main function
			while (m_vm->code_current && m_vm->function_stack_current == 0)
				spvm_state_step_into(m_vm);
		}

		m_funcStackLines[0] = m_vm->current_line;

#ifdef BUILD_IMMEDIATE_MODE
		// prepare immediate mode compiler
		m_compiler.SetSPIRV(m_spv);
#endif
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
		char* data = (char*)calloc(512, sizeof(char));
		strcpy(data, expr.c_str());

		m_watchExprs.push_back(data);
		m_watchValues.push_back("");

		if (execute)
			UpdateWatchValue(m_watchExprs.size() - 1);
	}
	void DebugInformation::UpdateWatchValue(size_t index)
	{
		char* expr = m_watchExprs[index];

		spvm_result_t resType = nullptr;
		spvm_result_t exprVal = Immediate(std::string(expr), resType);
		
		if (exprVal != nullptr && resType != nullptr) {
			std::stringstream ss;
			GetVariableValueAsString(ss, m_vmImmediate, resType, exprVal->members, exprVal->member_count, "");
			m_watchValues[index] = ss.str();
		} else
			m_watchValues[index] = "ERROR";
	}

	void DebugInformation::Jump(int line)
	{
		while (m_vm->code_current != nullptr && m_vm->current_line != line) {
			StepInto();
			if (CheckBreakpoint(GetCurrentLine()))
				break;
		}
	}
	void DebugInformation::Continue()
	{
		while (m_vm->code_current != nullptr) {
			StepInto();
			if (CheckBreakpoint(GetCurrentLine()))
				break;
		}
	}
	void DebugInformation::Step()
	{
		int old = m_vm->function_stack_current;
		StepInto();
		while (m_vm->code_current != nullptr && m_vm->function_stack_current > old) {
			StepInto();
			if (CheckBreakpoint(GetCurrentLine()))
				break;
		}
	}
	void DebugInformation::StepInto()
	{
		spvm_state_step_into(m_vm);

		if (m_vm->function_stack_current >= m_funcStackLines.size())
			m_funcStackLines.resize(m_vm->function_stack_current + 1);
		
		if (m_vm->function_stack_current >= 0)
			m_funcStackLines[m_vm->function_stack_current] = m_vm->current_line;
	}
	void DebugInformation::StepOut()
	{
		int old = m_vm->function_stack_current;
		while (m_vm->code_current != nullptr && m_vm->function_stack_current >= old) {
			spvm_state_step_opcode(m_vm);
			if (CheckBreakpoint(GetCurrentLine()))
				break;
		}

		if (m_vm->function_stack_current != -1)
			m_vm->current_line = m_funcStackLines[m_vm->function_stack_current];
	}
	bool DebugInformation::CheckBreakpoint(int line)
	{
		bool enabled = false;
		dbg::Breakpoint* brk = GetBreakpoint(m_file, line, enabled);

		if (brk != nullptr && enabled) {
			if (brk->IsConditional && !brk->Condition.empty()) {
				spvm_result_t resType = nullptr;
				spvm_result_t brkResult = Immediate(brk->Condition, resType);

				if (brkResult != nullptr && resType != nullptr && (resType->value_type == spvm_value_type_bool || resType->value_type == spvm_value_type_int) && brkResult->member_count == 1)
					return brkResult->members[0].value.b;
				else {
					m_msgs->Add(MessageStack::Type::Warning, "", "Invalid breakpoint condition on line " + std::to_string(line));
					return false;
				}
			}
			
			return true;
		}

		return false;
	}

	void DebugInformation::ClearPixelList()
	{
		for (PixelInformation& px : m_pixels) {
			for (int i = 0; i < px.VertexCount; i++) {
				auto& outputs = px.VertexShaderOutput[i];
				for (int j = 0; j < outputs.size(); j++)
					if (outputs[j].name != nullptr)
						free(outputs[j].name);
			}
		}
		m_pixels.clear();
	}
		
	void DebugInformation::AddBreakpoint(const std::string& file, int line, const std::string& condition, bool enabled)
	{
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];

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
			dbg::Breakpoint bkpt;
			bkpt.Line = line;
			bkpt.Condition = condition;
			bkpt.IsConditional = !condition.empty();
			bkpts.push_back(bkpt);
			m_breakpointStates[file].push_back(enabled);
		}
	}
	void DebugInformation::RemoveBreakpoint(const std::string& file, int line)
	{
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];
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
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& states = m_breakpointStates[file];

		for (size_t i = 0; i < bkpts.size(); i++) {
			if (bkpts[i].Line == line) {
				states[i] = enable;
				break;
			}
		}
	}
	dbg::Breakpoint* DebugInformation::GetBreakpoint(const std::string& file, int line, bool& enabled)
	{
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& bkptStates = m_breakpointStates[file];
		for (size_t i = 0; i < bkpts.size(); i++)
			if (bkpts[i].Line == line) {
				enabled = bkptStates[i];
				return &bkpts[i];
			}
		enabled = false;
		return nullptr;
	}
}
