#include <SHADERed/GUIManager.h>
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>

#include <glm/gtc/type_ptr.hpp>

namespace ed {
	uint8_t* getRawPixel(GLuint rt, uint8_t* data, int x, int y, int width)
	{
		uint8_t* ret = &data[(x + y * width) * 4];

		if (glGetTextureSubImage) {
			ret = &data[0];
			glGetTextureSubImage(rt, 0, x, y, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 4 * sizeof(uint8_t), data);
		} else {
			glBindTexture(GL_TEXTURE_2D, rt);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		return ret;
	}
	glm::vec4 getPixelColor(GLuint rt, uint8_t* data, int x, int y, int width)
	{
		uint8_t* pxData = getRawPixel(rt, data, x, y, width);
		return glm::vec4(pxData[0] / 255.0f, pxData[1] / 255.0f, pxData[2] / 255.0f, pxData[3] / 255.0f);
	}
	uint32_t getPixelID(GLuint rt, uint8_t* data, int x, int y, int width)
	{
		uint8_t* pxData = getRawPixel(rt, data, x, y, width);
		return ((uint32_t)pxData[0] << 0) | ((uint32_t)pxData[1] << 8) | ((uint32_t)pxData[2] << 16) | ((uint32_t)pxData[3] << 24);
	}
	void copyFloatData(eng::Model::Mesh::Vertex& out, GLfloat* bufData)
	{
		out.Position = glm::vec3(bufData[0], bufData[1], bufData[2]);
		out.Normal = glm::vec3(bufData[3], bufData[4], bufData[5]);
		out.TexCoords = glm::vec2(bufData[6], bufData[7]);
		out.Tangent = glm::vec3(bufData[8], bufData[9], bufData[10]);
		out.Binormal = glm::vec3(bufData[11], bufData[12], bufData[13]);
		out.Color = glm::vec4(bufData[14], bufData[15], bufData[16], bufData[17]);
	}

	InterfaceManager::InterfaceManager(GUIManager* gui)
			: Renderer(&Pipeline, &Objects, &Parser, &Messages, &Plugins, &Debugger)
			, Pipeline(&Parser, &Plugins)
			, Objects(&Parser, &Renderer)
			, Parser(&Pipeline, &Objects, &Renderer, &Plugins, &Messages, &Debugger, gui)
			, Debugger(&Objects, &Renderer, &Messages)
			, Analysis(&Debugger, &Renderer, &Pipeline, &Objects, &Messages)
			, DAP(&Debugger, gui, &Run)
	{
		m_ui = gui;
	}
	InterfaceManager::~InterfaceManager()
	{
		Objects.Clear();
		Plugins.Destroy();
	}
	void InterfaceManager::DebugClick(glm::vec2 r)
	{
		DAP.StopDebugging();
		Debugger.ClearPixelList();

		if (!m_canDebug())
			return;

		// info
		const std::vector<ObjectManagerItem*>& objs = Objects.GetObjects();
		glm::ivec2 previewSize = Renderer.GetLastRenderSize();
		glm::ivec2 maxRTSize = previewSize;
		GLuint previewTexture = Renderer.GetTexture();

		int x = r.x * previewSize.x;
		int y = r.y * previewSize.y;

		// results
		std::unordered_map<GLuint, glm::vec4> pixelColors;
		std::unordered_map<GLuint, std::pair<PipelineItem*, PipelineItem*>> pipelineItems;

		// get max RT size
		for (int i = 0; i < objs.size(); i++) {
			if (objs[i]->RT != nullptr) {
				glm::ivec2 rtSize = Objects.GetRenderTextureSize(objs[i]);
				if (rtSize.x > maxRTSize.x)
					maxRTSize.x = rtSize.x;
				if (rtSize.y > maxRTSize.y)
					maxRTSize.y = rtSize.y;
			}
		}

		uint8_t* mainPixelData = new uint8_t[maxRTSize.x * maxRTSize.y * 4]; // copy gpu data to cpu
		
		// window pixel color
		pixelColors[previewTexture] = getPixelColor(previewTexture, mainPixelData, x, y, previewSize.x);

		// rt pixel colors
		for (int i = 0; i < objs.size(); i++) {
			if (objs[i]->Type == ObjectType::RenderTexture) {
				GLuint tex = objs[i]->Texture;
				glm::ivec2 rtSize = Objects.GetRenderTextureSize(objs[i]);

				pixelColors[tex] = getPixelColor(tex, mainPixelData, r.x * rtSize.x, r.y * rtSize.y, rtSize.x);
			}
		}

		// render with object IDs
		Renderer.Render(true);

		// window pipeline item
		pipelineItems[previewTexture] = Renderer.GetPipelineItemByDebugID(0x00FFFFFF & getPixelID(previewTexture, mainPixelData, x, y, previewSize.x));

		// rt pipeline item
		for (int i = 0; i < objs.size(); i++) {
			if (objs[i]->RT != nullptr) {
				GLuint tex = objs[i]->Texture;
				glm::ivec2 rtSize = Objects.GetRenderTextureSize(objs[i]);

				pipelineItems[tex] = Renderer.GetPipelineItemByDebugID(0x00FFFFFF & getPixelID(tex, mainPixelData, r.x * rtSize.x, r.y * rtSize.y, rtSize.x));
			}
		}

		// add PixelInformation objects
		for (const auto& k : pipelineItems) {
			if (k.second.second == nullptr)
				continue;

			ObjectManagerItem* rtItem = Objects.GetByTextureID(k.first);
			std::string objName = k.second.second->Name;

			glm::ivec2 rtSize = previewSize;
			if (rtItem != nullptr)
				rtSize = Objects.GetRenderTextureSize(rtItem);
			x = r.x * rtSize.x;
			y = r.y * rtSize.y;

			BufferObject* instanceBuffer = nullptr;
			int vertexCount = 3, objTopology = GL_TRIANGLES;
			if (k.second.second->Type == ed::PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* geoData = ((pipe::GeometryItem*)k.second.second->Data);

				objTopology = geoData->Topology;

				int topologySelection = 0;
				for (; topologySelection < (sizeof(TOPOLOGY_ITEM_VALUES) / sizeof(*TOPOLOGY_ITEM_VALUES)); topologySelection++)
					if (TOPOLOGY_ITEM_VALUES[topologySelection] == objTopology)
						break;
				vertexCount = TOPOLOGY_SINGLE_VERTEX_COUNT[topologySelection];

				instanceBuffer = (BufferObject*)geoData->InstanceBuffer;
			} else if (k.second.second->Type == ed::PipelineItem::ItemType::VertexBuffer) {
				pipe::VertexBuffer* bufData = ((pipe::VertexBuffer*)k.second.second->Data);

				objTopology = bufData->Topology;

				int topologySelection = 0;
				for (; topologySelection < (sizeof(TOPOLOGY_ITEM_VALUES) / sizeof(*TOPOLOGY_ITEM_VALUES)); topologySelection++)
					if (TOPOLOGY_ITEM_VALUES[topologySelection] == objTopology)
						break;
				vertexCount = TOPOLOGY_SINGLE_VERTEX_COUNT[topologySelection];
			} else if (k.second.second->Type == ed::PipelineItem::ItemType::Model) {
				pipe::Model* objData = ((pipe::Model*)k.second.second->Data);
				instanceBuffer = (BufferObject*)objData->InstanceBuffer;
			} else if (k.second.second->Type == ed::PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* objData = ((pipe::PluginItemData*)k.second.second->Data);

				objTopology = objData->Owner->PipelineItem_GetTopology(objData->Type, objData->PluginData);
				
				int topologySelection = 0;
				for (; topologySelection < (sizeof(TOPOLOGY_ITEM_VALUES) / sizeof(*TOPOLOGY_ITEM_VALUES)); topologySelection++)
					if (TOPOLOGY_ITEM_VALUES[topologySelection] == objTopology)
						break;
				vertexCount = TOPOLOGY_SINGLE_VERTEX_COUNT[topologySelection];
			}

			int rtIndex = 0;
			if (k.second.first->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)k.second.first->Data;
				for (int i = 0; i < pass->RTCount; i++)
					if (pass->RenderTextures[i] == k.first) {
						rtIndex = i;
						break;
					}
			}

			PixelInformation pxInfo;
			pxInfo.Color = pixelColors[k.first];
			pxInfo.Coordinate = glm::ivec2(x, y);
			pxInfo.InstanceID = 0;
			pxInfo.Object = k.second.second;
			pxInfo.Pass = k.second.first;
			pxInfo.RelativeCoordinate = r;
			pxInfo.RenderTexture = rtItem;
			pxInfo.RenderTextureSize = rtSize;
			pxInfo.RenderTextureIndex = rtIndex;
			pxInfo.VertexID = 0;
			pxInfo.VertexCount = vertexCount;
			pxInfo.InTopology = pxInfo.OutTopology = objTopology;
			pxInfo.InstanceBuffer = instanceBuffer;
			pxInfo.GeometryShaderUsed = false;
			pxInfo.TessellationShaderUsed = false;

			if (pxInfo.Pass && pxInfo.Pass->Type == PipelineItem::ItemType::ShaderPass) {
				pxInfo.GeometryShaderUsed = ((pipe::ShaderPass*)pxInfo.Pass->Data)->GSUsed;
				pxInfo.TessellationShaderUsed = ((pipe::ShaderPass*)pxInfo.Pass->Data)->TSUsed;
			}

			if (Settings::Instance().Debug.AutoFetch)
				FetchPixel(pxInfo);
			Debugger.AddPixel(pxInfo);
		}

		// return old info
		Renderer.Render();
	}
	void InterfaceManager::FetchPixel(PixelInformation& pixel)
	{
		int vertexGroupID = Renderer.DebugVertexPick(pixel.Pass, pixel.Object, pixel.RelativeCoordinate, -1);
		int vertexID = Renderer.DebugVertexPick(pixel.Pass, pixel.Object, pixel.RelativeCoordinate, vertexGroupID);

		int instanceGroupID = Renderer.DebugInstancePick(pixel.Pass, pixel.Object, pixel.RelativeCoordinate, -1);
		int instanceID = Renderer.DebugInstancePick(pixel.Pass, pixel.Object, pixel.RelativeCoordinate, instanceGroupID);

		pixel.InstanceID = instanceID;
		pixel.VertexID = vertexID;
		m_fetchVertices(pixel);

		// return old info
		Renderer.Render(false, pixel.Pass); // render everything up to the pixel.Pass object

		// run vertex shader
		Debugger.PrepareVertexShader(pixel.Pass, pixel.Object);
		for (int i = 0; i < pixel.VertexCount; i++) {
			Debugger.SetVertexShaderInput(pixel, i);
			pixel.VertexShaderPosition[i] = Debugger.ExecuteVertexShader();
			Debugger.CopyVertexShaderOutput(pixel, i);
		}

		memcpy(pixel.FinalPosition, pixel.VertexShaderPosition, sizeof(glm::vec4) * 3);

		// run the tessellation shader if needed
		if (pixel.TessellationShaderUsed) {
			Debugger.PrepareTessellationControlShader(pixel.Pass, pixel.Object);
			Debugger.SetTessellationControlShaderInput(pixel);
			for (int iid = 0; iid < pixel.VertexCount; iid++)
				Debugger.ExecuteTessellationControlShader(iid);
			Debugger.CopyTessellationControlShaderOutput();
		}

		// run the geometry shader if needed
		if (pixel.GeometryShaderUsed) {
			Debugger.PrepareGeometryShader(pixel.Pass, pixel.Object);
			Debugger.SetGeometryShaderInput(pixel);
			Debugger.ExecuteGeometryShader();
		}

		// run pixel shader
		Debugger.PreparePixelShader(pixel.Pass, pixel.Object);
		Debugger.SetPixelShaderInput(pixel);
		pixel.DebuggerColor = Debugger.ExecutePixelShader(pixel.Coordinate.x, pixel.Coordinate.y, pixel.RenderTextureIndex);
		pixel.Discarded = Debugger.GetVM()->discarded;

		pixel.Fetched = true;

		// compute shader suggestion for instanced objects
		if (pixel.InstanceBuffer) {
			BufferObject* buf = (BufferObject*)pixel.InstanceBuffer;
			ObjectManagerItem* bufOwner = Objects.GetByBufferID(buf->ID);
			
			for (auto& item : Pipeline.GetList()) {
				if (Objects.IsUniformBound(bufOwner, item) != -1) {
					if (item->Type == PipelineItem::ItemType::ComputePass) {
						DebuggerSuggestion suggestion;
						suggestion.Type = DebuggerSuggestion::SuggestionType::ComputeShader;
						suggestion.Item = item;
						suggestion.Thread = glm::ivec3(pixel.InstanceID, 0, 0);
						Debugger.AddSuggestion(suggestion);
					}
				}
			}
		}

		// compute shader suggestion for VertexBuffer objects
		if (pixel.Object->Type == PipelineItem::ItemType::VertexBuffer) {
			pipe::VertexBuffer* vertBufferItem = (pipe::VertexBuffer*)pixel.Object->Data;

			BufferObject* buf = (BufferObject*)vertBufferItem->Buffer;

			if (buf) {
				ObjectManagerItem* bufOwner = Objects.GetByBufferID(buf->ID);
			
				for (auto& item : Pipeline.GetList()) {
					if (Objects.IsUniformBound(bufOwner, item) != -1) {
						if (item->Type == PipelineItem::ItemType::ComputePass) {
							DebuggerSuggestion suggestion;
							suggestion.Type = DebuggerSuggestion::SuggestionType::ComputeShader;
							suggestion.Item = item;

							// vertex #1
							suggestion.Thread = glm::ivec3(pixel.VertexID + 0, 0, 0);
							Debugger.AddSuggestion(suggestion);

							// vertex #2
							suggestion.Thread = glm::ivec3(pixel.VertexID + 1, 0, 0);
							Debugger.AddSuggestion(suggestion);

							// vertex #3
							suggestion.Thread = glm::ivec3(pixel.VertexID + 2, 0, 0);
							Debugger.AddSuggestion(suggestion);
						}
					}
				}
			}
		}
	}
	void InterfaceManager::m_fetchVertices(PixelInformation& pixel)
	{
		if (pixel.Object->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem::GeometryType geoType = ((pipe::GeometryItem*)pixel.Object->Data)->Type;
			GLuint vbo = ((pipe::GeometryItem*)pixel.Object->Data)->VBO;

			// TODO: don't bother GPU so much
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			if (geoType == pipe::GeometryItem::GeometryType::ScreenQuadNDC) {
				GLfloat bufData[6 * 4] = { 0.0f };
				glGetBufferSubData(GL_ARRAY_BUFFER, 0, 6 * 4 * sizeof(float), &bufData[0]);

				for (int i = 0; i < pixel.VertexCount; i++) {
					int actualIndex = pixel.VertexID + i;
				
					pixel.Vertex[i].Position = glm::vec3(bufData[actualIndex * 4 + 0], bufData[actualIndex * 4 + 1], 0.0f);
					pixel.Vertex[i].TexCoords = glm::vec2(bufData[actualIndex * 4 + 2], bufData[actualIndex * 4 + 3]);
				}
			} else {
				GLfloat bufData[3 * 18] = { 0.0f };
				glGetBufferSubData(GL_ARRAY_BUFFER, pixel.VertexID * 18 * sizeof(float), pixel.VertexCount * 18 * sizeof(float), &bufData[0]);

				copyFloatData(pixel.Vertex[0], &bufData[0]);
				copyFloatData(pixel.Vertex[1], &bufData[18]);
				copyFloatData(pixel.Vertex[2], &bufData[36]);
			}
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		} 
		else if (pixel.Object->Type == PipelineItem::ItemType::Model) {
			pipe::Model* mdl = ((pipe::Model*)pixel.Object->Data);

			int curOffset = 0;
			for (const auto& mesh : mdl->Data->Meshes) {
				if (pixel.VertexID >= curOffset + mesh.Indices.size()) {
					curOffset += mesh.Indices.size();
					continue;
				}

				pixel.Vertex[0] = mesh.Vertices[mesh.Indices[pixel.VertexID + 0]];
				pixel.Vertex[1] = mesh.Vertices[mesh.Indices[pixel.VertexID + 1]];
				pixel.Vertex[2] = mesh.Vertices[mesh.Indices[pixel.VertexID + 2]];
			}
		} 
		else if (pixel.Object->Type == PipelineItem::ItemType::VertexBuffer) {
			pipe::VertexBuffer* vBuffer = ((pipe::VertexBuffer*)pixel.Object->Data);
			ed::BufferObject* bufData = (ed::BufferObject*)vBuffer->Buffer;

			std::vector<ShaderVariable::ValueType> tData = Objects.ParseBufferFormat(bufData->ViewFormat);
			
			int stride = 0;
			for (const auto& dataEl : tData)
				stride += ShaderVariable::GetSize(dataEl, true); 
			
			GLfloat* bufPtr = (GLfloat*)malloc(pixel.VertexCount * stride);

			glBindBuffer(GL_ARRAY_BUFFER, bufData->ID);
			glGetBufferSubData(GL_ARRAY_BUFFER, pixel.VertexID * stride, pixel.VertexCount * stride, &bufPtr[0]);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			for (int i = 0; i < pixel.VertexCount; i++) {
				int iOffset = 0;
				for (int j = 0; j < tData.size(); j++) {

					if (j == 0) /* POSITION */
						pixel.Vertex[i].Position = glm::make_vec3(bufPtr + i * stride / 4 + iOffset);
					else if (j == 1)
						pixel.Vertex[i].Normal = glm::make_vec3(bufPtr + i * stride / 4 + iOffset);
					else if (j == 2)
						pixel.Vertex[i].TexCoords = glm::make_vec2(bufPtr + i * stride / 4 + iOffset);
					
					// TODO: use input layout

					iOffset += ShaderVariable::GetSize(tData[j]) / 4;
				}
			}

			free(bufPtr);
		} 
		else if (pixel.Object->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* pdata = (pipe::PluginItemData*)pixel.Object->Data;
			
			GLuint vbo = pdata->Owner->PipelineItem_GetVBO(pdata->Type, pdata->PluginData);
			GLuint vboStride = pdata->Owner->PipelineItem_GetVBOStride(pdata->Type, pdata->PluginData);
			int inpLayoutSize = pdata->Owner->PipelineItem_GetInputLayoutSize(pdata->Type, pdata->PluginData);
			
			// TODO: don't bother GPU so much
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			for (int i = 0; i < pixel.VertexCount; i++) {
				GLfloat bufData[18] = { 0.0f };
				glGetBufferSubData(GL_ARRAY_BUFFER, (pixel.VertexID + i) * vboStride * sizeof(float), vboStride * sizeof(float), &bufData[0]);

				int bufIndex = 0;
				for (int j = 0; j < inpLayoutSize; j++) {
					plugin::InputLayoutItem layItem;
					pdata->Owner->PipelineItem_GetInputLayoutItem(pdata->Type, pdata->PluginData, j, layItem);
					int valSize = InputLayoutItem::GetValueSize((InputLayoutValue)layItem.Value);

					for (int k = 0; k < valSize; k++) {
						if (layItem.Value == plugin::InputLayoutValue::Position)
							pixel.Vertex[i].Position[k] = bufData[bufIndex + k];
						else if (layItem.Value == plugin::InputLayoutValue::Texcoord)
							pixel.Vertex[i].TexCoords[k] = bufData[bufIndex + k];
						else if (layItem.Value == plugin::InputLayoutValue::Color)
							pixel.Vertex[i].Color[k] = bufData[bufIndex + k];
						else if (layItem.Value == plugin::InputLayoutValue::Binormal)
							pixel.Vertex[i].Binormal[k] = bufData[bufIndex + k];
						else if (layItem.Value == plugin::InputLayoutValue::Normal)
							pixel.Vertex[i].Normal[k] = bufData[bufIndex + k];
						else if (layItem.Value == plugin::InputLayoutValue::Tangent)
							pixel.Vertex[i].Tangent[k] = bufData[bufIndex + k];
					}

					bufIndex += valSize;
				}
			}
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		} 
	}
	void InterfaceManager::OnEvent(const SDL_Event& e)
	{
		Objects.OnEvent(e);
	}
	void InterfaceManager::Update(float delta)
	{
	}
	bool InterfaceManager::m_canDebug()
	{
		bool ret = true;
		
		for (const auto& i : Pipeline.GetList()) {
			if (i->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)i->Data;
				int langID = -1;

				IPlugin1* plugin = ed::ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->VSPath, Plugins.Plugins());
				ret &= (plugin != nullptr && plugin->CustomLanguage_IsDebuggable(langID)) || plugin == nullptr;

				plugin = ed::ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->PSPath, Plugins.Plugins());
				ret &= (plugin != nullptr && plugin->CustomLanguage_IsDebuggable(langID)) || plugin == nullptr;

				if (pass->GSUsed) {
					plugin = ed::ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->GSPath, Plugins.Plugins());
					ret &= (plugin != nullptr && plugin->CustomLanguage_IsDebuggable(langID)) || plugin == nullptr;
				}

				if (pass->TSUsed) {
					plugin = ed::ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->TCSPath, Plugins.Plugins());
					ret &= (plugin != nullptr && plugin->CustomLanguage_IsDebuggable(langID)) || plugin == nullptr;	
				
					plugin = ed::ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->TESPath, Plugins.Plugins());
					ret &= (plugin != nullptr && plugin->CustomLanguage_IsDebuggable(langID)) || plugin == nullptr;
				}
			} else if (i->Type == PipelineItem::ItemType::ComputePass) {
				pipe::ComputePass* pass = (pipe::ComputePass*)i->Data;
				int langID = -1;

				IPlugin1* plugin = ed::ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->Path, Plugins.Plugins());
				ret &= (plugin != nullptr && plugin->CustomLanguage_IsDebuggable(langID)) || plugin == nullptr;
			}
		}

		return ret;
	}
}